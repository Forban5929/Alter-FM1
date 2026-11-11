/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER: six oscillators played as one struck voice, after the Basimilus Iteritas Alter.
 *
 * MRPH  the shape of all six: sine -> triangle -> saw -> square (a linear crossfade between neighbours)
 * SPRD  their frequency ratios: 0 harmonic (1..6), 64 metallic (808-cymbal-like), 127 a free bar (wide, dissonant)
 * HARM  how many sound: 0 the lowest alone, 127 all six at full level (and the upper ones decay slower)
 * MODE  SKIN: added together; LIQUID: a fast falling pitch sweep at each hit; METAL: each modulates the
 *       phase of the one below (6 -> 5 -> .. -> 1)
 * AMNT  per MODE (alt_desc): SKIN DTUN (a slight detune, beating), LIQUID PNCH (the sweep's depth and length),
 *       METAL FM (the modulation depth)
 * NOIS  a white-noise burst at the hit (0 = none)
 * TILT  centre: all six follow the ADSR; left: the upper ones die first (bright -> dark); right: the lower
 *       ones die first (thin, ringing)
 * DRFT  a slow random wander of each oscillator's pitch, up to about a semitone
 * The track's SHP moves MRPH, FLT moves HARM. Amplitude: the track's ADSR, as every engine.
 *
 * Polyphony 3 per part (engine_t.poly): a hit rings on under the next, a chord sounds whole. Each voice 0..2 has a
 * state slot in the part's engine memory (engines.c eng_mem): the six phases, decays and drifts.
 * Fixed point only: phases are 32-bit turns, levels Q15, decays Q16; ratios and coefficients once a block. */

#define ALT_NOSC 6
#define ALT_POLY 3
enum { ALT_SKIN, ALT_LIQUID, ALT_METAL };

typedef struct {
    uint32_t ph[ALT_NOSC];
    int32_t env[ALT_NOSC];       /* each oscillator's own decay, Q16 (65536 = full) */
    int32_t drift[ALT_NOSC];     /* pitch offset now, 1/4096 of the frequency */
    int32_t dtgt[ALT_NOSC];      /* and where it is heading */
    int32_t glide;               /* LIQUID: pitch above the note, 1/16 semitone Q8 */
    int32_t nenv;                /* the noise burst's level, Q16 */
    int32_t rng;                 /* xorshift state */
    int32_t dcnt;                /* blocks to the next drift target */
} alt_slot_t;

ENG_MEM_FITS("ALTER", sizeof(alt_slot_t) * ALT_POLY);

static const char *const N_ALT_MODE[] = {"SKIN", "LIQD", "METL"};
static const param_desc_t ALT_D_DTUN = {"DTUN", F_PCT, 0, 127, 20, 0, 0};
static const param_desc_t ALT_D_PNCH = {"PNCH", F_PCT, 0, 127, 20, 0, 0};
static const param_desc_t ALT_D_FM = {"FM", F_PCT, 0, 127, 20, 0, 0};

/* frequency ratios, Q16: harmonic, metallic (the six square oscillators of a TR-808 cymbal, over the lowest),
 * a free-free bar's modes (wide and dissonant) */
static const int32_t ALT_RATIO[3][ALT_NOSC] = {
    {65536, 131072, 196608, 262144, 327680, 393216},
    {65536, 97175, 117964, 166824, 172360, 255406},
    {65536, 180617, 354156, 585436, 874512, 1221465},
};
/* SKIN's detune pattern: each oscillator's share of DTUN (1/4096 of its frequency at DTUN 127, x 20) */
static const int8_t ALT_DET[ALT_NOSC] = {0, 2, -2, 3, -3, 1};

static const param_desc_t *alt_desc(const track_t *t, uint32_t k)
{
    if (k != 4u)
        return 0;
    switch (t->p[P_E3]) {
    case ALT_LIQUID: return &ALT_D_PNCH;
    case ALT_METAL: return &ALT_D_FM;
    default: return &ALT_D_DTUN;
    }
}

static uint32_t alt_isqrt(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x)
        b >>= 2;
    while (b) {
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}

static alt_slot_t *alt_slot_of(track_t *t, voice_t *v)
{
    uint32_t i;
    if (t < &trk[0] || t >= &trk[NPART])
        return 0;
    i = (uint32_t)(v - t->v);
    return i < ALT_POLY ? (alt_slot_t *)ENG_MEM(t - trk) + i : 0;
}

static void alt_note_on(track_t *t, voice_t *v)
{
    alt_slot_t *S = alt_slot_of(t, v);
    uint32_t k, seed = (v->note + 1u) * 0x9E3779B9u ^ v->age * 0x85EBCA6Bu;
    int32_t a = t->p[P_E4], nz = t->p[P_E5];
    if (!S)
        return;
    if (v->env_out > 512) {                             /* a voice still sounding taken again (a DRONE's layers): */
        for (k = 0; k < ALT_NOSC; k++)                  /* its phases and decays kept, no sweep, no burst (a click) */
            S->env[k] = S->env[k] > 32768 ? S->env[k] : 32768;
        S->glide = 0;
        return;
    }
    for (k = 0; k < ALT_NOSC; k++) {
        S->ph[k] = 0;                                   /* all in phase: a hard, repeatable attack */
        S->env[k] = 65536;
    }
    S->glide = t->p[P_E3] == ALT_LIQUID ? (a * 6) << 8 : 0;   /* up to ~48 semitones above */
    S->nenv = (nz * nz * 65536) / (127 * 127);
    if (!S->rng)
        S->rng = (int32_t)(seed ? seed : 1u);
}

static void alt_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    alt_slot_t *S = alt_slot_of(t, v);
    const int16_t *p = t->p;
    uint32_t inc[ALT_NOSC], ph[ALT_NOSC], inc0, mode, i, k, seg;
    int32_t w[ALT_NOSC], ratio[ALT_NOSC], lvl[ALT_NOSC];
    int32_t mm, q, frac, sp, hh, posq, tilt, amt, L = 0, g, nlvl, nrate, fmd = 0, rng;
    if (!S)
        return;
    mode = (uint32_t)p[P_E3] % 3u;
    amt = p[P_E4];
    /* MRPH (+ SHP): the segment (0 sine/tri, 1 tri/saw, 2 saw/square) and the crossfade in it, Q15 */
    mm = clamp((p[P_E0] << 8) + m->shape - (64 << 8), 0, 127 << 8);
    q = (mm * 3 * 128) / 127;                           /* 0 .. 3 << 15 */
    seg = (uint32_t)(q >> 15);
    frac = q & 32767;
    if (seg > 2u) {
        seg = 2u;
        frac = 32767;
    }
    /* SPRD: the ratios, harmonic -> metallic (0..64) -> bar (64..127) */
    sp = p[P_E1];
    for (k = 0; k < ALT_NOSC; k++) {
        const int32_t *a0 = sp < 64 ? ALT_RATIO[0] : ALT_RATIO[1], *a1 = sp < 64 ? ALT_RATIO[1] : ALT_RATIO[2];
        int32_t f = sp < 64 ? sp * 512 : (sp - 64) * 520;   /* Q15 */
        ratio[k] = a0[k] + ((((a1[k] - a0[k]) >> 6) * f) >> 9);
    }
    /* HARM (+ FLT): oscillator k comes in as the knob passes k - 1 .. k (in fifths of its travel) */
    hh = clamp((p[P_E2] << 8) + m->cutoff, 0, 127 << 8);
    posq = hh * 5 / 127;                                /* 0 .. 1280: 0 .. 5 oscillators above the lowest, Q8 */
    lvl[0] = 32767;
    for (k = 1; k < ALT_NOSC; k++)
        lvl[k] = clamp(posq - (int32_t)(k - 1u) * 256, 0, 256) * 128;
    /* TILT: each oscillator's decay a block, Q16; HARM slows the upper ones' (up to half) */
    tilt = p[P_E6];
    for (k = 0; k < ALT_NOSC; k++) {
        int32_t r = 0;
        if (tilt < 0)
            r = ((int32_t)k * tilt * tilt * 1600) / (5 * 64 * 64);
        else if (tilt > 0)
            r = ((int32_t)(5u - k) * tilt * tilt * 1600) / (5 * 63 * 63);
        if (k)
            r = (r * (256 - (hh >> 8))) >> 8;
        S->env[k] -= (S->env[k] * r) >> 16;
    }
    /* DRFT: a new random target for each oscillator every 0.2 .. 1 s, followed with a lag */
    rng = S->rng;
    if (p[P_E7]) {
        int32_t range = p[P_E7] * p[P_E7] * 245 / (127 * 127);   /* up to ~6 % (a semitone) */
        if (--S->dcnt <= 0) {
            for (k = 0; k < ALT_NOSC; k++)
                S->dtgt[k] = (int32_t)(noise32(&rng) % (uint32_t)(2 * range + 1)) - range;
            S->dcnt = FE_T(280 + (int32_t)(noise32(&rng) % 1100u));
        }
    } else {
        for (k = 0; k < ALT_NOSC; k++)
            S->dtgt[k] = 0;
    }
    for (k = 0; k < ALT_NOSC; k++)
        S->drift[k] += (S->dtgt[k] - S->drift[k]) >> 5;
    /* the base pitch: LIQUID's sweep above the note, falling fast (4 .. 40 ms) */
    if (S->glide > 0) {
        int32_t tau = 6 + amt * amt / 320;              /* blocks */
        inc0 = cents_inc(m->pitch16 + (S->glide >> 8), 0, m->fine);
        S->glide -= S->glide / tau + 1;
        if (S->glide < 0)
            S->glide = 0;
    } else {
        inc0 = m->inc;
    }
    /* each oscillator's increment; one above 0.43 FS (or past the top of a turn) is left silent */
    for (k = 0; k < ALT_NOSC; k++) {
        uint64_t x = ((uint64_t)inc0 * (uint32_t)ratio[k]) >> 16;
        int32_t d = S->drift[k] + (mode == ALT_SKIN ? ALT_DET[k] * amt * 20 / 127 : 0);
        x += (uint64_t)((int64_t)(x >> 12) * d);
        if (x > 0x6E000000ull) {
            inc[k] = 0;
            w[k] = 0;
        } else {
            inc[k] = (uint32_t)x;
            w[k] = (lvl[k] * (S->env[k] >> 1)) >> 15;   /* Q15 */
            L += lvl[k];
        }
        ph[k] = S->ph[k];
    }
    /* the level: the sum of the oscillators heard, normalised by sqrt of their count (in-phase peaks go to
     * the knee) */
    g = (int32_t)((32767u * 16u) / alt_isqrt(((uint32_t)(L > 32767 ? L : 32767) + 64u) >> 7));
    if (mode == ALT_METAL)
        fmd = amt * amt * 8;                            /* the phase offset per unit of the one above: up to ~2 turns */
    /* the noise burst: longer as NOIS rises (tau ~23 .. 40 ms) */
    nlvl = S->nenv;
    nrate = 2600 - p[P_E5] * 10;
    /* (v0.58, measured on the FM-1: one ALTER circle took up to half a block) only the oscillators heard: one at level 0
     * adds nothing, and outside METAL nothing reads it (its phase is moved on after the block); and at a crossfade's
     * start (frac 0) only its first wave. The same samples, bit for bit */
    uint32_t ks[ALT_NOSC], nk = 0, j;
    for (k = ALT_NOSC; k-- > 0;)
        if (fmd || w[k])
            ks[nk++] = k;
    for (i = 0; i < n && !fmd && !frac; i++) {          /* (the common case: no FM, a pure wave) */
        int32_t acc = 0, y;
        for (j = 0; j < nk; j++) {
            uint32_t kk = ks[j], pk = ph[kk], ik = inc[kk];
            int32_t o = seg == 0u ? sine_i(pk) : seg == 1u ? osc_tri(pk + 0x40000000u) : osc_saw(pk + 0x80000000u, ik);
            ph[kk] += ik;
            acc += (o * w[kk]) >> 15;
        }
        y = ((acc >> 4) * g) >> 11;
        if (nlvl > 64) {
            int32_t r = (int32_t)(noise32(&rng) >> 17) - 16384;
            y += (r * (nlvl >> 1)) >> 14;
        }
        y = soft_knee(y, 20000);
        out[i] += voice_amp(y, m, i);
    }
    for (; i < n; i++) {
        int32_t acc = 0, mod = 0, y;
        for (j = 0; j < nk; j++) {
            uint32_t kk = ks[j], pk = ph[kk] + (uint32_t)mod, ik = inc[kk];
            int32_t a, b, o;
            k = kk;
            switch (seg) {
            case 0u:
                a = sine_i(pk);
                b = osc_tri(pk + 0x40000000u);
                break;
            case 1u:
                a = osc_tri(pk + 0x40000000u);
                b = osc_saw(pk + 0x80000000u, ik);
                break;
            default:
                a = osc_saw(pk + 0x80000000u, ik);
                b = osc_pulse(pk + 0x80000000u, ik, 0x80000000u);
                break;
            }
            o = a + (((b - a) * frac) >> 15);
            ph[k] += ik;
            acc += (o * w[k]) >> 15;
            if (fmd)
                mod = mul_wrap((o * (S->env[k] >> 1)) >> 15, fmd);   /* the next one down: by this one's level */
        }
        y = ((acc >> 4) * g) >> 11;
        if (nlvl > 64) {
            int32_t r = (int32_t)(noise32(&rng) >> 17) - 16384;
            y += (r * (nlvl >> 1)) >> 14;
        }
        y = soft_knee(y, 20000);
        out[i] += voice_amp(y, m, i);
    }
    for (k = 0; k < ALT_NOSC; k++)                      /* (the oscillators not run: their phase on, as if run) */
        if (!fmd && !w[k])
            ph[k] += inc[k] * n;
    if (nlvl > 64)
        nlvl -= (nlvl >> 4) * nrate >> 12;
    S->nenv = nlvl;
    S->rng = rng;
    for (k = 0; k < ALT_NOSC; k++)
        S->ph[k] = ph[k];
}

/* {MRPH, SPRD, HARM, MODE, AMNT, NOIS, TILT, DRFT} */
static const preset_t ALTER_PRESETS[] = {
    {"THUD", {0, 0, 8, ALT_LIQUID, 90, 15, -40, 0}, {0, 60, 0, 45}, 0, 0, FX(10, 0, 0, 10)},
    {"BELL", {8, 70, 90, ALT_SKIN, 15, 0, 30, 4}, {0, 95, 0, 90}, 0, 0, FX(0, 0, 25, 50)},
    {"CLANG", {30, 100, 75, ALT_METAL, 60, 25, 0, 0}, {0, 72, 0, 60}, 0, 0, FX(15, 0, 20, 35)},
    {"SNAP", {20, 40, 50, ALT_LIQUID, 40, 110, -30, 0}, {0, 45, 0, 35}, 0, 0, FX(20, 0, 0, 25)},
    {"STAB", {110, 15, 80, ALT_SKIN, 40, 10, -45, 0}, {0, 55, 0, 40}, 0, 0, FX(25, 0, 30, 20)},
    {"RUST", {70, 120, 110, ALT_METAL, 100, 0, 10, 60}, {30, 127, 127, 85}, 0, 0, FX(30, 0, 30, 60)},
};

static const engine_t ENG_ALTER = {
    .name = "ALTER",
    .page_title = {"TONE", "MOVE"},
    .edit = {
        {"MRPH", F_PCT, 0, 127, 0, 0, 0},
        {"SPRD", F_PCT, 0, 127, 0, 0, 0},
        {"HARM", F_PCT, 0, 127, 40, 0, 0},
        {"MODE", F_ENUM, 0, 2, 0, N_ALT_MODE, 0},
        {"AMNT", F_PCT, 0, 127, 20, 0, 0},               /* named by MODE (alt_desc) */
        {"NOIS", F_PCT, 0, 127, 0, 0, 0},
        {"TILT", F_BIPCT, -64, 63, 0, 0, 0},
        {"DRFT", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = ALTER_PRESETS,
    .npresets = NELEM(ALTER_PRESETS),
    .note_on = alt_note_on,
    .render = alt_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = ALT_POLY,
    .desc = alt_desc,
    .mem = sizeof(alt_slot_t) * ALT_POLY,
};
