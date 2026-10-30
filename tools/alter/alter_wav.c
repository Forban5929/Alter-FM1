/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER on the PC: renders listening clips as WAV files and measures the cost against ANALOG.
 *   cc -O2 -I build/alter -o build/alter/alter_wav tools/alter/alter_wav.c && build/alter/alter_wav build/alter
 * The ADSR here is an approximation of Felucca's (exponential times 1 ms .. 10 s), not voice.c itself. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define NVOICE 8
#define NPART 4
#include "fe_tables.h"
#include "../../firmware/engines/src/felucca_engine.h"

enum { P_ATK, P_DEC, P_SUS, P_REL, P_FTYPE, P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7, P_COUNT };
typedef struct track {
    int16_t p[P_COUNT];
    voice_t v[NVOICE];
} track_t;
static track_t trk[NPART];
static uint8_t eng_mem[NPART][2048];
#define ENG_MEM(p) ((void *)eng_mem[p])
#define ENG_MEM_FITS(name, size) _Static_assert((size) <= sizeof eng_mem[0], name " state too large")

#include "../../firmware/engines/src/dsp.c"
#include "../../firmware/engines/src/eng_analog.c"
#include "../../firmware/src/eng_alter.c"

/* ------------------------------------------------------------ voices --- */
static uint32_t vage;
static int32_t lastamp[NVOICE];

static int32_t ms_blocks(int32_t v)                     /* 0..127 -> blocks, ~1 ms .. 10 s */
{
    double ms = 1.0;
    int32_t i;
    for (i = 0; i < v; i++)
        ms *= 1.0752;                                   /* 10000 ^ (1/127) */
    return (int32_t)(ms * FS / 1000.0 / CTL) + 1;
}

static void voice_on(track_t *t, const engine_t *e, uint32_t k, uint32_t note, uint32_t vel)
{
    voice_t *v = &t->v[k];
    v->note = (uint8_t)note;
    v->vel = (uint8_t)vel;
    v->gate = 1;
    v->active = 1;
    v->stage = 1;
    v->age = ++vage;
    v->pitch16 = v->pitch_cur = (int32_t)note * 16;
    v->fine = 0;
    e->note_on(t, v);
}

static int32_t env_tick(track_t *t, voice_t *v)
{
    int32_t sus = t->p[P_SUS] << 17;                    /* Q24 */
    if (!v->active)
        return 0;
    switch (v->stage) {
    case 1:
        v->env += (1 << 24) / ms_blocks(t->p[P_ATK]);
        if (v->env >= 1 << 24) {
            v->env = 1 << 24;
            v->stage = 2;
        }
        break;
    case 2:
        v->env -= (v->env - sus) / (ms_blocks(t->p[P_DEC]) / 4 + 1);
        if (!sus && v->env < (1 << 24) / 2000) {
            v->env = 0;
            v->active = 0;
            v->stage = 0;
        }
        break;
    case 3:
        v->env -= v->env / (ms_blocks(t->p[P_REL]) / 4 + 1) + 1;
        if (v->env < (1 << 24) / 2000) {
            v->env = 0;
            v->stage = 0;
            v->active = 0;
        }
        break;
    default:
        break;
    }
    return (int32_t)(((int64_t)(v->env >> 9) * (v->vel + 27)) >> 7);   /* Q15, with the velocity */
}

static void render_block(track_t *t, const engine_t *e, int32_t *out)
{
    uint32_t k;
    memset(out, 0, CTL * sizeof *out);
    for (k = 0; k < NVOICE; k++) {
        voice_t *v = &t->v[k];
        vmod_t m;
        int32_t a;
        uint32_t base;
        if (!v->active)
            continue;
        a = env_tick(t, v);
        if (v->gstep && v->pitch_cur != v->pitch16) {  /* a glide: toward the note by gstep a block */
            int32_t d = v->pitch16 - v->pitch_cur;
            v->pitch_cur += d > v->gstep ? v->gstep : d < -v->gstep ? -v->gstep : d;
        } else {
            v->pitch_cur = v->pitch16;
        }
        base = pitch_inc((uint32_t)clamp(v->pitch_cur, 0, 2047));
        memset(&m, 0, sizeof m);
        m.pitch16 = v->pitch_cur;
        m.inc = base;
        m.amp0 = lastamp[k];
        m.amp1 = a;
        m.envq15 = a;
        m.shape = 64 << 8;
        lastamp[k] = a;
        e->render(t, v, out, CTL, &m);
        if (!v->active)
            lastamp[k] = 0;
    }
}

/* ------------------------------------------------------------ the WAV --- */
static void put32(FILE *f, uint32_t x) { fputc(x & 255, f); fputc(x >> 8 & 255, f); fputc(x >> 16 & 255, f); fputc(x >> 24, f); }
static void put16(FILE *f, uint32_t x) { fputc(x & 255, f); fputc(x >> 8 & 255, f); }

#define MAXS (FS * 20)
static int16_t pcm[MAXS];

typedef struct { int32_t peak; uint32_t clipped; double rms; } stat_t;

static stat_t wav_write(const char *dir, const char *name, uint32_t ns)
{
    char path[512];
    FILE *f;
    uint32_t i;
    stat_t s = {0, 0, 0};
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    f = fopen(path, "wb");
    fwrite("RIFF", 1, 4, f);
    put32(f, 36 + ns * 2);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16);
    put16(f, 1);
    put16(f, 1);
    put32(f, FS);
    put32(f, FS * 2);
    put16(f, 2);
    put16(f, 16);
    fwrite("data", 1, 4, f);
    put32(f, ns * 2);
    for (i = 0; i < ns; i++) {
        int32_t a = pcm[i] < 0 ? -pcm[i] : pcm[i];
        put16(f, (uint16_t)pcm[i]);
        if (a > s.peak)
            s.peak = a;
        if (a >= 32767)
            s.clipped++;
        s.rms += (double)pcm[i] * pcm[i];
    }
    fclose(f);
    return s;
}

/* a phrase: steps of a 16th at BPM; hit[] says which steps play (1 normal, 2 accent, 3 ghost), the note, the gate
 * in steps; sweep: a parameter moved from a to b across the clip (-1 = none) */
typedef struct {
    const char *name;
    int8_t e[8];
    uint8_t env[4];
    const char *hits;            /* 'x' hit, 'X' accent, 'g' ghost, '.' rest; repeated */
    uint8_t note, gate, bars;
    int8_t sweep;                /* P_E index 0..7 or -1 */
    int16_t from, to;
    int8_t notes[8];             /* a note sequence over the hits (0 = the note) */
} clip_t;

#define BPM 132

static uint32_t render_clip(const clip_t *c)
{
    track_t *t = &trk[0];
    int32_t out[CTL];
    uint32_t spb = (uint32_t)(FS * 60 / BPM / 4), nsteps = c->bars * 16u, len = (uint32_t)strlen(c->hits);
    uint32_t total = nsteps * spb + FS * 2, s = 0, b, i, k, step = 0, nexton = 0, gate_off = 0, ni = 0;
    uint32_t cur = 1;
    memset(t, 0, sizeof *t);
    memset(eng_mem, 0, sizeof eng_mem);
    memset(lastamp, 0, sizeof lastamp);
    vage = 0;
    for (k = 0; k < 8u; k++)
        t->p[P_E0 + k] = c->e[k];
    t->p[P_ATK] = c->env[0];
    t->p[P_DEC] = c->env[1];
    t->p[P_SUS] = c->env[2];
    t->p[P_REL] = c->env[3];
    if (total > MAXS)
        total = MAXS;
    for (b = 0; s + CTL <= total; b++) {
        if (c->sweep >= 0)
            t->p[P_E0 + c->sweep] = (int16_t)(c->from + (c->to - c->from) * (int32_t)s / (int32_t)(nsteps * spb));
        while (step < nsteps && s >= nexton) {         /* steps are placed to the block */
            char h = c->hits[step % len];
            if (h != '.') {
                uint32_t vel = h == 'X' ? 127 : h == 'g' ? 45 : 95, note = c->note;
                if (c->notes[0])
                    note = (uint32_t)((int32_t)c->note + c->notes[ni++ % 8u] - 1);
                cur ^= 1u;                              /* two voices: the tail rings under the next hit */
                if (t->v[cur].active)
                    t->v[cur].stage = 3;
                voice_on(t, &ENG_ALTER, cur, note, vel);
                gate_off = s + c->gate * spb;
            }
            step++;
            nexton = step * spb;
        }
        if (gate_off && s >= gate_off) {
            for (k = 0; k < 2u; k++)
                if (t->v[k].active && t->v[k].stage < 3)
                    t->v[k].stage = 3;
            gate_off = 0;
        }
        if (step >= nsteps && s >= nsteps * spb)
            for (k = 0; k < 2u; k++)
                if (t->v[k].active && t->v[k].stage < 3)
                    t->v[k].stage = 3;
        render_block(t, &ENG_ALTER, out);
        for (i = 0; i < CTL; i++)
            pcm[s + i] = (int16_t)clamp(out[i], -32767, 32767);
        s += CTL;
    }
    return s;
}

static const clip_t CLIPS[] = {
    /* the six presets on a rhythm */
    {"01_preset_THUD", {0, 0, 8, ALT_LIQUID, 90, 15, -40, 0}, {0, 60, 0, 45}, "x...x...x...x..g", 33, 1, 4, -1, 0, 0, {0}},
    {"02_preset_BELL", {8, 70, 90, ALT_SKIN, 15, 0, 30, 4}, {0, 95, 0, 90}, "x..x..x...x..x..", 57, 1, 4, -1, 0, 0, {1, 4, 1, 8, 6, 1, 4, 11}},
    {"03_preset_CLANG", {30, 100, 75, ALT_METAL, 60, 25, 0, 0}, {0, 72, 0, 60}, "..x...x...x..X.x", 45, 1, 4, -1, 0, 0, {0}},
    {"04_preset_SNAP", {20, 40, 50, ALT_LIQUID, 40, 110, -30, 0}, {0, 45, 0, 35}, "....X.......X..g", 50, 1, 4, -1, 0, 0, {0}},
    {"05_preset_STAB", {110, 15, 80, ALT_SKIN, 40, 10, -45, 0}, {0, 55, 0, 40}, "x..x..x...x..x..", 45, 1, 4, -1, 0, 0, {1, 1, 8, 1, 4, 1, 11, 6}},
    {"06_preset_RUST", {70, 120, 110, ALT_METAL, 100, 0, 10, 60}, {30, 127, 127, 85}, "x...............................", 33, 30, 4, -1, 0, 0, {0}},
    /* the three modes, the same sound, AMNT rising across the clip */
    {"07_mode_SKIN_dtun", {40, 30, 80, ALT_SKIN, 0, 10, -20, 0}, {0, 70, 0, 50}, "x.gx..x.gx..x.g.", 45, 1, 4, 4, 0, 127, {0}},
    {"08_mode_LIQUID_punch", {40, 30, 80, ALT_LIQUID, 0, 10, -20, 0}, {0, 70, 0, 50}, "x.gx..x.gx..x.g.", 45, 1, 4, 4, 0, 127, {0}},
    {"09_mode_METAL_fm", {40, 30, 80, ALT_METAL, 0, 10, -20, 0}, {0, 70, 0, 50}, "x.gx..x.gx..x.g.", 45, 1, 4, 4, 0, 127, {0}},
    /* the knobs swept one at a time */
    {"10_sweep_MORPH", {0, 20, 70, ALT_SKIN, 10, 0, -20, 0}, {0, 65, 0, 45}, "x..x..x.x..x..x.", 45, 1, 8, 0, 0, 127, {0}},
    {"11_sweep_SPREAD", {20, 0, 90, ALT_SKIN, 10, 0, 0, 0}, {0, 75, 0, 55}, "x..x..x.x..x..x.", 45, 1, 8, 1, 0, 127, {0}},
    {"12_sweep_HARMONICS", {20, 60, 0, ALT_SKIN, 10, 0, 0, 0}, {0, 75, 0, 55}, "x..x..x.x..x..x.", 45, 1, 8, 2, 0, 127, {0}},
    {"13_sweep_NOISE", {10, 40, 40, ALT_LIQUID, 50, 0, -30, 0}, {0, 55, 0, 40}, "x...x...x...x...", 40, 1, 8, 5, 0, 127, {0}},
    {"14_sweep_TILT", {15, 70, 110, ALT_SKIN, 10, 0, -64, 0}, {0, 90, 0, 70}, "x.......x.......", 52, 1, 8, 6, -64, 63, {0}},
    {"15_sweep_DRIFT_drone", {60, 90, 100, ALT_METAL, 60, 0, 0, 0}, {40, 127, 127, 90}, "x...............................................................", 33, 56, 8, 7, 0, 127, {0}},
};

/* cost: ns per voice and sample, 2 voices held for 4 s of audio */
static double cost(const engine_t *e, const int8_t *pe)
{
    track_t *t = &trk[1];
    int32_t out[CTL];
    uint32_t b, k, nb = (uint32_t)(FS * 4 / CTL);
    clock_t c0;
    memset(t, 0, sizeof *t);
    for (k = 0; k < 8u; k++)
        t->p[P_E0 + k] = pe[k];
    t->p[P_ATK] = 0;
    t->p[P_DEC] = 127;
    t->p[P_SUS] = 127;
    t->p[P_REL] = 60;
    voice_on(t, e, 0, 45, 100);
    voice_on(t, e, 1, 52, 100);
    c0 = clock();
    for (b = 0; b < nb; b++) {
        render_block(t, e, out);
        if (!t->v[0].active)
            voice_on(t, e, 0, 45, 100);
        if (!t->v[1].active)
            voice_on(t, e, 1, 52, 100);
    }
    return (double)(clock() - c0) * 1e9 / CLOCKS_PER_SEC / (nb * CTL * 2.0);
}

#ifndef ALTER_HOST_NO_MAIN
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    uint32_t j;
    for (j = 0; j < NELEM(CLIPS); j++) {
        uint32_t ns = render_clip(&CLIPS[j]);
        stat_t s = wav_write(dir, CLIPS[j].name, ns);
        printf("%-24s %5.1fs peak %5d (%5.1f dBFS) rms %5.1f dBFS clipped %u\n", CLIPS[j].name, (double)ns / FS, s.peak,
               20 * __builtin_log10((double)s.peak / 32767 + 1e-9), 10 * __builtin_log10(s.rms / ns / (32767.0 * 32767) + 1e-12),
               s.clipped);
    }
    {
        static const int8_t AN[8] = {1, 0, 64, 20, 0, 0, 0, 0};
        static const int8_t SK[8] = {0, 30, 127, ALT_SKIN, 40, 0, 0, 20};
        static const int8_t SKB[8] = {127, 30, 127, ALT_SKIN, 40, 0, 0, 20};
        static const int8_t LQ[8] = {64, 30, 127, ALT_LIQUID, 40, 0, 0, 20};
        static const int8_t MT[8] = {127, 30, 127, ALT_METAL, 100, 0, 0, 20};
        double a = 0, x;
        int r;
        for (r = 0; r < 3; r++)
            a += cost(&ENG_ANALOG, AN) / 3;
        printf("cost ANALOG          %6.1f ns / voice-sample\n", a);
        x = cost(&ENG_ALTER, SK);
        printf("cost ALTER SKIN sine %6.1f ns  (x%.1f ANALOG)\n", x, x / a);
        x = cost(&ENG_ALTER, SKB);
        printf("cost ALTER SKIN sqr  %6.1f ns  (x%.1f ANALOG)\n", x, x / a);
        x = cost(&ENG_ALTER, LQ);
        printf("cost ALTER LIQ saw   %6.1f ns  (x%.1f ANALOG)\n", x, x / a);
        x = cost(&ENG_ALTER, MT);
        printf("cost ALTER METAL sqr %6.1f ns  (x%.1f ANALOG)\n", x, x / a);
    }
    return 0;
}
#endif
