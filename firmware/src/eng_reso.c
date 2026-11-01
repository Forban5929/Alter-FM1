/* SPDX-License-Identifier: GPL-3.0-only
 * RESO: four resonant filters struck by an exciter (after a QMMF, a quad multimode filter played as a resonator).
 *
 *   exciter -> 4 x TPT state-variable filters (A. Simper's), tuned note x ratio_i -> their band outputs summed
 *
 * The four main knobs follow the new engines' common grammar (MATTER, SPREAD, RESONANCE, DIRT):
 * MATR  what strikes: 0 an impulse (a thump), 64 a folded click (metal), 127 a noise burst (crossfaded between)
 * SPRD  harmonic -> inharmonic: 0 the four on the note (one big resonance), up to 3 octaves apart on inharmonic
 *       ratios (1, 2^(0.29 s), 2^(0.61 s), 2^s for s octaves)
 * RES   a short ring (low Q) -> a long one; past ~110 the filters sing by themselves (self-oscillation: a sine at
 *       their pitch, played by the note like an oscillator)
 * DIRT  a tanh inside each filter's loop, driven harder (the ring and the scream saturate) and the exciter folded
 * OUT   MIX the four rings; NOTCH the exciter minus the rings (the phase cancels: hollow, industrial)
 * DCAY  the exciter's length (0.2 .. 80 ms)
 * PING  which filters a hit strikes: ALL, CYCL (one after the other), RAND (one at random), 1..4 (that one). The keys
 *       (alter_play.c, reso_force) strike one by hand.
 * FINE  the four detuned from their ratios (beating), up to ~40 cents
 * The track's SHP moves MATR, FLT moves the pitch of the filters (cutoff), as on every engine: the ADSR on top.
 *
 * Polyphony 2 per part: a ring rings on under the next hit. Fixed point: Q15 audio, Q24 coefficients (worked out
 * once a block: g = tan(pi f / FS) from the sine table, four divisions). */

#define RESO_NF 4
#define RESO_POLY 2
enum { RESO_MIX, RESO_NOTCH };
enum { RP_ALL, RP_CYCLE, RP_RAND, RP_1, RP_2, RP_3, RP_4, RP_N };

typedef struct {
    int32_t ic1[RESO_NF], ic2[RESO_NF];
    int32_t env;                 /* the exciter's level, Q16 */
    uint32_t eph;                /* the folded click's phase */
    int32_t rng;
    uint8_t mask;                /* the filters this hit strikes */
} reso_slot_t;

ENG_MEM_FITS("RESO", sizeof(reso_slot_t) * RESO_POLY);

static uint8_t reso_cyc[NPART];          /* CYCL: the filter struck next */
static uint8_t reso_force[NPART];        /* a key's strike: this mask on the next hit (alter_play.c), then 0 */

static const char *const N_RESO_OUT[] = {"MIX", "NTCH"};
static const char *const N_RESO_PING[] = {"ALL", "CYCL", "RAND", "1", "2", "3", "4"};
/* the ratios at SPRD = 3 octaves, as octave fractions Q12 */
static const int16_t RESO_OCT[RESO_NF] = {0, 1188, 2499, 4096};
static const int8_t RESO_DET[RESO_NF] = {0, 3, -2, 1};   /* FINE's pattern */

static reso_slot_t *reso_slot_of(track_t *t, voice_t *v)
{
    uint32_t i;
    if (t < &trk[0] || t >= &trk[NPART])
        return 0;
    i = (uint32_t)(v - t->v);
    return i < RESO_POLY ? (reso_slot_t *)ENG_MEM(t - trk) + i : 0;
}

/* 2^(o / 4096), Q16 (a quadratic between the powers) */
static uint32_t reso_exp2(int32_t o)
{
    int32_t n = o >> 12, f = o & 4095;
    uint32_t w = 65536u + (uint32_t)((f * (2690 + ((f * 1406) >> 12))) >> 12) * 16u;
    return n >= 0 ? w << n : w >> -n;
}

static void reso_note_on(track_t *t, voice_t *v)
{
    reso_slot_t *S = reso_slot_of(t, v);
    uint32_t p = (uint32_t)(t - trk), ping = (uint32_t)t->p[P_E6] % RP_N;
    if (!S)
        return;
    S->env = 65536;
    S->eph = 0;
    if (!S->rng)
        S->rng = (int32_t)((v->note + 1u) * 0x9E3779B9u ^ v->age * 0x85EBCA6Bu) | 1;
    if (reso_force[p]) {                 /* a key: that filter */
        S->mask = reso_force[p];
        reso_force[p] = 0;
    } else if (ping == RP_CYCLE) {
        S->mask = (uint8_t)(1u << (reso_cyc[p] % RESO_NF));
        reso_cyc[p] = (uint8_t)((reso_cyc[p] + 1u) % RESO_NF);
    } else if (ping == RP_RAND)
        S->mask = (uint8_t)(1u << (noise32(&S->rng) >> 30));
    else if (ping >= RP_1)
        S->mask = (uint8_t)(1u << (ping - RP_1));
    else
        S->mask = 15u;
}

static void reso_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    reso_slot_t *S = reso_slot_of(t, v);
    const int16_t *p = t->p;
    int32_t a[RESO_NF][3], kq, kn, lim, inv, drive, mt, sprd, fine, i, rate, fg, notch, rng, env;
    uint32_t inc[RESO_NF], k, einc, on = 0;
    if (!S)
        return;
    /* RES: k = 2 (no ring) .. ~0.004 (a long one); past 111 below 0 (it sings) */
    {
        int32_t r = p[P_E2];
        kq = r < 112 ? (int32_t)(((int64_t)(112 - r) * (112 - r) << 25) / (112 * 112)) + 70000
                     : -(r - 111) * 100000;
    }
    /* the band outputs scaled so a long ring is not lost (between unity peak, k, and none, 1): ~ sqrt k */
    kn = (int32_t)alt_isqrt((uint32_t)(kq > 503316 ? kq : 503316) << 6) << 3;   /* Q15 of 8 sqrt(k) (k Q24, at least 0.03):
                                                                               * the level measured (alter_render.mjs) */
    /* DIRT: the tanh's ceiling (Q15 units), lower and driven harder as DIRT rises; the scream held there */
    drive = 4096 + p[P_E3] * p[P_E3] / 2;                     /* Q12: 1 .. ~3 */
    lim = 22000 - p[P_E3] * 90;
    inv = (int32_t)((((int64_t)32768 << 16) * drive >> 12) / lim);
    mt = clamp((p[P_E0] << 8) + m->shape - (64 << 8), 0, 127 << 8) >> 8;   /* MATR (+ SHP) */
    sprd = p[P_E1] * 3;                                       /* octaves x 127 */
    fine = p[P_E7];
    notch = p[P_E4] == RESO_NOTCH;
    /* the four tunings: the note (FLT moves it, +-2 octaves) x ratio x FINE; one past 0.45 FS is left silent */
    for (k = 0; k < RESO_NF; k++) {
        int32_t o = sprd * RESO_OCT[k] / 127 + (m->cutoff - (64 << 8)) * 2 * 4096 / (64 << 8) +
                    RESO_DET[k] * fine * 4096 / (127 * 120);
        uint64_t x = ((uint64_t)m->inc * reso_exp2(o)) >> 16;
        if (x > 0x73000000ull) {
            inc[k] = 0;
            continue;
        }
        inc[k] = (uint32_t)x;
        {
            int32_t s = sine_i(inc[k] >> 1), c = sine_i((inc[k] >> 1) + 0x40000000u);
            int32_t g = (int32_t)(((int64_t)s << 24) / (c > 64 ? c : 64));
            int64_t den = (1ll << 24) + (((int64_t)g * (g + kq)) >> 24);
            a[k][0] = (int32_t)((1ll << 48) / (den > (1ll << 20) ? den : (1ll << 20)));
            a[k][1] = (int32_t)(((int64_t)g * a[k][0]) >> 24);
            a[k][2] = (int32_t)(((int64_t)g * a[k][1]) >> 24);
        }
        on |= 1u << k;
    }
    /* the exciter: its fall over DCAY (0.2 .. 80 ms), the click's sine at three times the note */
    rate = 65536 / (9 + p[P_E5] * p[P_E5] * 28 / 127);
    einc = m->inc * 3u;
    fg = 4096 + p[P_E3] * 64;                                 /* the fold's gain, Q12 */
    rng = S->rng;
    env = S->env;
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = 0, y = 0, xs = 0;
        if (env > 16) {
            int32_t e = env >> 1, imp = e, fold, nz, w;
            /* the folded click: a sine burst, folded */
            w = (int32_t)(((int64_t)sine_i(S->eph) * e) >> 15);
            w = (int32_t)(((int64_t)w * fg) >> 11);
            {
                int32_t q = (w + 16384) & 65535;
                fold = (q < 32768 ? q - 16384 : 49151 - q) * 2;
            }
            S->eph += einc;
            nz = (int32_t)(((int64_t)((int32_t)(noise32(&rng) >> 16) - 32768) * e) >> 15);
            x = mt < 64 ? imp + (fold - imp) * mt / 64 : fold + (nz - fold) * (mt - 64) / 63;
            env -= (int32_t)(((int64_t)env * rate) >> 16) + 1;
        } else if (kq < 0)
            x = (int32_t)(noise32(&rng) >> 28) - 8;           /* (a breath: the scream starts on its own) */
        for (k = 0; k < RESO_NF; k++) {
            int32_t v0, v1, v2, v3, *c1 = &S->ic1[k], *c2 = &S->ic2[k];
            if (!((on >> k) & 1u))
                continue;
            v0 = (S->mask >> k) & 1u ? x : 0;
            v3 = v0 - *c2;
            v1 = (int32_t)(((int64_t)a[k][0] * *c1 + (int64_t)a[k][1] * v3) >> 24);
            v2 = *c2 + (int32_t)(((int64_t)a[k][1] * *c1 + (int64_t)a[k][2] * v3) >> 24);
            *c1 = 2 * v1 - *c1;
            *c2 = clamp(2 * v2 - *c2, -(1 << 22), 1 << 22);
            /* DIRT: the band state through a tanh at its ceiling */
            *c1 = (int32_t)(((int64_t)softclip((int32_t)(((int64_t)clamp(*c1, -(1 << 22), 1 << 22) * inv) >> 16)) * lim) >>
                            15);
            y += (int32_t)(((int64_t)v1 * kn) >> 15);
            if (notch)
                xs += v0 - (int32_t)(((int64_t)kq * v1) >> 24);
        }
        if (notch)
            y = xs;
        y = soft_knee(y, 20000);
        out[i] += voice_amp(y, m, (uint32_t)i);
    }
    S->env = env;
    S->rng = rng;
}

/* {MATR SPRD RES DIRT OUT DCAY PING FINE} */
static const preset_t RESO_PRESETS[] = {
    {"PING", {10, 40, 90, 10, RESO_MIX, 8, RP_ALL, 10}, {0, 90, 127, 70}, 0, 0, FX(0, 0, 20, 30)},
    {"GLASS", {64, 90, 100, 20, RESO_MIX, 12, RP_CYCLE, 20}, {0, 100, 127, 80}, 0, 0, FX(0, 0, 30, 45)},
    {"HOLLOW", {100, 60, 95, 30, RESO_NOTCH, 30, RP_ALL, 30}, {0, 80, 127, 60}, 0, 0, FX(10, 0, 20, 30)},
    {"SCREAM", {64, 50, 118, 90, RESO_MIX, 10, RP_ALL, 40}, {0, 70, 100, 50}, 0, 0, FX(20, 0, 10, 20)},
};

static const engine_t ENG_RESO = {
    .name = "RESO",
    .page_title = {"MAIN", "MORE"},
    .edit = {
        {"MATR", F_PCT, 0, 127, 10, 0, 0},
        {"SPRD", F_PCT, 0, 127, 40, 0, 0},
        {"RES", F_PCT, 0, 127, 90, 0, 0},
        {"DIRT", F_PCT, 0, 127, 10, 0, 0},
        {"OUT", F_ENUM, 0, 1, 0, N_RESO_OUT, 0},
        {"DCAY", F_PCT, 0, 127, 8, 0, 0},
        {"PING", F_ENUM, 0, RP_N - 1, 0, N_RESO_PING, 0},
        {"FINE", F_PCT, 0, 127, 10, 0, 0},
    },
    .presets = RESO_PRESETS,
    .npresets = NELEM(RESO_PRESETS),
    .note_on = reso_note_on,
    .render = reso_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = RESO_POLY,
    .mem = sizeof(reso_slot_t) * RESO_POLY,
};
