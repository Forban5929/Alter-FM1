/* SPDX-License-Identifier: GPL-3.0-only
 * RING (Romain 05/11): two oscillators multiplied (a ring modulator), through a resonant filter and a valve's
 * asymmetric drive, with a radio's troubles on top (static, a carrier that wanders, fading). Bells, sirens, robots,
 * broken shortwave.
 *
 *   osc A (the note) x osc B (SPRD's ratio, SWEP's fall) -> BLND with A -> filter (TONE, RES) -> valve (DIRT) -> RADIO
 *
 * The four main knobs follow the new engines' grammar (MATTER, SPREAD, RESONANCE, DIRT):
 * MATR  the two waves: sine -> triangle -> saw -> square -> a square whose width the noise shakes (rough)
 * SPRD  B against A: 0 a hair apart (slow beating, a throb) -> a fifth, an octave -> inharmonic ratios (bells,
 *       metal) -> far apart (a robot, a siren's whine)
 * RES   the filter's resonance: a wide colour -> a sharp, whistling peak
 * DIRT  a valve: 0 clean; up the middle warm, then torn (asymmetric: even harmonics, a growl)
 * RADIO the radio's troubles: 0 none; static crackle, the level fading in and out, B's carrier drifting off tune
 * TONE  where the filter sits: a multiple of the note, low (a muffled hum) .. high (a hiss, a whistle)
 * BLND  the ring alone (0: no fundamental, only the sum and difference) .. A over it (127: the note under the ring)
 * SWEP  B starts high and falls to its ratio at each hit (up to 3 octaves): zaps, lasers, dropping bombs
 * The track's SHP moves MATR, FLT moves TONE; the ADSR on top. Three voices per part, no engine memory (the voice's
 * own state: ph[0] A, ph[1] B; s[0..1] the filter, s[2] the sweep, s[3] the fading, s[4] the drift's
 * value, s[5] the noise, s[6] the DC). Not band-limited: its edges alias, on purpose (it is a radio). */

#define RING_POLY 3

static void ring_note_on(track_t *t, voice_t *v)
{
    v->s[2] = t->p[P_E7] * t->p[P_E7] * 12288 / 16129;   /* SWEP: B up to 3 octaves above (Q12 octaves) */
    if (!v->s[5])
        v->s[5] = (int32_t)((v->note + 1u) * 0x9E3779B9u ^ (uint32_t)(v - t->v) * 0x85EBCA6Bu) | 1;
    v->s[3] = 32767;
}

/* 2^(o / 4096), Q16 (a quadratic between the powers) */
static uint32_t ring_exp2(int32_t o)
{
    int32_t n = o >> 12, f = o & 4095;
    uint32_t w = 65536u + (uint32_t)((f * (2690 + ((f * 1406) >> 12))) >> 12) * 16u;
    return n >= 0 ? w << n : w >> -n;
}

#include "eng_dvm.h"

/* the wave at phase ph: MATR (0..127 << 8) from sine to a shaken square (pw: the shaken width) */
static inline int32_t ring_wave(uint32_t ph, int32_t mt, uint32_t pw)
{
    int32_t a, b, seg = mt >> 13, fr = mt & 8191;   /* (four stretches of 32; v0.75: only the two shapes it mixes) */
    switch (seg) {
    case 0: a = sine_i(ph), b = osc_tri(ph); break;
    case 1: a = osc_tri(ph), b = (int32_t)(ph >> 16) - 32768; break;
    case 2: a = (int32_t)(ph >> 16) - 32768, b = ph < 0x80000000u ? 24000 : -24000; break;
    default: a = ph < 0x80000000u ? 24000 : -24000, b = ph < pw ? 24000 : -24000; break;
    }
    return a + (int32_t)(((int64_t)(b - a) * fr) >> 13);
}

/* SPRD's ratios (B / A, Q12 octaves above A), between which the knob moves */
static const int16_t RING_SPR[8] = {12, 300, 2397, 4096, 5984, 7453, 9834, 11660};
/*                                 (a hair, a semitone-ish, a fifth, an octave, 2.76 bell, 3.53, 5.27, 7.2) */

static void ring_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t mt, sp, seg, frac, ob, q, fq, dirt, bias, sbias, dd, radio, blnd, rng, i, lo, bd;
    uint32_t inca, incb, incf, pw, mdd;
    mt = clamp((p[P_E0] << 8) + m->shape - (64 << 8), 0, (127 << 8) - 1);   /* MATR (+ SHP) */
    sp = p[P_E1] * 7 * 256 / 127;                /* SPRD: between two of the eight ratios, Q8 */
    seg = sp >> 8 > 6 ? 6 : sp >> 8;
    frac = sp - seg * 256;
    ob = RING_SPR[seg] + (RING_SPR[seg + 1] - RING_SPR[seg]) * frac / 256 + v->s[2];
    radio = p[P_E4] * 70 / 127;                  /* RDIO: the knob at full = 70 (Romain 05/11: more was too much) */
    rng = v->s[5];
    if (radio) {                                 /* the carrier wanders (a random walk, slow) */
        v->s[4] += (int32_t)((noise32(&rng) >> 22) & 1023) - 512;
        v->s[4] -= v->s[4] / 64;
        ob += (int32_t)(((int64_t)v->s[4] * radio) >> 9);
    }
    inca = m->inc;
    incb = (uint32_t)(((uint64_t)m->inc * ring_exp2(ob)) >> 16);
    v->s[2] -= v->s[2] / 60 + 1;                 /* (the sweep falls over ~0.2 s) */
    if (v->s[2] < 0)
        v->s[2] = 0;
    /* the filter: a Chamberlin state variable at TONE x the note (up to ~7 kHz), Q from RES */
    {
        int32_t to = clamp(p[P_E5] * 256 + m->cutoff, 0, 127 << 8);
        uint64_t fc = ((uint64_t)m->inc * ring_exp2((int32_t)((int64_t)to * 5 * 4096 / (127 << 8)))) >> 16;   /* (x 1 .. 32) */
        if (fc > 0x0A000000ull)
            fc = 0x0A000000ull;                  /* (~6.9 kHz: the Chamberlin stays stable) */
        incf = (uint32_t)fc;
    }
    fq = sine_i(incf >> 1) * 2;                  /* f = 2 sin(pi fc / FS), Q15 */
    q = 32768 * 2 - p[P_E2] * 62000 / 127;       /* RES: damping 2 .. ~0.05, Q15 */
    if (q < 1600)
        q = 1600;
    dirt = p[P_E3];
    bias = dirt * 60;                            /* (the valve's bias: asymmetric, even harmonics) */
    sbias = softclip(bias);                      /* (v0.59: once a block) */
    dd = 4096 + dirt * 40;
    mdd = dvm_make((uint32_t)dd, 42u);           /* (v0.75: |x << 12| < 2^28, dd < 2^14) */
    blnd = p[P_E6];
    lo = v->s[0];
    bd = v->s[1];
    pw = 0x80000000u;
    for (i = 0; i < (int32_t)n; i++) {
        int32_t a, b, y, hi, x;
        if (mt >= 96 << 8)                       /* (the shaken square: its width jumps with the noise) */
            pw = 0x80000000u + (uint32_t)((int32_t)(noise32(&rng) >> 8) - (1 << 23)) * (uint32_t)((mt >> 8) - 95) * 8u;
        a = ring_wave(v->ph[0], mt, pw);
        b = ring_wave(v->ph[1], mt, pw);
        v->ph[0] += inca;
        v->ph[1] += incb;
        y = (int32_t)(((int64_t)a * b) >> 15);   /* the ring */
        y += (int32_t)(((int64_t)(a - y) * blnd) >> 8);   /* BLND: A over it */
        hi = y - lo - (int32_t)(((int64_t)q * bd) >> 15);   /* the filter */
        bd += (int32_t)(((int64_t)fq * hi) >> 15);
        lo += (int32_t)(((int64_t)fq * bd) >> 15);
        bd = clamp(bd, -(1 << 19), 1 << 19);
        lo = clamp(lo, -(1 << 19), 1 << 19);
        x = (lo + bd + (y >> 2)) >> 1;           /* (the low and the band: a resonant low-pass with some edge) */
        if (dirt) {                              /* the valve: driven, biased, its own bias taken off */
            int32_t d = 4096 + dirt * 120;
            x = softclip(clamp((int32_t)(((int64_t)x * d) >> 12) + bias, -65535, 65535)) - sbias;
            x = dvm_div(x << 12, mdd, 42u);      /* (v0.75: as a multiply; v0.59: no 64-bit division) */
        }
        if (radio) {                             /* static and fading */
            int32_t r = (int32_t)(noise32(&rng) >> 16);
            if (r < radio * 6)
                x += ((int32_t)(noise32(&rng) >> 17) - 16384) * radio / 127;   /* (a crackle) */
            if ((i & 31) == 0) {                 /* (the fading, a slow wobble) */
                int32_t tg = 32767 - (int32_t)((noise32(&rng) >> 17) * (uint32_t)radio / 160u);
                v->s[3] += (tg - v->s[3]) >> 4;
            }
            x = (int32_t)(((int64_t)x * v->s[3]) >> 15);
        }
        v->s[6] += (x - v->s[6]) >> 10;          /* (the DC out) */
        x -= v->s[6];
        out[i] += voice_amp(soft_knee(clamp(x * 2, -(1 << 20), 1 << 20), 22000), m, (uint32_t)i);
    }
    v->s[0] = lo;
    v->s[1] = bd;
    v->s[5] = rng;
}

/* {MATR SPRD RES DIRT RADIO TONE BLND SWEP} (ALTER's sounds: alter_sounds.c ALT_PRE) */
static const preset_t RING_PRESETS[] = {
    {"RADIO", {40, 50, 60, 30, 13, 70, 20, 0}, {0, 100, 110, 60}, 0, 0, FX(0, 0, 10, 20)},
};

static const engine_t ENG_RING = {
    .name = "RING",
    .page_title = {"MAIN", "MORE"},
    .edit = {
        {"MATR", F_PCT, 0, 127, 40, 0, 0},
        {"SPRD", F_PCT, 0, 127, 50, 0, 0},
        {"RES", F_PCT, 0, 127, 60, 0, 0},
        {"DIRT", F_PCT, 0, 127, 30, 0, 0},
        {"RDIO", F_PCT, 0, 127, 0, 0, 0},
        {"TONE", F_PCT, 0, 127, 70, 0, 0},
        {"BLND", F_PCT, 0, 127, 20, 0, 0},
        {"SWEP", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = RING_PRESETS,
    .npresets = NELEM(RING_PRESETS),
    .note_on = ring_note_on,
    .render = ring_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = RING_POLY,
};
