/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER played by its sequencer (alter_seq.c), on the PC: renders listening clips as WAV files and prints each
 * clip's circles (for the listening page) and when each asked-for change was heard.
 *   cc -std=c99 -O2 -I build/alter -o build/alter/alter_seq_demo tools/alter/alter_seq_demo.c -lm
 *   build/alter/alter_seq_demo build/alter/wav */
#define ALTER_HOST_NO_MAIN
#include "alter_wav.c"
#include "../../firmware/src/alter_seq.c"

typedef struct {
    const char *name;
    int8_t e[8];                 /* the sound: MRPH SPRD HARM MODE AMNT NOIS TILT DRFT */
    uint8_t env[4];
    uint8_t root, scale, locks;
    as_cfg_t c[AS_NCIRC];
    uint32_t bars;
    /* actions: at step `at` (sixteenths from the start), circle `circ` gets `cfg` (set) or a roll (roll = 1) */
    struct { uint32_t at; uint8_t circ, roll; as_cfg_t cfg; } act[8];
} demo_t;

#define C(len, hits, rot, prob, die, dose) {len, hits, rot, prob, die, dose, 0}

static const demo_t DEMOS[] = {
    {"16_seq_base", {0, 0, 10, ALT_LIQUID, 85, 15, -40, 0}, {0, 58, 0, 45}, 33, AS_SC_OCT, 0,
     {C(16, 4, 0, 127, 0, 0), C(16, 5, 2, 127, 0, 0), C(8, 3, 0, 127, 0, 0)}, 4, {{0}}},
    {"17_seq_polyrythmie", {30, 100, 70, ALT_METAL, 55, 20, 0, 0}, {0, 62, 0, 50}, 45, AS_SC_OCT, 0,
     {C(16, 5, 0, 127, 0, 0), C(12, 7, 1, 127, 0, 0), C(5, 2, 0, 127, 0, 0)}, 8, {{0}}},
    {"18_seq_changements_live", {110, 15, 80, ALT_SKIN, 40, 10, -45, 0}, {0, 52, 0, 38}, 45, AS_SC_OCT, 0,
     {C(16, 3, 0, 127, 0, 0), C(16, 0, 0, 127, 0, 0), C(16, 2, 0, 127, 0, 0)}, 8,
     {{21, AS_HITS, 0, C(16, 5, 0, 127, 0, 0)},          /* asked mid-bar 2: heard from bar 3 */
      {54, AS_GHOST, 0, C(16, 4, 1, 127, 0, 0)},         /* mid-bar 4: from bar 5 */
      {85, AS_HITS, 0, C(16, 5, 3, 127, 0, 0)},          /* mid-bar 6: the rotation, from bar 7 */
      {0}}},
    {"19_seq_probabilite", {20, 40, 50, ALT_LIQUID, 40, 60, -30, 0}, {0, 40, 0, 30}, 40, AS_SC_OCT, 0,
     {C(16, 9, 0, 105, 0, 0), C(16, 5, 1, 70, 0, 0), C(16, 4, 0, 90, 0, 0)}, 8, {{0}}},
    {"20_de_dose_faible", {100, 20, 75, ALT_SKIN, 30, 8, -40, 0}, {0, 50, 0, 36}, 36, AS_SC_MPEN, 0,
     {C(16, 7, 0, 127, 1, 22), C(16, 3, 1, 127, 0, 0), C(8, 3, 0, 127, 0, 0)}, 8,
     {{0, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)}, {37, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)},
      {69, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)}, {101, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)}, {0}}},
    {"21_de_dose_forte", {60, 60, 90, ALT_METAL, 50, 25, 0, 10}, {0, 60, 0, 45}, 36, AS_SC_PHRY, 0,
     {C(16, 7, 0, 127, 1, 120), C(16, 4, 1, 127, 1, 120), C(8, 3, 0, 127, 1, 120)}, 8,
     {{0, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)}, {37, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)},
      {69, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)}, {101, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)}, {0}}},
    {"22_de_verrous", {60, 60, 90, ALT_METAL, 50, 25, 0, 10}, {0, 60, 0, 45}, 36, AS_SC_PHRY, AS_LK_PITCH | AS_LK_GLIDE,
     {C(16, 7, 0, 127, 1, 120), C(16, 4, 1, 127, 1, 120), C(8, 3, 0, 127, 1, 120)}, 8,
     {{0, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)}, {37, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)},
      {69, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)}, {101, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)}, {0}}},
    {"23_acid_glide", {120, 10, 60, ALT_SKIN, 25, 0, -50, 0}, {0, 45, 40, 30}, 36, AS_SC_MIN, AS_LK_MRPH | AS_LK_SPRD,
     {C(16, 11, 0, 127, 1, 75), C(16, 0, 0, 127, 0, 0), C(16, 4, 2, 127, 0, 0)}, 8,
     {{0, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)}, {69, AS_HITS, 1, C(0, 0, 0, 0, 0, 0)}, {0}}},
};

static uint32_t render_demo(const demo_t *d, char *rings, size_t rsz)
{
    static as_seq_t q;
    track_t *t = &trk[0];
    int32_t out[CTL];
    uint32_t spb = (uint32_t)(FS * 60 / BPM / 4), nsteps = d->bars * 16u, total = nsteps * spb + FS * 2;
    uint32_t s = 0, step = 0, nexton = 0, gate_off = 0, cur = 1, k, i, a;
    int has_note = 0;
    memset(t, 0, sizeof *t);
    memset(eng_mem, 0, sizeof eng_mem);
    memset(lastamp, 0, sizeof lastamp);
    vage = 0;
    as_init(&q, 2026);
    q.root = d->root;
    q.scale = d->scale;
    q.locks = d->locks;
    for (k = 0; k < AS_NCIRC; k++) {
        q.c[k].cfg = q.c[k].next = d->c[k];
        q.c[k].pat = as_euclid(d->c[k].len, d->c[k].hits, d->c[k].rot);
    }
    t->p[P_ATK] = d->env[0];
    t->p[P_DEC] = d->env[1];
    t->p[P_SUS] = d->env[2];
    t->p[P_REL] = d->env[3];
    if (total > MAXS)
        total = MAXS;
    for (; s + CTL <= total;) {
        while (step < nsteps && s >= nexton) {
            as_event_t ev;
            for (a = 0; a < NELEM(d->act); a++)        /* an entry with neither a roll nor a LEN is empty */
                if (d->act[a].at == step && (d->act[a].roll || d->act[a].cfg.len)) {
                    if (d->act[a].roll)
                        as_roll(&q, d->act[a].circ);
                    else
                        as_set(&q, d->act[a].circ, d->act[a].cfg);
                }
            as_step(&q, &ev);
            if (ev.on) {
                for (k = 0; k < 8u; k++) {              /* the sound, with this step's timbre offsets */
                    int32_t v = d->e[k];
                    if (k == 0 || k == 1 || k == 2 || k == 4)
                        v += ev.timb[k == 4 ? 3 : k];
                    t->p[P_E0 + k] = (int16_t)(k == 3 ? v : k == 6 ? clamp(v, -64, 63) : clamp(v, 0, 127));
                }
                if (ev.glide && has_note && t->v[cur].active && t->v[cur].stage < 3) {
                    voice_t *v = &t->v[cur];           /* a slide: the sounding note moves, no new hit */
                    v->pitch16 = ev.note * 16;
                    v->gstep = 12;                      /* ~45 ms an octave */
                } else {
                    cur ^= 1u;
                    if (t->v[cur].active)
                        t->v[cur].stage = 3;
                    voice_on(t, &ENG_ALTER, cur, ev.note, ev.vel);
                    t->v[cur].gstep = 0;
                    has_note = 1;
                }
                gate_off = s + spb;
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
        render_block(t, &ENG_ALTER, out);
        for (i = 0; i < CTL; i++)
            pcm[s + i] = (int16_t)clamp(out[i], -32767, 32767);
        s += CTL;
    }
    /* the circles as they play at the end: len, then '1'/'0' per step, for the listening page */
    {
        size_t o = 0;
        for (k = 0; k < AS_NCIRC; k++) {
            o += (size_t)snprintf(rings + o, rsz - o, "%s%u:", k ? "|" : "", q.c[k].cfg.mute ? 0u : q.c[k].cfg.len);
            for (i = 0; i < q.c[k].cfg.len && !q.c[k].cfg.mute; i++)
                o += (size_t)snprintf(rings + o, rsz - o, "%u", (q.c[k].pat >> i) & 1u);
        }
    }
    return s;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    uint32_t j;
    for (j = 0; j < NELEM(DEMOS); j++) {
        char rings[256];
        uint32_t ns = render_demo(&DEMOS[j], rings, sizeof rings);
        stat_t s = wav_write(dir, DEMOS[j].name, ns);
        printf("%-26s %5.1fs peak %5.1f dBFS clipped %u rings %s\n", DEMOS[j].name, (double)ns / FS,
               20 * __builtin_log10((double)s.peak / 32767 + 1e-9), s.clipped, rings);
    }
    return 0;
}
