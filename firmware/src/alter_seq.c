/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's sequencer: three euclidean circles, three independent voices (each its own part and sound).
 *
 * Each circle: LEN steps (1..32), HITS of them spread evenly (euclidean), ROT turns the pattern, PROB the
 * chance (0..127, 127 = always) that a hit plays. A change (as_set) is held until the circle's next cycle
 * starts, so the pattern never breaks in the middle. MUTE is at once.
 *
 * Per step (the SEQ tab edits them, step 3d-2): a FORCE (0 ghost, 1 normal, 2 accent; its own rotation FROT,
 * independent of the hits) and a CHANCE (0..127). A step plays when it is a hit, the PROB coin and its CHANCE
 * coin both say so.
 *
 * The die (as_roll): AT ONCE a new draw for the circle, drastic at the default DOSE. The rhythm: HITS and ROT
 * moved (LEN kept), steps that skip (a lowered chance), bursts (1..3 repeats in the step) and flams (one quick
 * repeat). Per hit: a force move, a pitch (scale degrees above the circle's ROOT, up to 3 octaves: the scale itself
 * never changes), a velocity spread, a glide, a length (1..4 steps: it rings on, or the next hit cuts it), an
 * envelope offset (the host moves decay and release) and four timbre offsets (the host maps them to its engine's
 * parameters). Kept until the next roll; as_clear forgets it (the clean pattern). DOSE (0..127) is how far a draw
 * reaches: low for small variations, high for sound design. A circle's lock (AS_LK_*) keeps that part out of its draws.
 * DENSITY (as_density): +-1 HITS at once and kept (the keys are quantised to the grid: they act at the next step).
 *
 * Integers only, no allocation: as_step() runs once per sixteenth, in the audio interrupt if the host wants. */

#define AS_NCIRC 3
#define AS_MAXLEN 32
#define AS_NTIMB 4
enum { AS_GHOST, AS_NORMAL, AS_ACCENT };
enum { AS_LK_PITCH = 1, AS_LK_VEL = 2, AS_LK_GLIDE = 4, AS_LK_FORCE = 8, AS_LK_TIMB = 16, AS_LK_RHYTHM = 32,
       AS_LK_ENV = 64 };
#define AS_DOSE_DEF 100
enum { AS_SC_OCT, AS_SC_5TH, AS_SC_MPEN, AS_SC_MIN, AS_SC_PHRY, AS_SC_DOR, AS_SC_CHR, AS_NSCALE };
static const char *const AS_SCALE_NAME[AS_NSCALE] = {"OCT", "5TH", "MPEN", "MIN", "PHRY", "DOR", "CHR"};
static const uint8_t AS_VEL[3] = {40, 96, 124};  /* ghost, normal, accent */

typedef struct {
    uint8_t len, hits, rot, prob;   /* 1..32, 0..len, 0..len-1, 0..127 */
    uint8_t frot;                   /* the forces' own rotation, 0..len-1 */
    uint8_t dose, mute, root;       /* 0..127, 0/1, the MIDI note of degree 0 */
} as_cfg_t;

typedef struct {
    as_cfg_t cfg;                   /* playing */
    as_cfg_t next;                  /* asked for: from the next cycle */
    uint32_t pat;                   /* bit i: step i hits */
    uint8_t pos;                    /* the step about to play */
    uint8_t die;                    /* 1: a draw is on */
    uint8_t force[AS_MAXLEN];       /* AS_GHOST .. AS_ACCENT, before FROT */
    uint8_t chance[AS_MAXLEN];      /* 0..127 */
    int8_t dforce[AS_MAXLEN];       /* the draw: -1 0 +1 */
    int8_t pitch[AS_MAXLEN];        /* scale degrees above the root */
    int8_t vel[AS_MAXLEN];          /* velocity offset */
    uint8_t glide[AS_MAXLEN];       /* 1 = slide into this step's note */
    int8_t timb[AS_MAXLEN][AS_NTIMB];
    uint8_t rat[AS_MAXLEN];         /* repeats in the step: 0..3 */
    uint8_t flam[AS_MAXLEN];        /* 1 = one quick repeat */
    uint8_t dchance[AS_MAXLEN];     /* 127, or lower: the step skips now and then */
    uint8_t gate[AS_MAXLEN];        /* the note's length in steps, 1..4 */
    int8_t env[AS_MAXLEN];          /* decay / release offset */
    int8_t dhits;                   /* HITS moved by .. */
    uint8_t drot;                   /* .. and ROT turned by (0..31) */
    uint8_t locks;                  /* AS_LK_*: what this circle's draws leave alone */
    uint32_t seed;                  /* this circle's draws */
} as_circ_t;

typedef struct {
    as_circ_t c[AS_NCIRC];
    uint8_t scale;                  /* AS_SC_*, shared */
    uint32_t rng;                   /* the PROB and CHANCE coins */
} as_seq_t;

typedef struct {
    uint8_t on;                     /* 1 = a hit this step */
    uint8_t note, vel;              /* 1..127 */
    uint8_t glide;                  /* slide from the last note */
    uint8_t force;                  /* AS_GHOST .. AS_ACCENT (for the host and the screen) */
    int8_t timb[AS_NTIMB];          /* offsets for the host's four timbre parameters */
    uint8_t rat, flam;              /* repeats in the step (0..3), a quick repeat */
    uint8_t gate;                   /* the note's length in steps (1..4) */
    int8_t env;                     /* decay / release offset */
} as_event_t;

/* the scales as semitones of each degree, ended by -1 */
static const int8_t AS_SCALES[AS_NSCALE][13] = {
    {0, -1},
    {0, 7, -1},
    {0, 3, 5, 7, 10, -1},
    {0, 2, 3, 5, 7, 8, 10, -1},
    {0, 1, 3, 5, 7, 8, 10, -1},
    {0, 2, 3, 5, 7, 9, 10, -1},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -1},
};

static uint32_t as_rand(uint32_t *s)                     /* xorshift32 */
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/* the euclidean pattern: k hits on n steps, turned by r (step 0 is a hit at r = 0) */
static uint32_t as_euclid(uint32_t n, uint32_t k, uint32_t r)
{
    uint32_t i, p = 0;
    if (!n)
        return 0;
    if (k > n)
        k = n;
    for (i = 0; i < n; i++)
        if ((i * k) % n < k)
            p |= 1u << ((i + r) % n);
    return p;
}

static as_cfg_t as_clean(as_cfg_t c)
{
    if (c.len < 1)
        c.len = 1;
    if (c.len > AS_MAXLEN)
        c.len = AS_MAXLEN;
    if (c.hits > c.len)
        c.hits = c.len;
    c.rot %= c.len;
    c.frot %= c.len;
    if (c.prob > 127)
        c.prob = 127;
    if (c.dose > 127)
        c.dose = 127;
    if (c.root < 1)
        c.root = 1;
    if (c.root > 120)
        c.root = 120;
    c.mute = c.mute ? 1 : 0;
    return c;
}

/* a uniform value in -r..r */
static int32_t as_uni(uint32_t *s, int32_t r)
{
    return r <= 0 ? 0 : (int32_t)(as_rand(s) % (uint32_t)(2 * r + 1)) - r;
}

/* 1 with a chance of n / 1024 */
static int as_coin(uint32_t *s, int32_t n) { return (int32_t)(as_rand(s) % 1024u) < n; }

/* a new draw of circle i, over all 32 steps (a longer LEN later finds them ready) */
static void as_draw(as_seq_t *q, uint32_t i)
{
    as_circ_t *c = &q->c[i];
    int32_t d = c->next.dose, k, j, lk = c->locks;
    uint32_t *s = &c->seed;
    /* the rhythm: HITS moved by up to half the circle, ROT anywhere (each with a chance rising with DOSE) */
    c->dhits = 0;
    c->drot = 0;
    if (!(lk & AS_LK_RHYTHM)) {
        if (as_coin(s, d * 8))
            c->dhits = (int8_t)as_uni(s, 1 + d * 7 / 127);
        if (as_coin(s, d * 7))
            c->drot = (uint8_t)(as_rand(s) % AS_MAXLEN);
    }
    for (k = 0; k < AS_MAXLEN; k++) {
        c->dforce[k] = 0;
        if (!(lk & AS_LK_FORCE) && as_coin(s, 120 + d * 6))
            c->dforce[k] = (int8_t)(as_rand(s) & 1u ? 1 : -1);
        c->pitch[k] = 0;                                /* up to 3 octaves of degrees, the root less often */
        if (!(lk & AS_LK_PITCH) && as_coin(s, d * 7))
            c->pitch[k] = (int8_t)(as_rand(s) % (uint32_t)(1 + d * 21 / 127));
        c->vel[k] = (int8_t)(lk & AS_LK_VEL ? 0 : as_uni(s, 4 + d * 20 / 127));
        c->glide[k] = (uint8_t)(!(lk & AS_LK_GLIDE) && as_coin(s, d * 3));
        for (j = 0; j < AS_NTIMB; j++)
            c->timb[k][j] = (int8_t)(lk & AS_LK_TIMB ? 0 : as_uni(s, d * 5 / 8));
        c->rat[k] = 0;
        c->flam[k] = 0;
        c->dchance[k] = 127;
        if (!(lk & AS_LK_RHYTHM)) {
            if (as_coin(s, d * 2))
                c->rat[k] = (uint8_t)(1u + as_rand(s) % 3u);
            else if (as_coin(s, d * 2))
                c->flam[k] = 1;
            if (as_coin(s, d * 2))
                c->dchance[k] = (uint8_t)(25u + as_rand(s) % 70u);
        }
        c->gate[k] = 1;
        c->env[k] = 0;
        if (!(lk & AS_LK_ENV)) {
            if (as_coin(s, d * 3))
                c->gate[k] = (uint8_t)(2u + as_rand(s) % 3u);
            c->env[k] = (int8_t)as_uni(s, d * 50 / 127);
        }
    }
}

/* circle c's pattern as it plays: its settings, the draw's HITS and ROT on top */
static void as_repat(as_circ_t *c)
{
    int32_t h = c->cfg.hits, r = c->cfg.rot;
    if (c->die) {
        h = h + c->dhits;
        h = h < 0 ? 0 : h > c->cfg.len ? c->cfg.len : h;
        if (c->cfg.hits && !h)
            h = 1;                                      /* (a circle that plays keeps a hit) */
        r += c->drot;
    }
    c->pat = as_euclid(c->cfg.len, (uint32_t)h, (uint32_t)r % c->cfg.len);
}

static void as_init(as_seq_t *q, uint32_t seed)
{
    static const as_cfg_t DEF[AS_NCIRC] = {
        /* len hits rot prob frot dose mute root */
        {16, 4, 0, 127, 0, AS_DOSE_DEF, 0, 36},   /* ALTER: C2 */
        {16, 3, 2, 127, 0, AS_DOSE_DEF, 0, 48},   /* PHYS: C3 */
        {16, 5, 1, 127, 0, AS_DOSE_DEF, 0, 60},   /* NOISE: C4 */
    };
    uint32_t i, k;
    for (i = 0; i < sizeof *q; i++)
        ((uint8_t *)q)[i] = 0;
    q->scale = AS_SC_MIN;
    q->rng = seed | 1u;
    for (i = 0; i < AS_NCIRC; i++) {
        as_circ_t *c = &q->c[i];
        c->cfg = c->next = DEF[i];
        as_repat(c);
        c->seed = seed * 2654435761u + i * 40503u + 1u;
        for (k = 0; k < AS_MAXLEN; k++) {
            c->force[k] = AS_NORMAL;
            c->chance[k] = 127;
        }
    }
}

/* ask for circle i's new settings: they play from its next cycle (MUTE and ROOT at once) */
static void as_set(as_seq_t *q, uint32_t i, as_cfg_t cfg)
{
    if (i < AS_NCIRC) {
        as_circ_t *c = &q->c[i];
        c->next = as_clean(cfg);
        c->cfg.mute = c->next.mute;
        c->cfg.root = c->next.root;
    }
}

/* turn circle i's pattern by d steps at once (SHIFT), kept: the waiting settings too */
static void as_turn(as_seq_t *q, uint32_t i, int32_t d)
{
    as_circ_t *c;
    if (i >= AS_NCIRC)
        return;
    c = &q->c[i];
    c->cfg.rot = (uint8_t)(((int32_t)c->cfg.rot + d + 4 * AS_MAXLEN) % c->cfg.len);
    c->next.rot = (uint8_t)(((int32_t)c->next.rot + d + 4 * AS_MAXLEN) % c->next.len);
    as_repat(c);
}

/* DENSITY: circle i's HITS +-d at once, kept (the waiting settings too) */
static void as_density(as_seq_t *q, uint32_t i, int32_t d)
{
    as_circ_t *c;
    int32_t h;
    if (i >= AS_NCIRC)
        return;
    c = &q->c[i];
    h = (int32_t)c->cfg.hits + d;
    c->cfg.hits = (uint8_t)(h < 0 ? 0 : h > c->cfg.len ? c->cfg.len : h);
    h = (int32_t)c->next.hits + d;
    c->next.hits = (uint8_t)(h < 0 ? 0 : h > c->next.len ? c->next.len : h);
    as_repat(c);
}

/* the die of circle i: a new draw, at once, kept until the next roll */
static void as_roll(as_seq_t *q, uint32_t i)
{
    if (i < AS_NCIRC) {
        as_draw(q, i);
        q->c[i].die = 1;
        as_repat(&q->c[i]);
    }
}

/* the clean pattern again: the draw forgotten */
static void as_clear(as_seq_t *q, uint32_t i)
{
    if (i < AS_NCIRC) {
        q->c[i].die = 0;
        as_repat(&q->c[i]);
    }
}

/* a scale degree above a root as a note, carried into the octaves above */
static uint32_t as_note(const as_seq_t *q, uint32_t root, int32_t deg)
{
    const int8_t *sc = AS_SCALES[q->scale < AS_NSCALE ? q->scale : AS_SC_CHR];
    int32_t n = 0, oct, note;
    while (sc[n] >= 0)
        n++;
    oct = deg / n;
    note = (int32_t)root + oct * 12 + sc[deg % n];
    return (uint32_t)(note < 1 ? 1 : note > 127 ? 127 : note);
}

/* the force a step of circle c plays with (its FROT, the draw) */
static uint32_t as_force(const as_circ_t *c, uint32_t k)
{
    uint32_t len = c->cfg.len ? c->cfg.len : 1u;
    int32_t f = c->force[(k + len - c->cfg.frot % len) % len];
    if (c->die)
        f += c->dforce[k];
    return (uint32_t)(f < AS_GHOST ? AS_GHOST : f > AS_ACCENT ? AS_ACCENT : f);
}

/* one sixteenth: what each circle plays (ev[i].on), and the circles move on */
static void as_step(as_seq_t *q, as_event_t ev[AS_NCIRC])
{
    uint32_t i, j;
    for (i = 0; i < AS_NCIRC; i++) {
        as_circ_t *c = &q->c[i];
        as_event_t *e = &ev[i];
        uint32_t k, on;
        if (c->pos == 0) {                              /* a new cycle: what was asked for starts now */
            c->cfg = c->next;
            as_repat(c);
        }
        k = c->pos;
        {
            uint32_t ch = c->chance[k];
            if (c->die && c->dchance[k] < ch)
                ch = c->dchance[k];
            on = !c->cfg.mute && (c->pat >> k & 1u) &&
                 (c->cfg.prob >= 127 || (int32_t)(as_rand(&q->rng) % 127u) < c->cfg.prob) &&
                 (ch >= 127 || as_rand(&q->rng) % 127u < ch);
        }
        c->pos = (uint8_t)((c->pos + 1u) % c->cfg.len);
        for (j = 0; j < sizeof *e; j++)
            ((uint8_t *)e)[j] = 0;
        if (!on)
            continue;
        {
            uint32_t f = as_force(c, k);
            int32_t vel = AS_VEL[f], deg = 0;
            e->on = 1;
            e->force = (uint8_t)f;
            e->gate = 1;
            if (c->die) {
                e->rat = c->rat[k];
                e->flam = c->flam[k];
                e->gate = c->gate[k];
                e->env = c->env[k];
                deg = c->pitch[k];
                vel += c->vel[k];
                e->glide = c->glide[k];
                for (j = 0; j < AS_NTIMB; j++)
                    e->timb[j] = c->timb[k][j];
            }
            e->note = (uint8_t)as_note(q, c->cfg.root, deg);
            e->vel = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel);
        }
    }
}
