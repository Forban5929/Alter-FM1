/* ALTER's CONSOLE (étape 4, Romain 05/11): each circle's strip, after its DIST and INSERT, before the RUMBLE, the
 * SLICER, its LEVEL and its sends: DIST (Felucca's P_DIST, as before) -> EQ LOW MID HIGH -> COMP -> the sends to the
 * shared DELAY and REVERB (P_DLY P_REV, as before). Included at the end of alter_fx.c (mix_part / mix_spread call it).
 *
 * EQ: the block split by two one-pole low-passes, ~250 Hz and ~2.5 kHz: low = lp1, mid = lp2 - lp1, high = x - lp2 (the
 * three add up to x: at 64 64 64 it is the input). A band's value 0..127: 0 KILL (a DJ's kill), 32 -12 dB, 64 0 dB,
 * 127 +12 dB (acs_gain). COMP: one knob, AMOUNT 0..127: 0 off; up, the threshold falls (-4 .. -22 dB) and the ratio
 * rises (2:1 .. 8:1); an automatic make-up brings the strip's peaks (held ~1.5 s) back to where they were (at most
 * +12 dB): the body comes up, the peaks stay. Peak follower on the block itself (it sees the hit before
 * it plays it), release ~90 ms; the gain goes down within 8 samples, up over the block (no click). At 64 64 64 and COMP 0 nothing runs: the
 * output bit for bit as before, no cost.
 *
 * Integers only (FM-1): mulq15 is 32 bits, so the signal (up to ~230000 after DIST) is shifted by 2 before the
 * products. Each circle's state is its own (the second core may render another circle meanwhile); acs_v is written by
 * the UI (bytes) and read here. Not in REC; kept in the USER scenes (alter_store.c alt_ub.s.cs) */
#define ACS_N 4u                                    /* LOW MID HIGH COMP */
enum { ACS_LOW, ACS_MID, ACS_HIGH, ACS_COMP };
static const uint8_t ACS_DEF[ACS_N] = {64, 64, 64, 0};
static uint8_t acs_v[AM_NC][ACS_N] = {{64, 64, 64, 0}, {64, 64, 64, 0}, {64, 64, 64, 0}};
static int32_t acs_gr[AM_NC] = {4096, 4096, 4096};  /* the COMP's reduction now, Q12 (the MIXER's meter) */
typedef struct {
    int32_t lp1, lp2;                               /* the EQ's two low-passes */
    int32_t env, g;                                 /* the COMP's follower; its gain at the end of the last block, Q12 */
    int32_t thr, rat, pkl;                          /* .. its threshold, ratio for amt; its peaks' level, held */
    uint8_t eq_on, amt;                             /* (amt: the AMOUNT thr / mk / rat were worked out for) */
} acs_t;
static acs_t acs[AM_NC];

static void acs_reset(uint32_t c)                   /* a scene with no console stored, NEW SCENE: flat */
{
    uint32_t k;
    if (c >= AM_NC)
        return;
    for (k = 0; k < ACS_N; k++)
        acs_v[c][k] = ACS_DEF[k];
}

/* a band's value -> its gain, Q12 (4096 = 0 dB): 0 none .. 32 1024 (-12 dB) .. 64 4096 .. 95 ~8000 (+5.8) .. 127 ~16130 */
static int32_t acs_gain(int32_t v)
{
    int32_t d;
    v = clamp(v, 0, 127);
    if (v <= 64)
        return v * v;
    d = v - 64;
    return 4096 + d * d * 2 + d * 65;
}

/* AMOUNT -> threshold (falls ~0.14 dB a step from ~20000: -4 .. -22 dB), ratio 2:1 .. 8:1 */
static void acs_setup(acs_t *s, uint32_t amt)
{
    int32_t t = 20000;
    uint32_t k;
    for (k = 1; k < amt; k++)
        t = (int32_t)(((int64_t)t * 32230) >> 15);
    s->thr = t;
    s->rat = 2 + (int32_t)amt * 6 / 127;
    s->amt = (uint8_t)amt;
}

/* circle c's block b (n samples) through its EQ and COMP */
static void acs_part(uint32_t c, int32_t *b, uint32_t n)
{
    acs_t *s;
    const uint8_t *v;
    uint32_t i, amt;
    int eq;
    if (c >= AM_NC)
        return;
    s = &acs[c];
    v = acs_v[c];
    eq = v[ACS_LOW] != 64 || v[ACS_MID] != 64 || v[ACS_HIGH] != 64;
    amt = v[ACS_COMP] & 127u;
    if (eq) {
        int32_t gl = acs_gain(v[ACS_LOW]), gm = acs_gain(v[ACS_MID]), gh = acs_gain(v[ACS_HIGH]);
        if (!s->eq_on && n)                         /* starting: the low-passes on the signal, no thump */
            s->lp1 = s->lp2 = b[0];
        s->eq_on = 1;
        for (i = 0; i < n; i++) {
            int32_t x = clamp(b[i], -230000, 230000), lo, mi, hi;   /* (DIST's own bound: the products fit) */
            s->lp1 += (((x - s->lp1) >> 2) * 1147) >> 13;   /* ~250 Hz */
            s->lp2 += (((x - s->lp2) >> 2) * 9830) >> 13;   /* ~2.5 kHz */
            lo = s->lp1;
            mi = s->lp2 - s->lp1;
            hi = x - s->lp2;
            b[i] = (((lo >> 2) * gl) >> 10) + (((mi >> 2) * gm) >> 10) + (((hi >> 2) * gh) >> 10);
        }
    } else
        s->eq_on = 0;
    if (!amt) {
        s->env = 0;
        s->g = 4096;
        s->pkl = 0;
        acs_gr[c] = 4096;
        return;
    }
    {
        int32_t pk = 0, gt, red, g0, dg, mk;
        uint32_t na;
        if (s->amt != amt)
            acs_setup(s, amt);
        if (s->g <= 0)
            s->g = 4096;
        for (i = 0; i < n; i++) {
            int32_t a = b[i] < 0 ? -b[i] : b[i];
            if (a > pk)
                pk = a;
        }
        if (pk > s->env)
            s->env = pk;                            /* attack: this block's own peak */
        else
            s->env -= s->env >> 7;                  /* release: ~90 ms */
        red = 4096;
        if (s->env > s->thr)
            red = (int32_t)((int64_t)(s->thr + (s->env - s->thr) / s->rat) * 4096 / s->env);
        acs_gr[c] = red;
        if (s->env > s->pkl)                        /* the peaks' level: up at once, down ~1.5 s */
            s->pkl = s->env;
        else
            s->pkl -= s->pkl >> 11;
        mk = 4096;
        if (s->pkl > s->thr)                        /* the make-up: what the reduction takes from those peaks */
            mk = clamp((int32_t)((int64_t)s->pkl * 4096 / (s->thr + (s->pkl - s->thr) / s->rat)), 4096, 16384);
        gt = clamp((int32_t)(((int64_t)red * mk) >> 12), 0, 16384);
        g0 = s->g;
        na = gt < g0 ? (n < 8u ? n : 8u) : n;       /* down fast, up gently */
        dg = (gt - g0) / (int32_t)(na ? na : 1u);
        for (i = 0; i < n; i++) {
            int32_t x = clamp(b[i], -460000, 460000);
            b[i] = ((x >> 2) * g0) >> 10;
            if (i < na)
                g0 = i + 1u == na ? gt : g0 + dg;
        }
        s->g = gt;
    }
}
