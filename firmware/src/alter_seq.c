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
 * CHORD (as_cfg_t.chord): each hit plays up to two more notes (3 voices), moved by the die's pitch with it:
 *   OCT  the note an octave up · 5TH  a power chord (+7, +12 semitones, whatever the scale)
 *   TRIAD  the scale's third and fifth above the note · 7TH  third and seventh (no fifth: a house stab)
 *   9TH  rootless: third, seventh and ninth (the hit's note itself is left out; the bass plays it)
 *   TRIAD 7TH 9TH follow the scale (each tone: the scale note nearest that interval, so a 5-note scale works too).
 * DENSITY (as_density): +-1 HITS at once and kept (the keys are quantised to the grid: they act at the next step).
 *
 * Integers only, no allocation: as_step() runs once per sixteenth, in the audio interrupt if the host wants. */

#define AS_NCIRC 3
#define AS_MAXLEN 32
#define AS_PLAYLEN 16                     /* a circle plays 16 steps at most (Romain 04/11: readability; the arrays keep 32)
                                          * a scene stored longer is brought back to 16 (as_clean) */
#define AS_NTIMB 4
enum { AS_GHOST, AS_NORMAL, AS_ACCENT };
/* (10/11, v0.48, Romain: the locks follow the dice. Bits kept where the meaning stayed: PITCH 1, VEL 2, RHYTHM 32; old
 * GLIDE 4 / FORCE 8 / ENV 64 saved in scenes now read as LEN / CHANCE / THROWS, old TIMB 16 as SOUND) */
enum { AS_LK_PITCH = 1, AS_LK_VEL = 2, AS_LK_LEN = 4, AS_LK_CHANCE = 8, AS_LK_SOUND = 16, AS_LK_RHYTHM = 32,
       AS_LK_THROW = 64 };
#define AS_DOSE_DEF 100
enum { AS_SC_OCT, AS_SC_5TH, AS_SC_MPEN, AS_SC_MIN, AS_SC_PHRY, AS_SC_DOR, AS_SC_CHR, AS_NSCALE };
static const char *const AS_SCALE_NAME[AS_NSCALE] = {"OCT", "5TH", "MPEN", "MIN", "PHRY", "DOR", "CHR"};
static const uint8_t AS_VEL[3] = {40, 96, 124};  /* ghost, normal, accent (the VELO lane's three marks) */
/* the rhythm engine a circle runs (as_cfg_t.type): EUCLID HITS spread evenly; TURING a looping random pattern that
 * mutates as each cycle starts (LOCK 127 frozen, 0 new every time; DENS how many hits the new bits bring; RANGE how far
 * its pitch walks, in scale degrees). Live: as_tnext (a new pattern at once), as_tprev (the one before: two kept),
 * as_circ_t.tfreeze (the host's LOCK key: frozen whatever LOCK says) */
enum { AS_T_EUCLID, AS_T_TURING, AS_NTYPE };
static const char *const AS_TYPE_NAME[AS_NTYPE] = {"EUCLID", "TURING"};
#define AS_THIST 2                        /* TURING: the patterns kept to go back to */
#define AS_NLR 6                          /* the lanes with a RANGE: VELO CHANCE DELAY REVERB PITCH RATCH */
#define AS_NLANE 16                       /* the lanes' steps (VELO CHANCE DELAY REVERB PITCH): a longer circle repeats them */
/* chords. The values are what scenes store (2 was SUS: now 5TH; 3 was OPEN: now OCT); the knob steps through
 * AS_CH_ORDER */
enum { AS_CH_OFF, AS_CH_TRI, AS_CH_5TH, AS_CH_OCT, AS_CH_7TH, AS_CH_9TH, AS_NCHORD };
static const char *const AS_CHORD_NAME[AS_NCHORD] = {"OFF", "TRIAD", "5TH", "OCT", "7TH", "9TH"};
static const uint8_t AS_CH_ORDER[AS_NCHORD] = {AS_CH_OFF, AS_CH_OCT, AS_CH_5TH, AS_CH_TRI, AS_CH_7TH, AS_CH_9TH};
/* chord ch's place on the knob, and the chord s places further (kept in 0..AS_NCHORD-1) */
static uint32_t as_ch_pos(uint32_t ch)
{
    uint32_t i;
    for (i = 0; i < AS_NCHORD; i++)
        if (AS_CH_ORDER[i] == ch)
            return i;
    return 0;
}
static uint8_t as_ch_step(uint32_t ch, int32_t s)
{
    int32_t i = (int32_t)as_ch_pos(ch) + (s > 0 ? 1 : -1);
    return AS_CH_ORDER[i < 0 ? 0 : i >= AS_NCHORD ? AS_NCHORD - 1 : i];
}

typedef struct {
    uint8_t len, hits, rot, prob;   /* 1..32, 0..len, 0..len-1, 0..127 */
    uint8_t frot;                   /* the forces' own rotation, 0..len-1 */
    uint8_t dose, mute, root;       /* 0..127, 0/1, the MIDI note of degree 0 */
    uint8_t chord;                  /* AS_CH_*: each hit plays these degrees above its own */
    uint8_t type;                   /* AS_T_*; TURING: LEN 1..16 */
    uint8_t tlock, tdens, trange;   /* TURING: 0..127, 0..127, 0..14 degrees */
} as_cfg_t;

typedef struct {
    as_cfg_t cfg;                   /* playing */
    as_cfg_t next;                  /* asked for: from the next cycle */
    uint32_t pat;                   /* bit i: step i hits */
    uint8_t pos;                    /* the step about to play */
    uint8_t die;                    /* 1: a draw is on */
    uint8_t svel[AS_NLANE];         /* the lanes, per step (FROT turns VELO): velocity 1..127 (ghost < 64, accent >= 112) */
    uint8_t chance[AS_MAXLEN];      /* 0..127 */
    uint8_t sdly[AS_NLANE];         /* the hit thrown into the delay, 0..127 (added to the host's DELAY send) .. */
    uint8_t srev[AS_NLANE];         /* .. and into the reverb */
    int8_t spitch[AS_NLANE];        /* scale degrees above the hit's own, -7..7 */
    uint8_t srat[AS_NLANE];         /* RATCH: the hit repeated in its step, 0..3 (x1 .. x4) */
    uint16_t rmask[AS_NLR];         /* RANGE (the SEQ menu's KNOB 3 / 4): bit l = lane step l draws in rlo..rhi at each pass */
    int8_t rlo[AS_NLR][AS_NLANE], rhi[AS_NLR][AS_NLANE];
    uint16_t treg;                  /* TURING: its pattern now (mutated as a cycle starts) .. */
    uint16_t tback[AS_THIST], tfwd[AS_THIST];   /* .. the ones before (PREV) and after (NEXT, back again) */
    uint8_t nback, nfwd;
    uint8_t tfreeze;                /* TURING: 1 = frozen (the host's LOCK key writes it) */
    int8_t dforce[AS_MAXLEN];       /* the draw: -1 0 +1 */
    int8_t pitch[AS_MAXLEN];        /* scale degrees above the root */
    int8_t vel[AS_MAXLEN];          /* the hit's velocity, % of its own: 100 (0 = 100), 75, 50 (04/11) */
    uint8_t ddly[AS_MAXLEN];        /* the draw's throws into the delay and the reverb, 0..127 (04/11) */
    uint8_t drev[AS_MAXLEN];
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
    uint32_t add;                   /* steps added by hand (TRIGGER + REC: alter_play.c), on top of the pattern */
} as_circ_t;

typedef struct {
    as_circ_t c[AS_NCIRC];
    uint8_t scale;                  /* AS_SC_*, shared */
    uint32_t rng;                   /* the PROB and CHANCE coins */
    int16_t pmod[AS_NCIRC];         /* added to each circle's PROB (the host's LFOs), 0 = none */
    uint8_t prog[AS_NCIRC];         /* each circle's chord PROGRESSION (AS_PG_*, 0 = none; Romain 10/11) */
    uint8_t pbar[AS_NCIRC];         /* .. the bar it is at (its chord: as_pg_deg) */
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
    uint8_t chord[2];               /* the chord's other notes (0 = none) */
    uint8_t dly, rev;               /* the DELAY and REVERB lanes: this hit thrown into them, 0..127 */
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

/* v turned into 0..len-1, whatever its sign (a step back from 0 is len - 1, for any LEN) */
static uint8_t as_wrap(int32_t v, uint32_t len)
{
    int32_t n = len ? (int32_t)len : 1, r = v % n;
    return (uint8_t)(r < 0 ? r + n : r);
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
    if (c.len > AS_PLAYLEN)
        c.len = AS_PLAYLEN;
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
    if (c.chord >= AS_NCHORD)
        c.chord = AS_CH_OFF;
    if (c.type >= AS_NTYPE)
        c.type = AS_T_EUCLID;
    if (c.type == AS_T_TURING && c.len > 16)
        c.len = 16;
    if (c.hits > c.len)
        c.hits = c.len;
    c.rot %= c.len;
    c.frot %= c.len;
    if (c.tlock > 127)
        c.tlock = 127;
    if (c.tdens > 127)
        c.tdens = 127;
    if (c.trange > 14)
        c.trange = 14;
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

/* RANDOM VELOCITY AND CHANCE (Romain 04/11): a hit's normal state is 100 %; the random gives it most of the time.
 * VELO: 100 % 7 times in 10, 75 % 2 in 10, 50 % 1 in 10, never lower. CHANCE: 100 % as a rule, and the more hits the
 * pattern has, the more of them play less surely: none with 1 or 2 hits, then 4 % more of the hits per hit (45 % at
 * most); those: 75 % half the time, 50 % a third, 25 % a sixth (25 % the floor). r: a draw 0..1023 */
static uint32_t as_vpct(uint32_t r) { r &= 1023u; return r < 717u ? 100u : r < 922u ? 75u : 50u; }
static uint32_t as_cpct(uint32_t r, uint32_t r2, uint32_t hits)
{
    uint32_t p = hits <= 2u ? 0u : (hits - 2u) * 41u;
    p = p > 460u ? 460u : p;
    if ((r & 1023u) >= p)
        return 100u;
    r2 &= 1023u;
    return r2 < 512u ? 75u : r2 < 853u ? 50u : 25u;
}
static uint32_t as_bits(uint32_t m) { uint32_t n = 0; for (; m; m &= m - 1u) n++; return n; }

/* a new draw of circle i, over all 32 steps (a longer LEN later finds them ready). What it draws (Romain 04/11: three
 * dice, each its own): AS_DW_RHY the pattern (HITS, ROT, flams, a ratchet) · AS_DW_VC the velocities and chances ·
 * AS_DW_PDR the pitches and throws into the delay and reverb · AS_DW_SND (CHAOS's full draw only) glide, length,
 * envelope, timbre. What it does not draw stays as the last draw left it (a fresh draw: as the clean pattern) */
enum { AS_DW_RHY = 1, AS_DW_VC = 2, AS_DW_PDR = 4, AS_DW_SND = 8, AS_DW_ALL = 15 };
static void as_draw_w(as_seq_t *q, uint32_t i, uint32_t w)
{
    as_circ_t *c = &q->c[i];
    int32_t d = c->next.dose, k, j, lk = c->locks, ndly = 0, nrev = 0;
    uint32_t *s = &c->seed, hits;
    if (!c->die)                                        /* (from the clean pattern: nothing left of an old draw) */
        w |= 0x100u;
    if (w & (AS_DW_RHY | 0x100u)) {                     /* the rhythm: HITS moved by up to half the circle, ROT anywhere */
        c->dhits = 0;
        c->drot = 0;
        for (k = 0; k < AS_MAXLEN; k++)
            c->rat[k] = c->flam[k] = 0;
        if ((w & AS_DW_RHY) && !(lk & AS_LK_RHYTHM)) {
            if (as_coin(s, d * 8))
                c->dhits = (int8_t)as_uni(s, 1 + d * 7 / 127);
            if (as_coin(s, d * 7))
                c->drot = (uint8_t)(as_rand(s) % AS_MAXLEN);
            for (k = 0; k < AS_MAXLEN; k++)
                if (as_coin(s, d * 2))
                    c->flam[k] = 1;
            /* a ratchet (Romain 04/11): none as a rule; at most ONE step, x2 mostly, x3 1 in 16; a quarter at DOSE 127 */
            if (as_coin(s, d * 2)) {
                k = as_rand(s) % (c->cfg.len ? c->cfg.len : 16u);
                c->rat[k] = (uint8_t)(as_rand(s) % 16u ? 1u : 2u);
                c->flam[k] = 0;
            }
        }
    }
    hits = as_bits(c->pat & (c->cfg.len >= 32u ? 0xFFFFFFFFu : (1u << c->cfg.len) - 1u));
    if (hits == 0u)
        hits = c->cfg.hits;
    for (k = 0; k < AS_MAXLEN; k++) {
        if (w & (AS_DW_VC | 0x100u)) {
            c->dforce[k] = 0;
            c->vel[k] = 100;
            c->dchance[k] = 127;
            if (w & AS_DW_VC) {
                if (!(lk & AS_LK_VEL))
                    c->vel[k] = (int8_t)as_vpct(as_rand(s));
                if (!(lk & AS_LK_CHANCE)) {
                    uint32_t r1 = as_rand(s), r2 = as_rand(s);
                    c->dchance[k] = (uint8_t)(as_cpct(r1, r2, hits) * 127u / 100u);
                }
            }
        }
        if (w & (AS_DW_PDR | 0x100u)) {
            c->pitch[k] = 0;
            c->ddly[k] = c->drev[k] = 0;
            if (w & AS_DW_PDR) {
                if (!(lk & AS_LK_PITCH) && as_coin(s, d * 7))   /* up to 3 octaves of degrees, the root less often */
                    c->pitch[k] = (int8_t)(as_rand(s) % (uint32_t)(1 + d * 21 / 127));
                if (!(lk & AS_LK_THROW) && ndly < 3 && as_coin(s, 150))                 /* a few throws (3 at most each): 50..110 */
                    c->ddly[k] = (uint8_t)(50u + as_rand(s) % 61u), ndly++;
                if (!(lk & AS_LK_THROW) && nrev < 3 && as_coin(s, 120))
                    c->drev[k] = (uint8_t)(50u + as_rand(s) % 61u), nrev++;
            }
        }
        if (w & (AS_DW_SND | 0x100u)) {
            c->glide[k] = 0;
            c->gate[k] = 1;
            c->env[k] = 0;
            for (j = 0; j < AS_NTIMB; j++)
                c->timb[k][j] = 0;
            if (w & AS_DW_SND) {
                if (!(lk & AS_LK_SOUND) && as_coin(s, 120 + d * 6))
                    c->dforce[k] = (int8_t)(as_rand(s) & 1u ? 1 : -1);
                c->glide[k] = (uint8_t)(!(lk & AS_LK_SOUND) && as_coin(s, d * 3));
                for (j = 0; j < AS_NTIMB; j++)
                    c->timb[k][j] = (int8_t)(lk & AS_LK_SOUND ? 0 : as_uni(s, d * 5 / 8));
                if (!(lk & AS_LK_SOUND)) {
                    if (as_coin(s, d * 3))
                        c->gate[k] = (uint8_t)(2u + as_rand(s) % 3u);
                    c->env[k] = (int8_t)as_uni(s, d * 50 / 127);
                }
            }
        }
    }
}
static void as_draw(as_seq_t *q, uint32_t i) { as_draw_w(q, i, AS_DW_ALL); }

/* bits n..: turned by r within n */
static uint32_t as_rot(uint32_t m, uint32_t n, uint32_t r)
{
    uint32_t full = n >= 32u ? 0xFFFFFFFFu : (1u << n) - 1u;
    r %= n ? n : 1u;
    m &= full;
    return r ? ((m << r) | (m >> (n - r))) & full : m;
}

/* circle c's pattern as it plays: its settings, the draw's HITS and ROT on top (TURING: its register) */
static void as_repat(as_circ_t *c)
{
    int32_t h = c->cfg.hits, r = c->cfg.rot;
    if (c->cfg.type == AS_T_TURING) {
        c->pat = as_rot(c->treg, c->cfg.len, (uint32_t)r) | as_rot(c->add, c->cfg.len, 0);
        return;
    }
    if (c->die) {
        h = h + c->dhits;
        h = h < 0 ? 0 : h > c->cfg.len ? c->cfg.len : h;
        if (c->cfg.hits && !h)
            h = 1;                                      /* (a circle that plays keeps a hit) */
        r += c->drot;
    }
    c->pat = as_euclid(c->cfg.len, (uint32_t)h, (uint32_t)r % c->cfg.len) | as_rot(c->add, c->cfg.len, 0);
}

/* a step added by hand to circle i (TRIGGER + REC), kept: it plays on every cycle, on top of the pattern */
static void as_add(as_seq_t *q, uint32_t i, uint32_t k)
{
    if (i < AS_NCIRC && k < AS_MAXLEN) {
        q->c[i].add |= 1u << k;
        as_repat(&q->c[i]);
    }
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
        for (k = 0; k < AS_MAXLEN; k++)
            c->chance[k] = 127;
        for (k = 0; k < AS_NLANE; k++)
            c->svel[k] = AS_VEL[AS_NORMAL];
        c->cfg.tlock = c->next.tlock = 100;
        c->cfg.tdens = c->next.tdens = 64;
        c->cfg.trange = c->next.trange = 4;
        c->treg = (uint16_t)(as_rand(&c->seed) & 0xFFFFu);
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
        c->cfg.chord = c->next.chord;
    }
}

/* turn circle i's pattern by d steps at once (SHIFT), kept: the waiting settings too */
static void as_turn(as_seq_t *q, uint32_t i, int32_t d)
{
    as_circ_t *c;
    if (i >= AS_NCIRC)
        return;
    c = &q->c[i];
    c->cfg.rot = as_wrap((int32_t)c->cfg.rot + d, c->cfg.len);
    c->next.rot = as_wrap((int32_t)c->next.rot + d, c->next.len);
    c->add = as_rot(c->add, c->cfg.len, as_wrap(d, c->cfg.len));   /* (the added steps turn with it) */
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

/* the die of circle i: a new draw (w: AS_DW_*, what it draws), at once, kept until the next roll */
static void as_roll_w(as_seq_t *q, uint32_t i, uint32_t w)
{
    if (i < AS_NCIRC) {
        as_draw_w(q, i, w);
        q->c[i].die = 1;
        as_repat(&q->c[i]);
    }
}
static void as_roll(as_seq_t *q, uint32_t i) { as_roll_w(q, i, AS_DW_ALL); }

/* the clean pattern again: the draw forgotten */
static void as_clear(as_seq_t *q, uint32_t i)
{
    if (i < AS_NCIRC) {
        q->c[i].die = 0;
        as_repat(&q->c[i]);
    }
}

/* the chord PROGRESSIONs (Romain 10/11: the SCL page's knob 4, where DOSE was): a circle's hits moved by a scale degree
 * each bar (16 steps), from the ROOT, in its SCALE (a scale of 7 notes as written; another: each degree scaled to its
 * size). Its CHORD (TRIAD, 7TH ..) is built on the moved degree: CHORD says which chord, the PROGRESSION on which degree */
#define AS_NPROG 10
static const char *const AS_PG_NAME[AS_NPROG] = {"OFF", "1-5-6-4", "1-6-4-5", "1-4", "1-6", "1-7-6-7", "1-7-6-5",
                                                 "6-4-1-5", "1-3-7-4", "1-4-1-5"};
static const int8_t AS_PG[AS_NPROG][4] = {{0, 0, 0, 0}, {0, 4, 5, 3}, {0, 5, 3, 4}, {0, 3, 0, 3}, {0, 5, 0, 5},
                                          {0, 6, 5, 6}, {0, 6, 5, 4}, {5, 3, 0, 4}, {0, 2, 6, 3}, {0, 3, 0, 4}};
static int32_t as_pg_deg(const as_seq_t *q, uint32_t i)   /* circle i's degree this bar */
{
    uint32_t p = q->prog[i % AS_NCIRC] % AS_NPROG, n = 0;
    const int8_t *sc;
    int32_t d;
    if (!p)
        return 0;
    sc = AS_SCALES[q->scale < AS_NSCALE ? q->scale : AS_SC_CHR];
    while (sc[n] >= 0)
        n++;
    d = AS_PG[p][q->pbar[i % AS_NCIRC] & 3u];
    return n == 7u ? d : (d * (int32_t)n + 3) / 7;
}

/* a scale degree above a root as a note, carried into the octaves above */
static uint32_t as_note(const as_seq_t *q, uint32_t root, int32_t deg)
{
    const int8_t *sc = AS_SCALES[q->scale < AS_NSCALE ? q->scale : AS_SC_CHR];
    int32_t n = 0, oct, note;
    while (sc[n] >= 0)
        n++;
    oct = deg >= 0 ? deg / n : -((-deg + n - 1) / n);   /* (below the root: the octaves down) */
    note = (int32_t)root + oct * 12 + sc[deg - oct * n];
    return (uint32_t)(note < 1 ? 1 : note > 127 ? 127 : note);
}

/* the scale note above degree deg nearest t2/2 semitones over it (a tie: the lower) */
static uint32_t as_cnote(const as_seq_t *q, uint32_t root, int32_t deg, int32_t t2)
{
    int32_t base = (int32_t)as_note(q, root, deg), best = base + t2 / 2, bd = 999, d;
    for (d = deg + 1; d <= deg + 26; d++) {
        int32_t iv = (int32_t)as_note(q, root, d) - base, e = 2 * iv - t2;
        if (e < 0)
            e = -e;
        if (e < bd) {
            bd = e;
            best = base + iv;
        }
        if (2 * iv > t2 + bd)
            break;
    }
    return (uint32_t)(best < 1 ? 1 : best > 127 ? 127 : best);
}

/* the velocity a step of circle c plays with: its VELO lane (turned by FROT), the draw a ghost / accent away */
static int32_t as_svel(const as_circ_t *c, uint32_t k)
{
    uint32_t len = c->cfg.len ? c->cfg.len : 1u;
    int32_t v = c->svel[((k + len - c->cfg.frot % len) % len) % AS_NLANE];
    if (c->die)
        v += c->dforce[k] * 30;
    return v < 1 ? 1 : v > 127 ? 127 : v;
}

/* lane ln's step l as it plays now: its value v, or (a RANGE on it) a new draw between its two points */
static int32_t as_rng(as_seq_t *q, const as_circ_t *c, uint32_t ln, uint32_t l, int32_t v)
{
    int32_t lo, hi;
    if (!((c->rmask[ln] >> l) & 1u))
        return v;
    lo = c->rlo[ln][l];
    hi = c->rhi[ln][l];
    if (lo > hi) {
        int32_t t = lo;
        lo = hi;
        hi = t;
    }
    return lo + (int32_t)(as_rand(&q->rng) % (uint32_t)(hi - lo + 1));
}

/* .. as a force (ghost < 64, accent >= 112) */
static uint32_t as_force(const as_circ_t *c, uint32_t k)
{
    int32_t v = as_svel(c, k);
    return v < 64 ? AS_GHOST : v >= 112 ? AS_ACCENT : AS_NORMAL;
}

/* TURING: a new cycle of circle c: each of its bits replaced with a chance (LOCK 127 never), by a hit with DENS */
static void as_turing(as_circ_t *c, uint32_t *rng)
{
    uint32_t k, len = c->cfg.len, ch = c->tfreeze ? 0u : 127u - c->cfg.tlock;
    for (k = 0; k < len && k < 16u; k++)
        if (as_rand(rng) % 127u < ch) {
            if (as_rand(rng) % 127u < c->cfg.tdens)
                c->treg |= (uint16_t)(1u << k);
            else
                c->treg &= (uint16_t)~(1u << k);
        }
}

/* TURING: circle i's pattern replaced at once (from its next step) by a new one (NEXT; one undone by PREV first) or by
 * the one before (PREV); two kept each way */
static void as_tpush(uint16_t *st, uint8_t *n, uint16_t v)
{
    uint32_t k;
    for (k = AS_THIST - 1u; k > 0u; k--)
        st[k] = st[k - 1u];
    st[0] = v;
    if (*n < AS_THIST)
        (*n)++;
}
static uint16_t as_tpop(uint16_t *st, uint8_t *n)
{
    uint16_t v = st[0];
    uint32_t k;
    for (k = 0; k + 1u < AS_THIST; k++)
        st[k] = st[k + 1u];
    (*n)--;
    return v;
}
static void as_tnext(as_seq_t *q, uint32_t i)
{
    as_circ_t *c;
    uint32_t k;
    if (i >= AS_NCIRC)
        return;
    c = &q->c[i];
    as_tpush(c->tback, &c->nback, c->treg);
    if (c->nfwd)
        c->treg = as_tpop(c->tfwd, &c->nfwd);
    else
        for (k = 0, c->treg = 0; k < 16u; k++)   /* a new one, every bit drawn with DENS */
            if (as_rand(&q->rng) % 127u < c->cfg.tdens)
                c->treg |= (uint16_t)(1u << k);
    as_repat(c);
}
static void as_tprev(as_seq_t *q, uint32_t i)
{
    as_circ_t *c;
    if (i >= AS_NCIRC || !q->c[i].nback)
        return;
    c = &q->c[i];
    as_tpush(c->tfwd, &c->nfwd, c->treg);
    c->treg = as_tpop(c->tback, &c->nback);
    as_repat(c);
}

/* TURING: step k's pitch, a walk over RANGE degrees read from the register's bits there */
static int32_t as_tpitch(const as_circ_t *c, uint32_t k)
{
    uint32_t len = c->cfg.len ? c->cfg.len : 1u, w = 0, b;
    for (b = 0; b < 4u; b++)
        w |= ((c->treg >> ((k + b) % len)) & 1u) << b;
    return (int32_t)(w * c->cfg.trange / 15u);
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
            if (c->cfg.type == AS_T_TURING)
                as_turing(c, &q->rng);
            as_repat(c);
        }
        k = c->pos;
        {
            uint32_t ch = (uint32_t)as_rng(q, c, 1u, k % AS_NLANE, c->chance[k]);
            int32_t prob = (int32_t)c->cfg.prob + q->pmod[i];
            if (c->die && c->dchance[k] < ch)
                ch = c->dchance[k];
            on = !c->cfg.mute && (c->pat >> k & 1u) &&
                 (prob >= 127 || (int32_t)(as_rand(&q->rng) % 127u) < prob) &&
                 (ch >= 127 || as_rand(&q->rng) % 127u < ch);
        }
        c->pos = (uint8_t)((c->pos + 1u) % c->cfg.len);
        for (j = 0; j < sizeof *e; j++)
            ((uint8_t *)e)[j] = 0;
        if (!on)
            continue;
        {
            uint32_t f = as_force(c, k), l = k % AS_NLANE;
            int32_t vel = as_svel(c, k), deg = (int32_t)as_rng(q, c, 4u, l, c->spitch[l]);
            uint32_t len2 = c->cfg.len ? c->cfg.len : 1u, vi = ((k + len2 - c->cfg.frot % len2) % len2) % AS_NLANE;
            if ((c->rmask[0] >> vi) & 1u) {             /* VELO with a RANGE: a new velocity each pass */
                vel = as_rng(q, c, 0u, vi, c->svel[vi]) + (c->die ? c->dforce[k] * 30 : 0);
                vel = vel < 1 ? 1 : vel > 127 ? 127 : vel;
            }
            e->on = 1;
            e->force = (uint8_t)f;
            e->gate = 1;
            e->dly = (uint8_t)as_rng(q, c, 2u, l, c->sdly[l]);
            e->rev = (uint8_t)as_rng(q, c, 3u, l, c->srev[l]);
            e->rat = (uint8_t)as_rng(q, c, 5u, l, c->srat[l]);   /* (the RATCH lane; a draw's burst may go further) */
            if (c->cfg.type == AS_T_TURING)
                deg += as_tpitch(c, k);
            if (c->die) {
                if (c->rat[k] > e->rat)
                    e->rat = c->rat[k];
                e->flam = c->flam[k];
                e->gate = c->gate[k];
                e->env = c->env[k];
                deg += c->pitch[k];
                if (c->vel[k] > 0 && c->vel[k] < 100)  /* (the draw's 75 / 50 %) */
                    vel = vel * c->vel[k] / 100;
                if (c->ddly[k] > e->dly)
                    e->dly = c->ddly[k];
                if (c->drev[k] > e->rev)
                    e->rev = c->drev[k];
                e->glide = c->glide[k];
                for (j = 0; j < AS_NTIMB; j++)
                    e->timb[j] = c->timb[k][j];
            }
            deg += as_pg_deg(q, i);                     /* (the PROGRESSION's degree this bar) */
            e->note = (uint8_t)as_note(q, c->cfg.root, deg);
            switch (c->cfg.chord) {                     /* the chord moves with the hit's degree */
            case AS_CH_OCT:
                e->chord[0] = (uint8_t)(e->note + 12 > 127 ? 127 : e->note + 12);
                break;
            case AS_CH_5TH:
                e->chord[0] = (uint8_t)(e->note + 7 > 127 ? 127 : e->note + 7);
                e->chord[1] = (uint8_t)(e->note + 12 > 127 ? 127 : e->note + 12);
                break;
            case AS_CH_TRI:                             /* (third ~3.5, fifth 7, seventh ~10.5, ninth 14 semitones) */
                e->chord[0] = (uint8_t)as_cnote(q, c->cfg.root, deg, 7);
                e->chord[1] = (uint8_t)as_cnote(q, c->cfg.root, deg, 14);
                break;
            case AS_CH_7TH:
                e->chord[0] = (uint8_t)as_cnote(q, c->cfg.root, deg, 7);
                e->chord[1] = (uint8_t)as_cnote(q, c->cfg.root, deg, 21);
                break;
            case AS_CH_9TH:
                e->chord[0] = (uint8_t)as_cnote(q, c->cfg.root, deg, 21);
                e->chord[1] = (uint8_t)as_cnote(q, c->cfg.root, deg, 28);
                e->note = (uint8_t)as_cnote(q, c->cfg.root, deg, 7);
                break;
            }
            e->vel = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel);
        }
    }
}
