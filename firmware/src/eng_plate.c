/* SPDX-License-Identifier: GPL-3.0-only
 * PLATE (Romain 05/11): a struck metal sheet. Four delay lines at inharmonic lengths, mixed into one another every
 * sample (a 4 x 4 Hadamard: every line feeds every other), each one damped: the modes of a plate, a cymbal, a gong,
 * a cracked bell. After the "Crucible" idea (lines, a filter, a drive in the loop).
 *
 *   exciter -> the four lines (+ - + -) -> damping -> Hadamard x RES -> (DIRT: a tanh, a fold) -> back ; out: their sum
 *
 * The four main knobs follow the new engines' grammar (MATTER, SPREAD, RESONANCE, DIRT, as RESO's):
 * MATR  what strikes it: 0..42 a mallet (a soft, dark thump), 43..84 a hard hit (a bright noise burst), 85..127 a
 *       scrape that lasts while the note is held (sparse crackle -> rough noise): sheet metal being rubbed
 * SPRD  the lines' lengths: 0 four close ones (a tuned gong, beating) -> harmonic (1 1/2 1/3 1/4) -> a plate (golden
 *       ratios) -> cracked (wide, dissonant)
 * RES   how long it rings: a dead knock (50 ms) -> a gong that hangs on (10 s), the same at every pitch
 * DIRT  a drive inside the loop: 0 clean; up the middle the ringing thickens (trash metal); past ~90 a fold too: it
 *       breaks up, screams, buzzes
 * TONE  the damping: dark (felt, rust) .. bright (a crash, a hi-hat's sizzle)
 * DCAY  the exciter's length (0.2 .. 80 ms)
 * BEND  the pitch falls after the hit (a gong's swoop, a thunder sheet): 0 none .. an octave and a half
 * HIT   how much of the strike itself is heard over the ring (the click)
 * The track's SHP moves MATR, FLT moves TONE, as on every engine; the ADSR on top (a long SUS keeps it ringing).
 *
 * Monophonic (a new hit strikes the same plate: the ring carries on). The lines in the part's engine memory, 16 bits:
 * 4 x 1536 samples (the note an octave up while its lines would not fit: C1..B1 ring an octave higher). */

#include "eng_dvm.h"

#define PL_NL 4
#define PL_LEN 1536
typedef struct {
    int16_t buf[PL_NL][PL_LEN];
    int32_t lp[PL_NL];           /* each line's damping filter */
    int32_t env;                 /* the exciter's level, Q16 */
    int32_t bend;                /* the pitch bend now, Q12 of the lengths (0: none) */
    int32_t xlp;                 /* the exciter's own low-pass (the mallet) */
    int32_t dc, dcx;             /* the output's DC blocker */
    int32_t rng;
    uint16_t w;
} plate_t;

ENG_MEM_FITS("PLATE", sizeof(plate_t));

/* SPRD's four sets: each line's length against the note's period, Q12 */
static const int16_t PL_SET[4][PL_NL] = {
    {4096, 4125, 4067, 4149},    /* close: a tuned gong, beating */
    {4096, 2048, 1365, 1024},    /* harmonic: 1 1/2 1/3 1/4 */
    {4096, 2531, 1696, 1257},    /* a plate: 1 0.618 0.414 0.307 */
    {4096, 3444, 2435, 1218},    /* cracked: 1 0.841 0.595 0.297 */
};

static plate_t *plate_of(track_t *t)
{
    return t >= &trk[0] && t < &trk[NPART] ? (plate_t *)ENG_MEM(t - trk) : 0;
}

static void plate_note_on(track_t *t, voice_t *v)
{
    plate_t *S = plate_of(t);
    if (!S)
        return;
    S->env = 65536;
    S->xlp = 0;
    S->bend = t->p[P_E6] * t->p[P_E6] * 6144 / 16129;   /* (BEND: up to 1.5 x the lengths' worth, finer near 0) */
    if (!S->rng)
        S->rng = (int32_t)((v->note + 1u) * 0x9E3779B9u) | 1;
}

/* the line k's sample d samples ago (Q16: linear between two) */
static inline int32_t pl_read(const int16_t *b, uint32_t w, uint32_t d16)
{
    uint32_t di = d16 >> 16, f = d16 & 65535u;
    int32_t i0 = (int32_t)w - (int32_t)di, i1 = i0 - 1, a, c;
    if (i0 < 0)
        i0 += PL_LEN;
    if (i1 < 0)
        i1 += PL_LEN;
    a = b[i0];
    c = b[i1];
    return a + (int32_t)(((int64_t)(c - a) * (int32_t)f) >> 16);
}

static void plate_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    plate_t *S = plate_of(t);
    const int16_t *p = t->p;
    uint32_t len[PL_NL], k, i, per16, w;
    int32_t gk[PL_NL], mt, sp, seg, frac, g, drv, dirt, fold, tone, rate, hit, env, rng, xa, xg;
    uint32_t mdrv, mrate;                        /* (v0.75: the divisions as multiplies, eng_dvm.h) */
    if (!S)
        return;
    /* the note's period in samples, Q16 (2^48 / inc), an octave up while the longest line would not fit */
    per16 = m->inc > 0x20000u ? (uint32_t)((1ull << 48) / m->inc) : (uint32_t)(PL_LEN - 8) << 16;
    while (per16 > (uint32_t)(PL_LEN - 8) << 15 && per16 > (64u << 16))   /* (x 1.013 and the bend: half the line) */
        per16 >>= 1;
    mt = clamp((p[P_E0] << 8) + m->shape - (64 << 8), 0, 127 << 8) >> 8;   /* MATR (+ SHP) */
    sp = p[P_E1] * 3 * 256 / 127;                /* SPRD: between two of the four sets, Q8 */
    seg = sp >> 8 > 2 ? 2 : sp >> 8;
    frac = sp - seg * 256;
    for (k = 0; k < PL_NL; k++) {
        int32_t r = PL_SET[seg][k] + (PL_SET[seg + 1][k] - PL_SET[seg][k]) * frac / 256;
        uint64_t d = ((uint64_t)per16 * (uint32_t)r) >> 12;
        d -= (d * (uint32_t)S->bend) >> 13;      /* (BEND: shorter at the hit, the pitch higher, falling back) */
        if (d < (4u << 16))
            d = 4u << 16;
        if (d > (uint64_t)(PL_LEN - 3) << 16)
            d = (uint64_t)(PL_LEN - 3) << 16;
        len[k] = (uint32_t)d;
    }
    S->bend -= S->bend / 180 + 1;                /* (the bend falls back over ~0.5 s) */
    if (S->bend < 0)
        S->bend = 0;
    /* RES: how long it rings, 50 ms .. 10 s to -60 dB (x 200: 7.64 octaves), the same at every pitch: each line's
     * gain from its own length (60 dB / 6.02 = 9.97 octaves of loss over the ring, a line's share of it) */
    {
        int32_t n60 = (int32_t)(((int64_t)2205 * reso_exp2(p[P_E2] * 31293 / 127)) >> 16);
        for (k = 0; k < PL_NL; k++) {
            int32_t o = (int32_t)((int64_t)40820 * (int32_t)(len[k] >> 16) / (n60 > 1 ? n60 : 1));
            gk[k] = (int32_t)(reso_exp2(-(o > 60000 ? 60000 : o)) >> 1);
            if (gk[k] > 32700)
                gk[k] = 32700;
        }
    }
    g = 0;
    dirt = p[P_E3];
    drv = 4096 + dirt * 64;                      /* DIRT: the drive into the loop's tanh, 1 .. 3, Q12 */
    fold = dirt > 90 ? (dirt - 90) * 700 : 0;    /* (past 90: a fold as well, Q15 of the signal) */
    tone = clamp(p[P_E4] * 256 + m->cutoff, 0, 127 << 8);   /* TONE (+ FLT, an offset around 0) */
    tone = 1200 + tone;                          /* (the damping low-pass, Q15: ~250 Hz .. nearly none) */
    if (tone > 32767)
        tone = 32767;
    rate = 10 + p[P_E5] * p[P_E5] * 3500 / 16129;   /* DCAY: the exciter's time constant, samples */
    hit = p[P_E7];
    xa = mt < 43 ? 900 + mt * 200 : 32767;        /* the mallet: a dark burst; harder above 42 */
    xg = 64 + 16384 / (rate + 16) + (32767 - xa) / 90;   /* (the exciter's gain, Q6: short and dark strikes louder) */
    mdrv = dvm_make((uint32_t)drv, 42u);         /* (|z << 12| < 2^27, drv < 2^14: 2^41 < 2^42) */
    mrate = dvm_make((uint32_t)rate, 32u);       /* (env < 2^17, rate <= 3510: < 2^29) */
    env = S->env;
    rng = S->rng;
    w = S->w;
    for (i = 0; i < n; i++) {
        int32_t x = 0, s[PL_NL], h0, h1, h2, h3, y;
        if (env > 16) {                           /* the strike: noise, shaped */
            int32_t nz = (int32_t)(noise32(&rng) >> 16) - 32768;
            x = (int32_t)(((int64_t)nz * (env >> 1)) >> 15);
            env -= dvm_div(env, mrate, 32u) + 1;
        }
        if (mt > 84 && v->gate) {                 /* the scrape: crackle, then rough noise, while held */
            int32_t r = (int32_t)(noise32(&rng) >> 16), dens = (mt - 84) * 1500;
            if (dens >= 60000)
                x += (r - 32768) >> 2;
            else if (r < dens)
                x += (int32_t)(noise32(&rng) >> 17) - 16384;
        }
        x = clamp((int32_t)(((int64_t)x * xg) >> 6), -32767, 32767);   /* (a short strike as strong as a long one) */
        S->xlp += (int32_t)(((int64_t)(x - S->xlp) * xa) >> 15);
        x = S->xlp;
        for (k = 0; k < PL_NL; k++) {             /* the lines out, damped */
            int32_t r = pl_read(S->buf[k], w, len[k]);
            S->lp[k] += (int32_t)(((int64_t)(r - S->lp[k]) * tone) >> 15);
            s[k] = S->lp[k];
        }
        y = s[0] - s[1] + s[2] - s[3];
        /* the Hadamard (orthogonal, / 2), x RES */
        h0 = (s[0] + s[1] + s[2] + s[3]) >> 1;
        h1 = (s[0] - s[1] + s[2] - s[3]) >> 1;
        h2 = (s[0] + s[1] - s[2] - s[3]) >> 1;
        h3 = (s[0] - s[1] - s[2] + s[3]) >> 1;
        s[0] = h0, s[1] = h1, s[2] = h2, s[3] = h3;
        if (++w >= PL_LEN)
            w = 0;
        for (k = 0; k < PL_NL; k++) {
            int32_t z = (int32_t)(((int64_t)s[k] * gk[k]) >> 15) + ((k & 1u) ? -x : x);
            if (dirt) {                           /* DIRT: a tanh in the loop (the small-signal gain kept) */
                z = (int32_t)(((int64_t)z * drv) >> 12);
                z = softclip(clamp(z, -65535, 65535));
                z = dvm_div(z << 12, mdrv, 42u);   /* (v0.75: as a multiply; v0.59: no 64-bit division) */
                if (fold) {                       /* past 90: folded back over a lower ceiling */
                    int32_t c = 32767 - fold, a = z < 0 ? -z : z;
                    if (a > c)
                        a = 2 * c - a;
                    z = z < 0 ? -a : a;
                }
            }
            S->buf[k][w] = (int16_t)clamp(z, -32767, 32767);
        }
        y = y * 3 + (int32_t)(((int64_t)x * hit) >> 7);   /* the ring (x 6: a strike's energy spreads over 6000 samples) and a part of the strike (HIT) */
        S->dc = y - S->dcx + (int32_t)(((int64_t)S->dc * 32604) >> 15);   /* the DC blocker (~8 Hz) */
        S->dcx = y;
        out[i] += voice_amp(soft_knee(clamp(S->dc * 13 / 16, -(1 << 20), 1 << 20), 22000), m, i);   /* (-1.8 dB, Romain 05/11) */
    }
    S->env = env;
    S->rng = rng;
    S->w = (uint16_t)w;
}

/* {MATR SPRD RES DIRT TONE DCAY BEND HIT} (ALTER's sounds: alter_sounds.c ALT_PRE) */
static const preset_t PLATE_PRESETS[] = {
    {"GONG", {20, 15, 115, 10, 70, 20, 50, 10}, {0, 110, 127, 110}, 0, 0, FX(0, 0, 10, 30)},
};

static const engine_t ENG_PLATE = {
    .name = "PLATE",
    .page_title = {"MAIN", "MORE"},
    .edit = {
        {"MATR", F_PCT, 0, 127, 20, 0, 0},
        {"SPRD", F_PCT, 0, 127, 64, 0, 0},
        {"RES", F_PCT, 0, 127, 100, 0, 0},
        {"DIRT", F_PCT, 0, 127, 0, 0, 0},
        {"TONE", F_PCT, 0, 127, 80, 0, 0},
        {"DCAY", F_PCT, 0, 127, 15, 0, 0},
        {"BEND", F_PCT, 0, 127, 0, 0, 0},
        {"HIT", F_PCT, 0, 127, 20, 0, 0},
    },
    .presets = PLATE_PRESETS,
    .npresets = NELEM(PLATE_PRESETS),
    .note_on = plate_note_on,
    .render = plate_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = 1,
    .mem = sizeof(plate_t),
};
