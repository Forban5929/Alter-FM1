/* SPDX-License-Identifier: GPL-3.0-only
 * RESO: four resonant filters in a feedback ring, struck or rubbed (after a QMMF played as a resonator, made dirty).
 *
 *   exciter -> filter 1 -> filter 2 -> filter 3 -> filter 4 -> (back to 1, through a tanh)  ; out: their sum
 *   (each filter also takes the exciter; the ring couples them: they growl, beat, scream)
 *
 * Version 2 (Romain, 01/11: "always the same kind of result"): every knob changes the whole sound, not its first ms.
 * The four main knobs follow the new engines' common grammar (MATTER, SPREAD, RESONANCE, DIRT):
 * MATR  what excites them: 0..42 an impact (an impulse, a pitched click), 43..84 a noise burst (wide: the hit's
 *       body), 85..127 a rubbing that lasts while the note is held (sparse crackle -> full noise): textures
 * The filters sit two octaves above the note (a resonator's zone: C2 rings at C4 and up).
 * SPRD  the four tunings: 0 together (a single huge resonance, beating) -> harmonic (1 2 3 4) -> metallic
 *       (808-like) -> wide and dissonant (to ~3.7 octaves)
 * RES   their Q: a short knock -> a long ring
 * DIRT  the ring's feedback and its drive: 0 four filters apart; up the middle they colour one another (growl); past
 *       ~90 the ring sings by itself (larsen, screams, unstable)
 * OUT   MIX the rings and a part of the exciter (the impact); NTCH the exciter minus the rings (hollow, phase
 *       cancelled); LOW their low-pass outputs (bodies, thuds)
 * DCAY  the exciter's length (impact, burst: 0.2 .. 80 ms; and the sweep's)
 * PING  which filters a hit strikes: ALL, CYCL (one after the other), RAND (one at random), 1..4 (that one)
 * SWEP  the filters start above the note and fall to it at the hit (up to 4 octaves): zaps, kicks, lasers
 * The track's SHP moves MATR, FLT moves the tunings (+-2 octaves), as on every engine: the ADSR on top.
 *
 * Polyphony 2 per part. Fixed point: Q15 audio, Q24 coefficients (once a block: g = tan(pi f / FS) from the sine
 * table, four divisions). */

#define RESO_NF 4
#define RESO_POLY 2
enum { RESO_MIX, RESO_NOTCH, RESO_LOW, RESO_NOUT };
enum { RP_ALL, RP_CYCLE, RP_RAND, RP_1, RP_2, RP_3, RP_4, RP_N };

typedef struct {
    int32_t ic1[RESO_NF], ic2[RESO_NF];
    int32_t o[RESO_NF];          /* each filter's band output a sample ago (the ring) */
    int32_t env;                 /* the exciter's level, Q16 */
    int32_t sw;                  /* the sweep now, octaves Q12 */
    uint32_t eph;                /* the click's phase */
    int32_t rng;
    uint8_t mask;                /* the filters this hit strikes */
} reso_slot_t;

ENG_MEM_FITS("RESO", sizeof(reso_slot_t) * RESO_POLY);

static uint8_t reso_cyc[NPART];          /* CYCL: the filter struck next */

static const char *const N_RESO_OUT[] = {"MIX", "NTCH", "LOW"};
static const char *const N_RESO_PING[] = {"ALL", "CYCL", "RAND", "1", "2", "3", "4"};
/* SPRD's four tunings, octaves Q12 above the note: together (a few cents apart), harmonic, metallic, dissonant */
static const int16_t RESO_SET[4][RESO_NF] = {
    {0, 27, -20, 10},
    {0, 4096, 6492, 8192},
    {0, 2316, 3473, 5531},
    {0, 6000, 9965, 15100},
};

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
    S->sw = t->p[P_E7] * t->p[P_E7] * 16384 / 16129;  /* up to 4 octaves (finer near 0) */
    if (!S->rng)
        S->rng = (int32_t)((v->note + 1u) * 0x9E3779B9u ^ v->age * 0x85EBCA6Bu) | 1;
    if (ping == RP_CYCLE) {
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
    int32_t a[RESO_NF][3], kq, kb, fb, drv, mt, sp, seg, frac, i, rate, outm, rng, env, dirt, gain, dens, rub;
    uint32_t inc[RESO_NF], k, einc, on = 0;
    if (!S)
        return;
    dirt = p[P_E3];
    outm = p[P_E4] % RESO_NOUT;
    /* RES: k = 2 (a knock) .. 0.006 (a long ring) */
    kq = (int32_t)(((int64_t)(127 - p[P_E2]) * (127 - p[P_E2]) << 25) / (127 * 127)) + 100000;
    kb = kq < 4 << 20 ? 4 << 20 : kq;            /* the band outputs scaled by k (unity peak), at least 0.25 */
    /* DIRT: the ring's feedback (0 .. ~2.6, Q12) and the drive into its tanh (1 .. 4, Q12) */
    fb = dirt * dirt * 13000 / 16129;             /* (x 0.6 on itself: 1 at ~92, below it colours, past it sings) */
    drv = 4096 + dirt * 96;
    mt = clamp((p[P_E0] << 8) + m->shape - (64 << 8), 0, 127 << 8) >> 8;   /* MATR (+ SHP) */
    /* SPRD: between two of the four tunings */
    sp = p[P_E1] * 3 * 256 / 127;                /* 0 .. 768: three stretches, Q8 */
    seg = sp >> 8 > 2 ? 2 : sp >> 8;
    frac = sp - seg * 256;
    /* DCAY: the exciter's fall (its time constant, 0.2 .. 80 ms in samples) */
    rate = 10 + p[P_E5] * p[P_E5] * 3500 / 16129;
    for (k = 0; k < RESO_NF; k++) {
        int32_t o = 2 * 4096 + RESO_SET[seg][k] + (RESO_SET[seg + 1][k] - RESO_SET[seg][k]) * frac / 256 + S->sw +
                    m->cutoff * 2 * 4096 / (64 << 8);   /* (FLT: an offset around 0) */
        uint64_t x = ((uint64_t)m->inc * reso_exp2(o)) >> 16;
        if (x > 0x73000000ull)
            x = 0x73000000ull;                   /* (held under 0.45 FS: a sweep from the top stays a filter) */
        inc[k] = (uint32_t)x;
        if (inc[k] < 2000u)
            continue;
        {
            int32_t s = sine_i(inc[k] >> 1), c = sine_i((inc[k] >> 1) + 0x40000000u);
            int32_t g = (int32_t)(((int64_t)s << 24) / (c > 64 ? c : 64));
            int64_t den = (1ll << 24) + (((int64_t)g * (g + kq)) >> 24);
            a[k][0] = (int32_t)((1ll << 48) / den);
            a[k][1] = (int32_t)(((int64_t)g * a[k][0]) >> 24);
            a[k][2] = (int32_t)(((int64_t)g * a[k][1]) >> 24);
        }
        on |= 1u << k;
    }
    S->sw -= S->sw / (5 + p[P_E5] * p[P_E5] * 115 / 16129) + 1;   /* the sweep falls over ~4 .. 90 ms (DCAY) */
    if (S->sw < 0)
        S->sw = 0;
    einc = m->inc * 3u;
    /* the rubbing (MATR past 84): crackle -> full noise, while the note is held */
    rub = mt > 84 ? (v->gate ? 1 : 0) : 0;
    dens = mt > 84 ? (mt - 84) * 1500 : 0;      /* (of 65536: a crackle's chance a sample) */
    /* the level: louder as the rings get thin (high Q) */
    gain = outm == RESO_LOW ? 6 : 12;
    rng = S->rng;
    env = S->env;
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = 0, y = 0, xs = 0, ring;
        if (env > 16) {                          /* the impact or the burst */
            int32_t e = env >> 1, imp, w, nz;
            w = (int32_t)(((int64_t)sine_i(S->eph) * e) >> 15);
            S->eph += einc;
            imp = (e >> 2) + w;                  /* a pitched click on a little thump */
            {
                int32_t q = (imp + 16384) & 65535;   /* (folded: an impact never overflows) */
                imp = (q < 32768 ? q - 16384 : 49151 - q) * 2;
            }
            nz = (int32_t)(((int64_t)((int32_t)(noise32(&rng) >> 16) - 32768) * e) >> 15);
            if (mt < 43)
                x = imp + (int32_t)(((int64_t)(nz - imp) * mt) / 86);   /* (half way to the noise at 42) */
            else
                x = nz + (imp - nz) * (84 - clamp(mt, 43, 84)) / 82;
            env -= env / rate + 1;
        }
        if (rub) {                               /* the rubbing: crackle, then noise */
            int32_t r = (int32_t)(noise32(&rng) >> 16);
            if (dens >= 60000)
                x += (r - 32768) >> 2;
            else if (r < dens)
                x += ((int32_t)(noise32(&rng) >> 17) - 16384);
        }
        if (fb && env <= 16 && !rub)
            x += (int32_t)(noise32(&rng) >> 28) - 8;   /* (a breath: a singing ring starts on its own) */
        ring = S->o[RESO_NF - 1];
        for (k = 0; k < RESO_NF; k++) {
            int32_t v0, v1, v2, v3, *c1 = &S->ic1[k], *c2 = &S->ic2[k], in;
            if (!((on >> k) & 1u)) {
                S->o[k] = 0;
                continue;
            }
            /* this filter's input: the exciter (if struck), itself (0.6) and the one before it (0.4) through the tanh */
            in = (int32_t)(((int64_t)(S->o[k] * 3 + ring * 2) / 5 * drv) >> 12);   /* (harder, the small-signal gain kept) */
            in = softclip(clamp(in, -(1 << 20), 1 << 20));
            in = (int32_t)(((int64_t)in * fb) / drv);
            v0 = ((S->mask >> k) & 1u ? x * 4 : 0) + in;   /* (the exciter x 4: a resonator takes a hard hit) */
            v3 = v0 - *c2;
            v1 = (int32_t)(((int64_t)a[k][0] * *c1 + (int64_t)a[k][1] * v3) >> 24);
            v2 = *c2 + (int32_t)(((int64_t)a[k][1] * *c1 + (int64_t)a[k][2] * v3) >> 24);
            *c1 = clamp(2 * v1 - *c1, -(1 << 21), 1 << 21);
            *c2 = clamp(2 * v2 - *c2, -(1 << 21), 1 << 21);
            ring = (int32_t)(((int64_t)v1 * kb) >> 24);   /* (the next one's input: unity-peak band) */
            S->o[k] = ring;
            if (outm == RESO_LOW)
                y += v2;
            else
                y += ring;
            xs += v0 - (int32_t)(((int64_t)kq * v1) >> 24);
        }
        if (outm == RESO_NOTCH)
            y = xs;
        else if (outm == RESO_MIX)
            y += x >> 3;                         /* (the impact itself: the hit) */
        y = soft_knee(clamp(y, -(1 << 20), 1 << 20) * gain / 4, 22000);
        out[i] += voice_amp(y, m, (uint32_t)i);
    }
    S->env = env;
    S->rng = rng;
}

/* {MATR SPRD RES DIRT OUT DCAY PING SWEP} */
static const preset_t RESO_PRESETS[] = {
    {"ZAP", {10, 50, 90, 30, RESO_MIX, 10, RP_ALL, 90}, {0, 80, 0, 60}, 0, 0, FX(10, 0, 10, 20)},
    {"HOLLOW", {70, 60, 95, 30, RESO_NOTCH, 35, RP_ALL, 0}, {0, 80, 127, 60}, 0, 0, FX(10, 0, 20, 30)},
    {"LARSEN", {20, 10, 110, 115, RESO_MIX, 8, RP_ALL, 0}, {10, 90, 110, 60}, 0, 0, FX(10, 0, 10, 20)},
};

static const engine_t ENG_RESO = {
    .name = "RESO",
    .page_title = {"MAIN", "MORE"},
    .edit = {
        {"MATR", F_PCT, 0, 127, 10, 0, 0},
        {"SPRD", F_PCT, 0, 127, 50, 0, 0},
        {"RES", F_PCT, 0, 127, 90, 0, 0},
        {"DIRT", F_PCT, 0, 127, 30, 0, 0},
        {"OUT", F_ENUM, 0, RESO_NOUT - 1, 0, N_RESO_OUT, 0},
        {"DCAY", F_PCT, 0, 127, 10, 0, 0},
        {"PING", F_ENUM, 0, RP_N - 1, 0, N_RESO_PING, 0},
        {"SWEP", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = RESO_PRESETS,
    .npresets = NELEM(RESO_PRESETS),
    .note_on = reso_note_on,
    .render = reso_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = RESO_POLY,
    .mem = sizeof(reso_slot_t) * RESO_POLY,
};
