/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's own sounds and scenes (included by alter_ui.c). Felucca's preset tables stay as they are: PRESETS on the
 * PLAY view browses ALT_PRE instead, the industrial / noise / texture sounds of the three engines in a row, and SAVE
 * opens ALT_SCENE, a starting point for the whole instrument.
 *
 * A sound: its engine (ALT_ENGS index: 0 ALTER, 1 PHYS, 2 NOISE), the engine's E1..E8, the ADSR, the sends
 * (DIST CHOR DLY REV), LEVEL and the envelope mode it is made for (LEVEL set by measure: tools/alter/alter_render.mjs prints each sound's loudness,
 * the levels bring them within a few dB of each other).
 * A scene: TEMPO, SCALE, SWING, then per circle a sound, its envelope mode (alter_play.c AM_*), its rhythm (LEN HITS
 * ROT PROB), its root, its chord (alter_seq.c AS_CH_*), a level offset (0.5 dB steps), a DELAY send (0: the sound's);
 * then the FX page's DELAY, REVERB and MASTER presets. Loading one clears the dice, the mutes and the locks. */

typedef struct {
    const char *name;
    uint8_t eng;                 /* 0 ALTER, 1 PHYS, 2 NOISE, 3 RESO, 4 PLATE, 5 CORDE, 6 RING (alter_ui.c ALT_ENGS) */
    int8_t e[8];
    uint8_t env[4];              /* ATK DEC SUS REL */
    uint8_t fx[4];               /* DIST CHOR DLY REV */
    uint8_t level;
    uint8_t mode;                /* the envelope mode it is made for (alter_play.c AM_*: the renders use it) */
    uint8_t rumble;              /* not 0: the sound has a RUMBLE (alter_fx.c), KNOB 4 sets it, this its start (04/11) */
    uint8_t hide;                /* 1: out of PRESETS and the random draws (Romain 05/11: WIRE = SITAR), kept for the
                                  * scenes that use it (their index) */
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
    {"BARS", 1, {PM_MODAL, 55, 110, 100, 45, 90, 0, 10}, {0, 100, 127, 80}, {15, 15, 30, 50}, 126, 2},   /* (04/11: brighter, +10 dB) */
    {"SCRAPE", 1, {PM_MODAL, 98, 100, 120, 10, 100, 127, 50}, {40, 90, 127, 90}, {10, 30, 20, 70}, 124, 3},
    {"BUZZ", 1, {PM_STRING, 30, 100, 90, 40, 100, 0, 20}, {0, 100, 127, 60}, {40, 0, 25, 30}, 124, 0},   /* (04/11: more buzz, +3 dB) */
    {"SITAR", 1, {PM_SYMP, 127, 80, 88, 64, 100, 92, 0}, {40, 100, 127, 100}, {0, 10, 20, 60}, 96, 3},
    /* NOISE */
    {"HAT", 2, {0, 0, 115, 60, 64, 0, 0, 12}, {0, 32, 0, 20}, {22, 0, 10, 15}, 127, 1},   /* (04/11: louder +4 dB, DIST, longer) */
    {"SNARE", 2, {0, 30, 85, 40, 64, 40, 0, 0}, {0, 40, 0, 30}, {10, 0, 15, 30}, 124, 1},
    {"METAL", 2, {3, 0, 100, 40, 100, 32, 0, 0}, {0, 75, 40, 60}, {0, 20, 30, 45}, 110, 1},
    {"CRUSH", 2, {0, 20, 90, 30, 64, 20, 0, 90}, {0, 50, 0, 40}, {30, 0, 20, 25}, 122, 0},
    {"RAIN", 2, {1, 30, 88, 80, 64, 105, 40, 0}, {10, 90, 127, 70}, {0, 0, 20, 70}, 110, 0},
    {"WIND", 2, {0, 64, 70, 100, 127, 0, 70, 0}, {90, 90, 110, 90}, {0, 30, 20, 80}, 127, 3},
    {"ARCADE", 2, {2, 0, 110, 90, 127, 112, 0, 70}, {0, 60, 0, 50}, {0, 0, 10, 20}, 116, 1},   /* (04/11: tuned, resonant) */
    /* (added after the others: the USER scenes keep their sounds' numbers) */
    {"WARM", 0, {30, 0, 45, AL_S, 35, 0, -30, 8}, {2, 70, 50, 60}, {0, 20, 25, 40}, 108, 2},     /* deep house chords */
    {"PAD", 0, {12, 0, 40, AL_S, 50, 0, -40, 25}, {90, 110, 110, 100}, {0, 30, 10, 70}, 127, 3}, /* a soft, slow pad */
    {"OPEN HAT", 2, {0, 0, 115, 80, 64, 0, 0, 0}, {0, 62, 0, 55}, {0, 0, 10, 20}, 126, 0},
    {"KICK", 0, {0, 0, 0, AL_L, 64, 20, 0, 0}, {0, 85, 0, 70}, {40, 0, 0, 0}, 124, 1, 70},   /* a full kick: a sine, a short
                                  * drop (~2 octaves, ~14 ms), a long body, DIST for the fat (04/11: fatter: DIST 40, +4.5 dB,
                                  * RUMBLE 45 on KNOB 4) */
    /* RESO {MATR SPRD RES DIRT OUT DCAY PING SWEP} (eng_reso.c): the four main knobs MATTER SPREAD RESONANCE DIRT */
    {"ZAP", 3, {10, 50, 100, 50, RESO_MIX, 25, RP_ALL, 80}, {0, 85, 0, 60}, {10, 0, 10, 20}, 111, 1},
    {"CLANK", 3, {50, 85, 100, 45, RESO_MIX, 20, RP_ALL, 20}, {0, 90, 127, 70}, {10, 0, 20, 30}, 112, 1},
    {"HOLLOW", 3, {70, 60, 95, 30, RESO_NOTCH, 35, RP_ALL, 0}, {0, 80, 127, 60}, {10, 0, 20, 30}, 122, 1},
    {"LARSEN", 3, {20, 10, 110, 115, RESO_MIX, 8, RP_ALL, 0}, {10, 90, 110, 60}, {10, 0, 10, 20}, 106, 2},
    {"FRICTION", 3, {115, 100, 105, 60, RESO_MIX, 60, RP_CYCLE, 0}, {20, 100, 110, 70}, {0, 10, 20, 40}, 94, 0},
    {"GROWL", 3, {95, 30, 100, 95, RESO_LOW, 40, RP_ALL, 0}, {10, 100, 110, 60}, {20, 0, 10, 20}, 97, 0},
    /* PLATE {MATR SPRD RES DIRT TONE DCAY BEND HIT} (eng_plate.c, 05/11): 2 struck, 2 textures */
    {"GONG", 4, {20, 15, 115, 10, 70, 20, 50, 10}, {0, 110, 127, 110}, {0, 0, 10, 35}, 98, 0},
    {"TOLE", 4, {60, 90, 100, 45, 110, 8, 0, 30}, {0, 90, 0, 70}, {10, 0, 10, 20}, 115, 1},
    {"SCRAPE", 4, {110, 70, 105, 30, 80, 40, 0, 0}, {20, 100, 110, 90}, {0, 0, 20, 40}, 97, 0},
    {"CRACKED", 4, {30, 127, 118, 70, 90, 12, 15, 5}, {0, 110, 127, 100}, {0, 0, 10, 30}, 115, 2},
    /* CORDE {MATR SPRD RES DIRT TONE DCAY BUZZ BEND} (eng_corde.c, 05/11) */
    {"WIRE", 5, {85, 40, 110, 20, 90, 10, 0, 0}, {0, 100, 127, 80}, {0, 0, 10, 20}, 122, 1, 0, 1},   /* (hidden) */
    {"SITAR", 5, {71, 15, 115, 25, 100, 8, 80, 10}, {0, 100, 127, 80}, {0, 0, 10, 20}, 122, 1},
    {"BOW", 5, {124, 30, 120, 40, 70, 30, 0, 0}, {30, 110, 120, 90}, {0, 10, 10, 40}, 120, 0},
    {"BROKEN", 5, {20, 127, 118, 100, 75, 40, 110, 90}, {0, 90, 60, 60}, {0, 0, 10, 20}, 127, 2},
    /* RING {MATR SPRD RES DIRT RDIO TONE BLND SWEP} (eng_ring.c, 05/11) */
    {"RADIO", 6, {40, 50, 60, 30, 13, 70, 20, 0}, {10, 100, 110, 60}, {0, 0, 10, 20}, 115, 0},
    {"RING BELL", 6, {0, 70, 30, 10, 0, 90, 0, 0}, {0, 90, 0, 70}, {0, 0, 10, 30}, 106, 1},
    {"LASER", 6, {64, 60, 90, 60, 0, 60, 30, 110}, {0, 70, 0, 50}, {0, 0, 10, 20}, 110, 1},
    {"SIREN", 6, {70, 118, 105, 45, 13, 75, 70, 50}, {60, 100, 127, 110}, {10, 0, 10, 30}, 110, 0},
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
        uint8_t dly;             /* the DELAY send (0: the sound's own) */
        uint8_t phold, pdec;     /* PERC: its HOLD and DECAY (0: the defaults, alter_lfo.c AE_DEF) */
        uint8_t lstep, ldly;     /* step lstep - 1 sent ldly more into the delay (the SEQ menu's DELAY lane; 0: none) */
    } c[AS_NCIRC];
    uint8_t dpre, rpre, mpre;    /* the FX page's DELAY, REVERB and MASTER presets (alter_fx.c AFX_DLY AFX_REV AFX_MST):
                                  * all 0 (Felucca's own: ECHO, HALL, FLAT), Romain's choice, the effects his to add */
    const uint8_t *user;         /* not 0: a USER scene of Romain's instead, whole (alter_user_*.h, alter_store.c) */
} alt_scene_t;
#include "alter_user_ambient.h"   /* AMBIENT: his USER 01 (31/10) */

static const alt_scene_t ALT_SCENE[] = {
    {"TECHNO", "PERC . STAB . TEXTURE", 132, AS_SC_PHRY, 0,
     {{"KICK", 1, 16, 4, 0, 127, 36, AS_CH_OFF, -12, 0, 45, 85, 13, 51},   /* the 4th kick 40 % into the delay */
      {"STAB", 2, 16, 3, 3, 127, 48, AS_CH_TRI, 0, 0},
      {"CRUSH", 0, 16, 11, 0, 110, 60, AS_CH_OFF, 0, 0}}, 0, 0, 0},
    {"HOUSE", "DEEP . WARM CHORDS . OPEN HAT", 122, AS_SC_DOR, 8,
     {{"KICK", 1, 16, 4, 0, 127, 36, AS_CH_OFF, -12, 0, 45, 85, 0, 0},   /* four to the floor */
      {"WARM", 2, 16, 3, 3, 127, 48, AS_CH_7TH, -4, 0},     /* minor 7th stabs off the beat (steps 3 9 14) */
      {"OPEN HAT", 0, 16, 4, 2, 127, 60, AS_CH_OFF, -6, 0}}, 0, 0, 0},   /* between the kicks */
    {"AMBIENT", "ROMAIN'S USER 01", 70, AS_SC_MIN, 0,
     {{"PAD", 3, 32, 1, 0, 127, 48, AS_CH_OCT, 0, 0}, {"PAD", 3, 24, 1, 12, 127, 55, AS_CH_OFF, 0, 0},
      {"RAIN", 0, 16, 3, 0, 70, 72, AS_CH_OFF, 0, 0}}, 0, 0, 0, ALT_USER_AMBIENT},   /* (the rest unused: user) */
    {"INDUS", "METAL . NOISE . GONG", 128, AS_SC_PHRY, 0,
     {{"ANVIL", 1, 16, 5, 0, 127, 36, AS_CH_OFF, 0, 0},
      {"METAL", 1, 16, 7, 1, 127, 55, AS_CH_OFF, 0, 0},
      {"GONG", 0, 16, 3, 2, 100, 40, AS_CH_OFF, 0, 0}}, 0, 0, 0},
};
#define ALT_NSCENE (sizeof ALT_SCENE / sizeof ALT_SCENE[0])
