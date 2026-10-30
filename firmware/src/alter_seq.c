/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's sequencer: three euclidean circles playing one sound.
 *
 *   HITS    the main hits
 *   GHOST   soft hits on the steps HITS leaves empty
 *   ACCENT  makes a HITS or GHOST hit on its step loud; alone on an empty step it plays nothing
 *
 * Each circle: LEN steps (1..32), HITS of them spread evenly (euclidean), ROT turns the pattern, PROB the
 * chance (0..127, 127 = always) that a hit plays. A change (as_set) is held until the circle's next cycle
 * starts, so the pattern never breaks in the middle.
 *
 * The die (as_roll): a circle with its die on gets, per step, a pitch (scale degrees), a velocity offset, a
 * glide and four timbre offsets (MRPH SPRD HARM AMNT), drawn at the roll and kept until the next roll; the
 * new draw too waits for the circle's next cycle. DOSE (0..127) is how far they reach: low for small live
 * variations, high for sound design. A lock (AS_LK_*) keeps that parameter out of every draw.
 * ACCENT's die moves the accent's strength only.
 *
 * Integers only, no allocation: as_step() runs once per sixteenth, in the audio interrupt if the host wants. */

#define AS_NCIRC 3
#define AS_MAXLEN 32
enum { AS_HITS, AS_GHOST, AS_ACCENT };
enum { AS_MRPH, AS_SPRD, AS_HARM, AS_AMNT, AS_NTIMB };
enum {
    AS_LK_PITCH = 1, AS_LK_VEL = 2, AS_LK_GLIDE = 4,
    AS_LK_MRPH = 8, AS_LK_SPRD = 16, AS_LK_HARM = 32, AS_LK_AMNT = 64,
};
enum { AS_SC_OCT, AS_SC_5TH, AS_SC_MPEN, AS_SC_MIN, AS_SC_PHRY, AS_SC_DOR, AS_SC_CHR, AS_NSCALE };
static const char *const AS_SCALE_NAME[AS_NSCALE] = {"OCT", "5TH", "MPEN", "MIN", "PHRY", "DOR", "CHR"};

typedef struct {
    uint8_t len, hits, rot, prob;   /* 1..32, 0..len, 0..len-1, 0..127 */
    uint8_t die, dose;              /* die 0/1, dose 0..127 */
    uint8_t mute;
} as_cfg_t;

typedef struct {
    as_cfg_t cfg;                   /* playing */
    as_cfg_t next;                  /* asked for: from the next cycle */
    uint32_t pat;                   /* bit i: step i hits */
    uint8_t pos;                    /* the step about to play */
    uint8_t roll_wait;              /* a roll asked for: drawn at the next cycle */
    int8_t pitch[AS_MAXLEN];        /* the draw: scale degrees above the root */
    int8_t vel[AS_MAXLEN];          /* velocity offset */
    uint8_t glide[AS_MAXLEN];       /* 1 = slide into this step's note */
    int8_t timb[AS_MAXLEN][AS_NTIMB];
    uint32_t seed;                  /* this circle's draws */
} as_circ_t;

typedef struct {
    as_circ_t c[AS_NCIRC];
    uint8_t root;                   /* MIDI note */
    uint8_t scale;                  /* AS_SC_* */
    uint8_t locks;                  /* AS_LK_* */
    uint32_t rng;                   /* PROB's coin */
} as_seq_t;

typedef struct {
    uint8_t on;                     /* 1 = a hit this step */
    uint8_t note, vel;              /* 1..127 */
    uint8_t glide;                  /* slide from the last note */
    uint8_t ghost, accent;          /* what played it (for the host and the screen) */
    int8_t timb[AS_NTIMB];          /* offsets to add to MRPH SPRD HARM AMNT */
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

/* a signed value in -r..r, its spread narrowed toward 0 (the sum of two draws: small offsets are common) */
static int32_t as_spread(uint32_t *s, int32_t r)
{
    int32_t a, b;
    if (r <= 0)
        return 0;
    a = (int32_t)(as_rand(s) % (uint32_t)(2 * r + 1)) - r;
    b = (int32_t)(as_rand(s) % (uint32_t)(2 * r + 1)) - r;
    return (a + b) / 2;
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
    if (c.prob > 127)
        c.prob = 127;
    if (c.dose > 127)
        c.dose = 127;
    c.die = c.die ? 1 : 0;
    c.mute = c.mute ? 1 : 0;
    return c;
}

/* a new draw of circle i, over all 32 steps (a longer LEN later finds them ready) */
static void as_draw(as_seq_t *q, uint32_t i)
{
    as_circ_t *c = &q->c[i];
    int32_t d = c->next.dose, k, j;
    uint32_t *s = &c->seed;
    for (k = 0; k < AS_MAXLEN; k++) {
        /* pitch: stays on the root with a chance falling as DOSE rises; else up to 2 octaves of degrees */
        c->pitch[k] = 0;
        if (!(q->locks & AS_LK_PITCH) && (int32_t)(as_rand(s) % 128u) < d)
            c->pitch[k] = (int8_t)(as_rand(s) % (uint32_t)(1 + d * 14 / 127));
        c->vel[k] = (int8_t)(q->locks & AS_LK_VEL ? 0 : as_spread(s, d * 40 / 127));
        c->glide[k] = (uint8_t)(!(q->locks & AS_LK_GLIDE) && (int32_t)(as_rand(s) % 256u) < d);
        for (j = 0; j < AS_NTIMB; j++)
            c->timb[k][j] = (int8_t)(q->locks & (AS_LK_MRPH << j) ? 0 : as_spread(s, d / 2));
    }
}

static void as_init(as_seq_t *q, uint32_t seed)
{
    static const as_cfg_t DEF[AS_NCIRC] = {
        {16, 4, 0, 127, 0, 30, 0},
        {16, 3, 2, 127, 0, 30, 0},
        {16, 2, 0, 127, 0, 30, 0},
    };
    uint32_t i;
    for (i = 0; i < sizeof *q; i++)
        ((uint8_t *)q)[i] = 0;
    q->root = 36;
    q->scale = AS_SC_MIN;
    q->rng = seed | 1u;
    for (i = 0; i < AS_NCIRC; i++) {
        as_circ_t *c = &q->c[i];
        c->cfg = c->next = DEF[i];
        c->pat = as_euclid(c->cfg.len, c->cfg.hits, c->cfg.rot);
        c->seed = seed * 2654435761u + i * 40503u + 1u;
        as_draw(q, i);
    }
}

/* ask for circle i's new settings: they play from its next cycle */
static void as_set(as_seq_t *q, uint32_t i, as_cfg_t cfg)
{
    if (i < AS_NCIRC)
        q->c[i].next = as_clean(cfg);
}

/* the die of circle i: a new draw, played from its next cycle and kept until the next roll */
static void as_roll(as_seq_t *q, uint32_t i)
{
    if (i < AS_NCIRC)
        q->c[i].roll_wait = 1;
}

/* a scale degree above the root as a note, carried into the octaves above */
static uint32_t as_note(const as_seq_t *q, int32_t deg)
{
    const int8_t *sc = AS_SCALES[q->scale < AS_NSCALE ? q->scale : AS_SC_CHR];
    int32_t n = 0, oct, note;
    while (sc[n] >= 0)
        n++;
    oct = deg / n;
    note = q->root + oct * 12 + sc[deg % n];
    return (uint32_t)(note < 1 ? 1 : note > 127 ? 127 : note);
}

/* one sixteenth: what plays (ev->on) and the circles move on */
static void as_step(as_seq_t *q, as_event_t *ev)
{
    uint32_t i, on[AS_NCIRC], at[AS_NCIRC];
    for (i = 0; i < AS_NCIRC; i++) {
        as_circ_t *c = &q->c[i];
        if (c->pos == 0) {                              /* a new cycle: what was asked for starts now */
            if (c->roll_wait) {
                as_draw(q, i);
                c->roll_wait = 0;
            }
            c->cfg = c->next;
            c->pat = as_euclid(c->cfg.len, c->cfg.hits, c->cfg.rot);
        }
        at[i] = c->pos;
        on[i] = !c->cfg.mute && (c->pat >> c->pos & 1u) &&
                (c->cfg.prob >= 127 || (int32_t)(as_rand(&q->rng) % 127u) < c->cfg.prob);
        c->pos = (uint8_t)((c->pos + 1u) % c->cfg.len);
    }
    for (i = 0; i < sizeof *ev; i++)
        ((uint8_t *)ev)[i] = 0;
    if (!on[AS_HITS] && !on[AS_GHOST])
        return;
    {
        uint32_t src = on[AS_HITS] ? AS_HITS : AS_GHOST, k = at[src], j;
        const as_circ_t *c = &q->c[src];
        int32_t vel = src == AS_HITS ? 96 : 40, deg = 0;
        ev->on = 1;
        ev->ghost = (uint8_t)(src == AS_GHOST);
        if (on[AS_ACCENT]) {
            const as_circ_t *a = &q->c[AS_ACCENT];
            ev->accent = 1;
            vel = 124 + (a->cfg.die ? a->vel[at[AS_ACCENT]] / 2 : 0);   /* the accent's own die: its strength */
            if (ev->ghost)
                vel -= 30;                              /* an accented ghost: firm, still under a hit */
        }
        if (c->cfg.die) {
            deg = c->pitch[k];
            vel += c->vel[k];
            ev->glide = c->glide[k];
            for (j = 0; j < AS_NTIMB; j++)
                ev->timb[j] = c->timb[k][j];
        }
        ev->note = (uint8_t)as_note(q, deg);
        ev->vel = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel);
    }
}
