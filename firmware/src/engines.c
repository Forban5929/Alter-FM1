/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol), the
 * factory patterns (SAVE > PHRASES) and the parts' sounds at power-on. */
#include "../engines/src/dsp.c"   /* Felucca-Engines (MIT): the shared DSP blocks */

/* The parts' engine memory (1.5): a part plays one engine at a time, so the engines with large per-part state
 * (engine_t.mem: PHYS's slots, GRAIN's grains and index, DRUM's lanes, FM6's notes, SLICE's reverse windows)
 * keep it in one region per part instead of an array each. The region holds the state of the engine that
 * last claimed it (eng_mem_owner); a switch to another such engine zeroes its engine_t.mem bytes, which is
 * the state each had at power-on, so a switch sounds as it did with the arrays (tests/union_test.c). State
 * that runs on across notes and switches stays outside (WHEEL's wheels, GRAIN's random numbers).
 * Sized by the largest, ADPCM when it is built (else PHYS, 3 x SYMP's slot); an engine that needs more than ENG_MEM_PART grows it for
 * every part (x NPART of the pool). Zero-initialised pool section: main.c fm1_cstart clears it. */
#define ENG_MEM_PART 12888u
static union {
    uint64_t align;
    uint8_t b[ENG_MEM_PART];
} eng_mem[NPART] __attribute__((section(".pool")));
static uint8_t eng_mem_owner[NPART];       /* engine index + 1 whose state is in the region, 0 = none (zeros) */
#define ENG_MEM(p) ((void *)eng_mem[p].b)  /* part p's region (p < NPART) */
#define ENG_MEM_FITS(name, size) _Static_assert((size) <= ENG_MEM_PART, name " state over ENG_MEM_PART")

#include "../engines/src/eng_noise.c"   /* Felucca-Engines (MIT): NOISE */
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#include "eng_alter.c"          /* ALTER: six struck oscillators (after the Basimilus Iteritas Alter) */
#include "eng_reso.c"           /* RESO: four struck resonant filters (after a QMMF) */
#include "eng_plate.c"          /* PLATE: a struck metal sheet (05/11) */
#include "eng_corde.c"          /* CORDE: a mistreated metal string (05/11) */
#include "eng_ring.c"           /* RING: a ring modulator, a valve, a radio (05/11) */
#include "alter_legacy.c"       /* what the old Felucca pages still name of the engines taken out */

/* ALTER plays three engines; the other numbers are kept empty (a silent engine nobody can pick), so the engine
 * numbers stay as they were (ALTER 14, PHYS 9, NOISE 11) */
static void gone_note_on(track_t *t, voice_t *v) { (void)t; v->active = 0; }
static void gone_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)out; (void)n; (void)m;
    v->active = 0;
}
static const engine_t ENG_GONE = {
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {{"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}},
    .note_on = gone_note_on,
    .render = gone_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
};

static const engine_t *const ENGINES[NENGINES] = {
    &ENG_GONE, &ENG_GONE,
    &ENG_CORDE,                  /* 2 (ENGI_CORDE) */
    &ENG_RING,                   /* 3 (ENGI_RING) */
    &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE, &ENG_GONE,
    &ENG_PHYS,                   /* 9 (ENGI_PHYS) */
    &ENG_RESO,                   /* 10 (ENGI_RESO) */
    &ENG_NOISE,                  /* 11 (ENGI_NOISE) */
    &ENG_PLATE,                  /* 12 (ENGI_PLATE) */
    &ENG_GONE,
    &ENG_ALTER,                  /* 14 (ENGI_ALTER) */
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* part t's engine memory goes to its engine (t->engine; voice.c engine_block, once a block: on a switch, after
 * the old engine's fade, before any note on the new one): zeroed (its engine_t.mem bytes) when another
 * engine's state is in it. An engine without memory leaves it as it is: a switch back finds its state. */
static void eng_mem_claim(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk), e, k, n;
    uint32_t *w;
    if (p >= NPART)
        return;
    e = eng_idx(t->engine);
    n = ENGINES[e]->mem;
    if (!n || eng_mem_owner[p] == e + 1u)
        return;
    w = (uint32_t *)ENG_MEM(p);
    for (k = 0; k < (n + 3u) / 4u; k++)
        w[k] = 0;
    eng_mem_owner[p] = (uint8_t)(e + 1u);
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the EDIT layer's keys, the editor's list):
 * engine indices, never DIGITAL's reserved 1 (with FELUCCA_FM4 it follows FM6). The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {ENGI_ALTER, ENGI_PHYS, ENGI_NOISE, ENGI_RESO, ENGI_PLATE, ENGI_CORDE, ENGI_RING};

static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }

/* the parts at power-on (engine, preset, PATTERNS[n - 1] in the sequencer, 0 = empty: all are): bass, pad, lead, drums */
/* ALTER: circles 1-3 play parts 1-3 (alter_play.c): ALTER THUD, PHYS TOMS, NOISE METAL; part 4 stays silent */
static const uint8_t TRK_DEF[NPART][3] = {{ENGI_ALTER, 0, 0}, {ENGI_PHYS, 6, 0}, {ENGI_NOISE, 3, 0}, {ENGI_NOISE, 3, 0}};
