/* SPDX-License-Identifier: GPL-3.0-only
 * WAVE (v1.16, Romain 23/11: "une wavetable indus"): one oscillator reading an industrial wavetable, a warp in the
 * manner of the big wavetable synths (one MODE, one AMNT), two short envelopes, a sub and a second detuned copy.
 * After four listening rounds (tools/alter/alter_wave_wav.py, alter_wave_warp.py): the tables CLANG RUST SHRED
 * FEEDBACK, the warps SYNC BEND FM RING FOLD BITS (PWM and MIRROR left out). Ideas from TastyWaves (Mark Weiss,
 * GPL-3.0: an integer table oscillator with mip levels, its BEND warp) and from Shrub (ideas only); the tables are
 * our own (eng_wave_tab.h, tools/alter/alter_wave_tables.py: 4 tables x 16 frames x 4 mips, int8, ~31 KB).
 *
 * TABL  the table: CLANG (bells, anvils), RUST (tuned noise), SHRED (sample-and-hold steps), FEEDBACK (a howl)
 * POS   where in the table (16 frames, blended); the track's SHP moves it too
 * MODE  the warp: SYNC (the cycle read faster, reset: x1 .. x8), BEND (the phase bent: p^1 .. p^5), FM (the phase
 *       pushed by a sine at twice the note), RING (eng_xform.h: x0.5 mixed in, then the carrier up to x2.71),
 *       FOLD (eng_xform.h's folder), BITS (fewer levels, samples held)
 * AMNT  how much warp; the track's FLT moves it too. 0: no warp runs
 * PENV  a hit pushes POS up, falling back over ~0.15 s (a bright attack, a sweep through the table)
 * WENV  a hit pushes AMNT up the same way (a zap of warp at each hit)
 * SUB   a sine one octave down, under the warp (weight)
 * SPRD  a second copy, detuned up to ~40 cents, through the same warp (wide, beating); 0: it does not run
 * Three voices per part; voice state: ph[0] the oscillator, ph[1] its copy, ph[2] the sub; s[0] PENV, s[1] WENV,
 * s[2] BITS' count, s[3] BITS' held sample, s[4] RING's carrier phase. Integers only, no division per sample. */

#include "eng_wave_tab.h"

#define WAVE_POLY 3
#define WV_ENVK 32609                            /* the envelopes' fall a block: e^(-32 / (0.15 s x 44100)), Q15 */

static const char *const N_WV_TAB[] = {"CLNG", "RUST", "SHRD", "FDBK"};
static const char *const N_WV_MODE[] = {"SYNC", "BEND", "FM", "RING", "FOLD", "BITS"};
enum { WV_SYNC, WV_BEND, WV_FM, WV_RING, WV_FOLD, WV_BITS };
static const uint16_t WV_MOFF[4] = {0, 256, 384, 448};

static void wave_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->s[0] = 32767;
    v->s[1] = 32767;
    v->ph[1] = v->ph[0];                         /* (SPRD: the copy starts in phase, a small detune changes no level;
                                                  * a sounding voice keeps its phases: voice.c voice_start) */
}

/* one point of two frames (a, b at the same mip) at phase p: linear between the points, then between the frames
 * (ff, Q8); ~ +-32512 (int8 x 256) */
static inline int32_t wv_rd(const int8_t *a, const int8_t *b, uint32_t p, uint32_t sh, uint32_t mk, int32_t ff)
{
    uint32_t i = p >> sh, j = (i + 1u) & mk;
    int32_t fr = (int32_t)((p << (32u - sh)) >> 24), x0, x1;
    x0 = a[i] * 256 + (a[j] - a[i]) * fr;
    x1 = b[i] * 256 + (b[j] - b[i]) * fr;
    return x0 + (((x1 - x0) * ff) >> 8);
}

/* the phase warps (SYNC BEND FM); w: SYNC's ratio (Q12), BEND's amount (0 .. 4 in Q15), FM's depth */
static inline uint32_t wv_warp(uint32_t p, uint32_t mode, int32_t w)
{
    if (mode == WV_SYNC)
        return (uint32_t)(((uint64_t)p * (uint32_t)w) >> 12);
    if (mode == WV_BEND) {                       /* p^(1 + 4a): between p, p^2, p^4, p^5 */
        uint32_t x = p >> 16, x2 = (x * x) >> 16, x4 = (x2 * x2) >> 16, y;
        if (w < 32768)
            y = x + (uint32_t)(((int64_t)((int32_t)x2 - (int32_t)x) * w) >> 15);
        else if (w < 98304)
            y = x2 + (uint32_t)(((int64_t)((int32_t)x4 - (int32_t)x2) * (w - 32768)) >> 16);
        else
            y = x4 + (uint32_t)(((int64_t)((int32_t)((x4 * x) >> 16) - (int32_t)x4) * (w - 98304)) >> 15);
        return y << 16;
    }
    return p + (uint32_t)((int64_t)sine_i(p << 1) * w);   /* FM */
}

static void wave_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    const int8_t *fa, *fb, *ga = 0, *gb = 0;
    uint32_t tab = (uint32_t)p[P_E0] & 3u, mode = (uint32_t)p[P_E2] % 6u, inc = m->inc, inc2 = 0, einc, sh, mk, mi, spr,
             rph = (uint32_t)v->s[4], rinc = 0;
    int32_t pos, ak, a, fp, fr, ff, g, w = 0, sk = 0, subk, q = 0, hold = 1, i, cnt = v->s[2], held = v->s[3];
    ering_t er;
    efold_t fo;
    /* POS (+ SHP, + PENV) -> two frames and the blend */
    pos = clamp((p[P_E1] << 8) + m->shape - (64 << 8) + ((v->s[0] * p[P_E4]) >> 7), 0, (127 << 8) - 1);
    fp = pos * 15 / 127;                         /* (frames, Q8: 0 .. 14.99) */
    fr = fp >> 8;
    ff = fp & 255;
    g = WV_GAIN[tab][fr] + (((WV_GAIN[tab][fr + 1] - WV_GAIN[tab][fr]) * ff) >> 8);
    /* AMNT (+ FLT, + WENV), Q15 */
    ak = clamp((p[P_E3] << 8) + m->cutoff + ((v->s[1] * p[P_E5]) >> 7), 0, 127 << 8);
    a = ak + (ak >> 7);                          /* (0 .. 32766) */
    v->s[0] = (v->s[0] * WV_ENVK) >> 15;
    v->s[1] = (v->s[1] * WV_ENVK) >> 15;
    einc = inc;
    if (a && mode == WV_SYNC) {
        w = 4096 + ((a * 7) >> 3);               /* x1 .. x8, Q12 */
        einc = (uint32_t)(((uint64_t)inc * (uint32_t)w) >> 12);
    } else if (a && mode == WV_BEND) {
        w = a * 4;
        einc = inc + (uint32_t)(((uint64_t)inc * (uint32_t)a) >> 14);
    } else if (a && mode == WV_FM) {
        w = ((a * 39321) >> 15) << 1;            /* (0.6 turn at full: sine (Q15) x 0.6 a x 2^17) */
        einc = inc + (uint32_t)(((uint64_t)inc * (uint32_t)a) >> 14);
    } else if (a && mode == WV_BITS) {
        q = 6 + ((a * 9) >> 15);                 /* (the step: 2^6 .. 2^14) */
        hold = 1 + ((a * 20) >> 15);
    }
    if (!a && mode < WV_RING)
        mode = WV_RING;                          /* (AMNT 0: no phase warp) */
    er.on = 0;
    fo.on = 0;
    if (mode == WV_RING) {
        ering_set(&er, ak >> 8);
        rinc = ering_inc(&er, inc);
    } else if (mode == WV_FOLD)
        efold_set(&fo, ak >> 8);
    /* the mip: the highest of its harmonics under ~22 kHz (64 harmonics below ~340 Hz, then 32, 16, 8) */
    for (mi = 0; mi < 3u && einc >= (1u << 25); mi++)
        einc >>= 1;
    sh = 24u + mi;
    mk = (256u >> mi) - 1u;
    fa = WV_TAB[tab][fr] + WV_MOFF[mi];
    fb = WV_TAB[tab][fr + 1] + WV_MOFF[mi];
    spr = (uint32_t)p[P_E7];
    if (spr) {
        inc2 = (uint32_t)(((uint64_t)inc * (4096u + spr * spr * 100u / 16129u)) >> 12);
        ga = fa;
        gb = fb;
        sk = spr > 32u ? 16384 : (int32_t)spr * 512;
    }
    subk = p[P_E6] * 120;
    for (i = 0; i < (int32_t)n; i++) {
        int32_t y;
        uint32_t ph = v->ph[0];
        if (mode < WV_RING)
            ph = wv_warp(ph, mode, w);
        y = wv_rd(fa, fb, ph, sh, mk, ff);
        v->ph[0] += inc;
        if (spr) {
            uint32_t p2 = v->ph[1];
            if (mode < WV_RING)
                p2 = wv_warp(p2, mode, w);
            y += ((wv_rd(ga, gb, p2, sh, mk, ff) - y) * sk) >> 15;   /* (up to half and half) */
            v->ph[1] += inc2;
        }
        y = (y * g) >> 12;                       /* (back to the frame's loudness: RMS ~8000) */
        if (er.on)
            y = ering_run(&er, y, &rph, rinc);
        else if (fo.on)
            y = efold_run(&fo, y * 2) >> 1;   /* (peaks ~90000: under the folder's first fold at its g = 1) */
        else if (q) {
            if (++cnt >= hold) {
                cnt = 0;
                held = (y >> q) << q;
            }
            y = held;
        }
        if (subk) {
            y += (sine_i(v->ph[2]) * subk) >> 15;
            v->ph[2] += inc >> 1;
        }
        out[i] += voice_amp(soft_knee(y * 2, 24000), m, (uint32_t)i);
    }
    v->s[2] = cnt;
    v->s[3] = held;
    v->s[4] = (int32_t)rph;
}

/* {TABL POS MODE AMNT PENV WENV SUB SPRD} (ALTER's sounds: alter_sounds.c ALT_PRE) */
static const preset_t WAVE_PRESETS[] = {
    {"CLANG", {0, 40, 0, 30, 60, 40, 0, 0}, {0, 80, 0, 50}, 0, 0, FX(0, 0, 10, 20)},
};

static const engine_t ENG_WAVE = {
    .name = "WAVE",
    .page_title = {"MAIN", "MORE"},
    .edit = {
        {"TABL", F_ENUM, 0, 3, 0, N_WV_TAB, 0},
        {"POS", F_PCT, 0, 127, 40, 0, 0},
        {"MODE", F_ENUM, 0, 5, 0, N_WV_MODE, 0},
        {"AMNT", F_PCT, 0, 127, 30, 0, 0},
        {"PENV", F_PCT, 0, 127, 60, 0, 0},
        {"WENV", F_PCT, 0, 127, 40, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
        {"SPRD", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = WAVE_PRESETS,
    .npresets = NELEM(WAVE_PRESETS),
    .note_on = wave_note_on,
    .render = wave_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = WAVE_POLY,
};
