/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's own sounds and scenes (included by alter_ui.c). Felucca's preset tables stay as they are: PRESETS on the
 * PLAY view browses ALT_PRE instead, the industrial / noise / texture sounds of the three engines in a row, and SAVE
 * opens ALT_SCENE, a starting point for the whole instrument.
 *
 * A sound: its engine (ALT_ENGS index: 0 ALTER, 1 PHYS, 2 NOISE), the engine's E1..E8, the ADSR, the sends
 * (DIST CHOR DLY REV), LEVEL and the envelope mode it is made for (LEVEL set by measure: tools/alter/alter_render.mjs prints each sound's loudness,
 * the levels bring them within a few dB of each other).
 * A scene: TEMPO, SCALE, SWING, then per circle a sound, its envelope mode (alter_play.c AM_*), its rhythm (LEN HITS
 * ROT PROB), its root, its chord (alter_seq.c AS_CH_*), a level offset (0.5 dB steps). Loading one clears the dice, the mutes and the locks. */

typedef struct {
    const char *name;
    uint8_t eng;                 /* 0 ALTER, 1 PHYS, 2 NOISE */
    int8_t e[8];
    uint8_t env[4];              /* ATK DEC SUS REL */
    uint8_t fx[4];               /* DIST CHOR DLY REV */
    uint8_t level;
    uint8_t mode;                /* the envelope mode it is made for (alter_play.c AM_*: the renders use it) */
} alt_pre_t;

/* ALTER {MRPH SPRD HARM MODE AMNT NOIS TILT DRFT}: MODE 0 SKIN, 1 LIQUID, 2 METAL */
#define AL_S 0
#define AL_L 1
#define AL_M 2
/* PHYS {MODEL ..}: MODAL {STRC BRIT DAMP POS ACC BOW EXC}, STRNG the same, MEMB {HARM BRIT DAMP POS ACC BEND EXC},
 * SYMP {CHRD BRIT DAMP SYMP ACC BUZZ EXC} */
/* NOISE {MODE COLR FREQ RES TRK DENS DRFT CRSH}: MODE 0 ANLG, 1 DUST, 2 LFSR, 3 META */
static const alt_pre_t ALT_PRE[] = {
    /* ALTER */
    {"THUD", 0, {0, 0, 8, AL_L, 90, 15, -40, 0}, {0, 60, 0, 45}, {10, 0, 0, 10}, 116, 1},
    {"ANVIL", 0, {40, 100, 90, AL_M, 70, 30, -20, 0}, {0, 70, 0, 55}, {20, 0, 15, 30}, 114, 1},
    {"STAB", 0, {110, 15, 80, AL_S, 40, 10, -45, 0}, {0, 55, 40, 40}, {25, 0, 30, 25}, 116, 2},
    {"PIPES", 0, {8, 0, 70, AL_S, 25, 0, 20, 6}, {0, 90, 60, 70}, {0, 10, 30, 45}, 106, 2},
    {"RUST", 0, {70, 120, 110, AL_M, 100, 0, 10, 60}, {30, 127, 127, 85}, {30, 0, 30, 60}, 96, 3},
    {"GRIND", 0, {127, 90, 127, AL_M, 127, 60, 0, 90}, {10, 100, 90, 70}, {60, 0, 20, 40}, 92, 3},
    /* PHYS */
    {"TOMS", 1, {PM_MEMB, 0, 52, 40, 30, 100, 40, 30}, {0, 100, 127, 70}, {0, 0, 0, 40}, 104, 1},
    {"GONG", 1, {PM_MODAL, 100, 90, 112, 10, 100, 0, 10}, {0, 100, 127, 90}, {0, 0, 20, 55}, 104, 0},
    {"BARS", 1, {PM_MODAL, 34, 80, 100, 15, 90, 0, 10}, {0, 100, 127, 80}, {0, 15, 30, 50}, 110, 2},
    {"SCRAPE", 1, {PM_MODAL, 98, 100, 120, 10, 100, 127, 50}, {40, 90, 127, 90}, {10, 30, 20, 70}, 124, 3},
    {"BUZZ", 1, {PM_STRING, 10, 90, 90, 40, 100, 0, 20}, {0, 100, 127, 60}, {20, 0, 25, 30}, 122, 0},
    {"SITAR", 1, {PM_SYMP, 127, 80, 88, 64, 100, 92, 0}, {40, 100, 127, 100}, {0, 10, 20, 60}, 96, 3},
    /* NOISE */
    {"HAT", 2, {0, 0, 115, 80, 64, 0, 0, 0}, {0, 25, 0, 20}, {0, 0, 10, 15}, 114, 1},
    {"SNARE", 2, {0, 30, 85, 40, 64, 40, 0, 0}, {0, 40, 0, 30}, {10, 0, 15, 30}, 124, 1},
    {"METAL", 2, {3, 0, 100, 40, 100, 32, 0, 0}, {0, 75, 40, 60}, {0, 20, 30, 45}, 110, 1},
    {"CRUSH", 2, {0, 20, 90, 30, 64, 20, 0, 90}, {0, 50, 0, 40}, {30, 0, 20, 25}, 122, 0},
    {"RAIN", 2, {1, 30, 88, 80, 64, 105, 40, 0}, {10, 90, 127, 70}, {0, 0, 20, 70}, 110, 0},
    {"WIND", 2, {0, 64, 70, 100, 127, 0, 70, 0}, {90, 90, 110, 90}, {0, 30, 20, 80}, 100, 3},
    {"ARCADE", 2, {2, 0, 127, 0, 127, 112, 0, 84}, {0, 80, 0, 50}, {0, 0, 10, 20}, 110, 0},
};
#define ALT_NPRE (sizeof ALT_PRE / sizeof ALT_PRE[0])

/* the sound named n (0 = none: the first) */
static uint32_t alt_pre_find(const char *n)
{
    uint32_t i, k;
    for (i = 0; i < ALT_NPRE; i++) {
        for (k = 0; n[k] && n[k] == ALT_PRE[i].name[k]; k++)
            ;
        if (!n[k] && !ALT_PRE[i].name[k])
            return i;
    }
    return 0;
}

typedef struct {
    const char *name, *about;
    uint8_t bpm, scale, swing;
    struct {
        const char *pre;
        uint8_t mode, len, hits, rot, prob, root, chord;
        int8_t gain;             /* LEVEL steps on the sound's (0.5 dB each): the scene's balance */
    } c[AS_NCIRC];
} alt_scene_t;

static const alt_scene_t ALT_SCENE[] = {
    {"TECHNO", "PERC . STAB . TEXTURE", 132, AS_SC_PHRY, 0,
     {{"THUD", 1, 16, 4, 0, 127, 36, AS_CH_OFF, 0},
      {"STAB", 2, 16, 3, 3, 127, 48, AS_CH_TRI, 0},
      {"CRUSH", 0, 16, 11, 0, 110, 60, AS_CH_OFF, 0}}},
    {"HOUSE", "PERC . CHORD . DRONE", 124, AS_SC_DOR, 18,
     {{"TOMS", 1, 16, 5, 2, 127, 45, AS_CH_OFF, 0},
      {"BARS", 2, 16, 3, 2, 127, 50, AS_CH_TRI, 0},
      {"SITAR", 3, 32, 1, 0, 127, 38, AS_CH_OFF, 0}}},
    {"AMBIENT", "DRONE . DRONE . TEXTURE", 80, AS_SC_MIN, 0,
     {{"RUST", 3, 32, 1, 0, 127, 36, AS_CH_OFF, -4},
      {"SCRAPE", 3, 24, 1, 12, 127, 43, AS_CH_OPEN, -34},
      {"RAIN", 0, 16, 5, 0, 100, 60, AS_CH_OFF, -8}}},
    {"INDUS", "METAL . NOISE . GONG", 128, AS_SC_PHRY, 0,
     {{"ANVIL", 1, 16, 5, 0, 127, 36, AS_CH_OFF, 0},
      {"METAL", 1, 16, 7, 1, 127, 55, AS_CH_OFF, 0},
      {"GONG", 0, 16, 3, 2, 100, 40, AS_CH_OFF, 0}}},
};
#define ALT_NSCENE (sizeof ALT_SCENE / sizeof ALT_SCENE[0])
