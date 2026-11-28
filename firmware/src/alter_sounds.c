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
    uint8_t eng;                 /* 0 ALTER, 1 PHYS, 2 NOISE, 3 RESO, 4 PLATE, 5 CORDE, 6 RING, 7 WAVE (alter_ui.c ALT_ENGS) */
    int8_t e[8];
    uint8_t env[4];              /* ATK DEC SUS REL */
    uint8_t fx[4];               /* DIST CHOR DLY REV */
    uint8_t level;
    uint8_t mode;                /* the envelope mode it is made for (alter_play.c AM_*: the renders use it) */
    uint8_t rumble;              /* not 0: the sound has a RUMBLE (alter_fx.c), KNOB 4 sets it, this its start (04/11) */
    uint8_t hide;                /* 1: out of PRESETS and the random draws (Romain 05/11: WIRE = SITAR), kept for the
                                  * scenes that use it (their index) */
    uint8_t own;                 /* (v1.2.20, Romain 28/11 "by default"): picked by hand (PRESETS, the page), AP_MODE its mode,
                                  * AP_FX its fx[] (v1.18 cleared them for every sound), AP_CHORD its chord (alter_ui.c
                                  * alt_pick_sound); the scenes and the draws set theirs as before */
    uint8_t chord;               /* AS_CH_* with AP_CHORD */
} alt_pre_t;
#define AP_MODE 1u
#define AP_FX 2u
#define AP_CHORD 4u

/* ALTER {MRPH SPRD HARM MODE AMNT NOIS TILT DRFT}: MODE 0 SKIN, 1 LIQUID, 2 METAL */
#define AL_S 0
#define AL_L 1
#define AL_M 2
/* PHYS {MODEL ..}: MODAL {STRC BRIT DAMP POS ACC BOW EXC}, STRNG the same, MEMB {HARM BRIT DAMP POS ACC BEND EXC},
 * SYMP {CHRD BRIT DAMP SYMP ACC BUZZ EXC} */
/* NOISE {MODE COLR FREQ RES TRK DENS DRFT CRSH}: MODE 0 ANLG, 1 DUST, 2 LFSR, 3 META */
static const alt_pre_t ALT_PRE[] = {
    /* ALTER */
    {"THUD", 0, {5, 118, 11, AL_M, 44, 12, 3, 51}, {3, 65, 41, 75}, {10, 0, 0, 10}, 127, 0},   /* v1.2.20 Romain 28/11 */
    {"ANVIL", 0, {40, 44, 127, AL_M, 103, 86, 63, 0}, {0, 77, 12, 58}, {20, 0, 15, 30}, 89, 0},   /* v1.2.20 Romain: -6 dB */
    {"STAB", 0, {0, 15, 109, AL_S, 0, 0, -45, 0}, {0, 55, 40, 40}, {25, 0, 20, 20}, 105, 2, .own = AP_MODE | AP_FX},   /* v1.2.20: REV + DLY 20 */
    {"PIPES", 0, {0, 0, 36, AL_S, 127, 0, 20, 2}, {0, 90, 60, 70}, {0, 10, 30, 45}, 101, 2},   /* v1.2.20: -3 dB */
    {"RUST", 0, {70, 120, 110, AL_M, 77, 117, 10, 37}, {30, 127, 127, 85}, {30, 0, 30, 60}, 85, 2},   /* v1.2.20: -7 dB */
    {"GRIND", 0, {127, 90, 20, AL_S, 127, 63, -7, 113}, {10, 100, 90, 70}, {60, 50, 20, 40}, 88, 2, .own = AP_MODE | AP_FX},   /* v1.2.20: CHORUS 50, STAB, -6 dB */
    /* PHYS */
    {"TOMS", 1, {PM_MEMB, 0, 52, 40, 30, 100, 40, 0}, {0, 100, 127, 70}, {0, 0, 0, 40}, 105, 1},
    {"GONG", 1, {PM_MODAL, 100, 90, 112, 10, 100, 0, 0}, {0, 100, 127, 90}, {0, 0, 20, 55}, 107, 0},
    {"BARS", 1, {PM_MODAL, 55, 110, 100, 45, 90, 0, 0}, {0, 100, 127, 80}, {15, 15, 30, 50}, 127, 2},   /* (04/11: brighter, +10 dB) */
    {"SCRAPE", 1, {PM_MODAL, 98, 100, 120, 10, 100, 127, 0}, {40, 90, 127, 90}, {10, 30, 20, 70}, 112, 3},
    {"BUZZ", 1, {PM_STRING, 30, 100, 90, 40, 100, 0, 0}, {0, 100, 127, 60}, {40, 0, 25, 30}, 125, 0},   /* (04/11: more buzz, +3 dB) */
    {"SITAR", 1, {PM_SYMP, 127, 80, 88, 64, 100, 92, 0}, {40, 100, 127, 100}, {0, 10, 20, 60}, 95, 3},
    /* NOISE */
    {"HAT", 2, {0, 0, 115, 60, 64, 0, 0, 12}, {0, 32, 0, 20}, {22, 0, 10, 15}, 81, 1},   /* (04/11: louder +4 dB, DIST, longer) */
    {"SNARE", 2, {0, 30, 85, 40, 64, 10, 0, 0}, {0, 40, 0, 30}, {10, 0, 15, 30}, 99, 1},
    {"METAL", 2, {3, 0, 100, 40, 100, 32, 0, 0}, {0, 75, 40, 60}, {0, 20, 30, 45}, 80, 1},
    {"CRUSH", 2, {0, 20, 90, 30, 64, 5, 0, 90}, {0, 50, 0, 40}, {30, 0, 20, 25}, 119, 0},
    {"RAIN", 2, {1, 30, 88, 80, 64, 97, 0, 0}, {10, 90, 127, 70}, {0, 0, 20, 70}, 90, 0},
    {"WIND", 2, {0, 64, 70, 100, 127, 0, 0, 0}, {90, 90, 110, 90}, {0, 30, 20, 80}, 111, 3},
    {"ARCADE", 2, {2, 0, 110, 90, 127, 112, 0, 70}, {0, 60, 0, 50}, {0, 0, 10, 20}, 70, 1},   /* (04/11: tuned, resonant) */
    /* (added after the others: the USER scenes keep their sounds' numbers) */
    {"WARM", 0, {30, 0, 45, AL_S, 127, 0, -30, 54}, {69, 0, 118, 82}, {0, 20, 25, 40}, 105, 0, .own = AP_MODE | AP_FX},   /* v1.2.20: REV 40, FREE, -2 dB */
    {"PAD", 0, {0, 34, 0, AL_S, 127, 79, -40, 6}, {75, 127, 127, 127}, {0, 30, 127, 127}, 73, 0, .own = AP_FX | AP_CHORD, .chord = AS_CH_TRI},   /* v1.2.20: TRIAD, REV + DLY max, -10 dB */
    {"OPEN HAT", 2, {0, 0, 115, 80, 64, 0, 0, 0}, {0, 62, 0, 55}, {0, 0, 10, 20}, 98, 0},
    {"KICK", 0, {10, 25, 2, AL_L, 46, 10, 0, 0}, {0, 85, 0, 70}, {30, 0, 0, 0}, 111, 1, 30, .own = AP_MODE | AP_FX},   /* v1.2.20: DIST 30, PERC, RUMBLE 30, -1 dB */
    /* RESO {MATR SPRD RES DIRT OUT DCAY PING SWEP} (eng_reso.c): the four main knobs MATTER SPREAD RESONANCE DIRT */
    {"ZAP", 3, {10, 50, 100, 0, RESO_MIX, 58, RP_ALL, 80}, {0, 85, 0, 60}, {10, 0, 10, 20}, 106, 1},
    {"CLANK", 3, {50, 85, 100, 0, RESO_MIX, 49, RP_ALL, 20}, {0, 90, 127, 70}, {10, 0, 20, 30}, 110, 1},
    {"HOLLOW", 3, {70, 60, 95, 0, RESO_NOTCH, 72, RP_ALL, 0}, {0, 80, 127, 60}, {10, 0, 20, 30}, 88, 1},
    {"LARSEN", 3, {20, 10, 110, 0, RESO_MIX, 19, RP_ALL, 0}, {10, 90, 110, 60}, {10, 0, 10, 20}, 112, 2},
    {"FRICTION", 3, {115, 100, 105, 0, RESO_MIX, 95, RP_CYCLE, 0}, {20, 100, 110, 70}, {0, 10, 20, 40}, 109, 0},
    {"GROWL", 3, {95, 30, 100, 0, RESO_LOW, 78, RP_ALL, 0}, {10, 100, 110, 60}, {20, 0, 10, 20}, 100, 0},
    /* PLATE {MATR SPRD RES DIRT TONE DCAY BEND HIT} (eng_plate.c, 05/11): 2 struck, 2 textures (v1.08: MATR scaled to
     * 0..84 and TONE exponential, the values converted to sound as before; SCRAPE redone as a hard hit with a long
     * burst and BEND, its scrape is gone, Romain 22/11) */
    {"GONG", 4, {30, 15, 115, 10, 106, 20, 50, 3}, {0, 110, 127, 110}, {0, 0, 10, 35}, 101, 0},
    {"TOLE", 4, {91, 90, 100, 45, 123, 8, 0, 8}, {0, 90, 0, 70}, {10, 0, 10, 20}, 104, 1},
    {"SCRAPE", 4, {127, 70, 105, 30, 111, 95, 60, 20}, {20, 100, 110, 90}, {0, 0, 20, 40}, 97, 0},
    {"CRACKED", 4, {45, 127, 118, 70, 115, 12, 15, 1}, {0, 110, 127, 100}, {0, 0, 10, 30}, 123, 2},
    /* CORDE {MATR SPRD RES DIRT TONE DCAY BUZZ BEND} (eng_corde.c, 05/11) */
    {"WIRE", 5, {118, 40, 110, 0, 90, 10, 0, 0}, {0, 100, 127, 80}, {0, 0, 10, 20}, 102, 1, 0, 1},   /* (hidden) */
    {"SITAR", 5, {118, 15, 115, 0, 100, 8, 80, 10}, {0, 100, 127, 80}, {0, 0, 10, 20}, 104, 1},
    {"BOW", 5, {124, 30, 120, 0, 70, 30, 0, 0}, {30, 110, 120, 90}, {0, 10, 10, 40}, 101, 0},
    {"BROKEN", 5, {20, 127, 118, 0, 75, 40, 110, 90}, {0, 90, 60, 60}, {0, 0, 10, 20}, 122, 2},
    /* RING {MATR SPRD RES DIRT RDIO TONE BLND SWEP} (eng_ring.c, 05/11) */
    {"RADIO", 6, {40, 50, 60, 0, 13, 70, 20, 0}, {10, 100, 110, 60}, {0, 0, 10, 20}, 127, 0},
    {"RING BELL", 6, {0, 70, 30, 0, 0, 90, 0, 0}, {0, 90, 0, 70}, {0, 0, 10, 30}, 111, 1},
    {"LASER", 6, {64, 60, 90, 0, 0, 60, 30, 110}, {0, 70, 0, 50}, {0, 0, 10, 20}, 117, 1},
    {"SIREN", 6, {70, 118, 105, 0, 13, 75, 70, 50}, {60, 100, 127, 110}, {10, 0, 10, 30}, 101, 0},
    /* WAVE {TABL POS MODE AMNT PENV WENV SUB SPRD} (eng_wave.c, v1.16): TABL 0 CLNG 1 RUST 2 SHRD 3 FDBK; MODE 0 SYNC
     * 1 BEND 2 FM 3 RING 4 FOLD 5 BITS */
    {"HAMMER", 7, {0, 30, 0, 20, 90, 70, 40, 0}, {0, 60, 0, 45}, {10, 0, 10, 20}, 105, 1},
    {"BENDER", 7, {0, 80, 1, 30, 70, 100, 60, 0}, {0, 70, 0, 50}, {0, 0, 15, 25}, 106, 1},
    {"CORRODE", 7, {1, 60, 4, 50, 30, 30, 0, 40}, {10, 100, 90, 70}, {0, 10, 20, 40}, 107, 3},
    {"WAVE RING", 7, {1, 20, 3, 90, 60, 30, 20, 0}, {0, 80, 0, 55}, {0, 0, 10, 30}, 109, 1},
    {"SHREDDER", 7, {2, 40, 5, 60, 50, 60, 30, 0}, {0, 70, 40, 45}, {10, 0, 20, 20}, 127, 2},
    {"HOWL", 7, {3, 70, 2, 40, 20, 20, 0, 60}, {30, 110, 110, 80}, {0, 10, 20, 45}, 111, 0},
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
    uint8_t dpre, rpre, mpre;    /* the FX page's DELAY, REVERB (v1.20: in the list before v1.18, HALL first: alter_store.c
                                  * OLD_REV) and MASTER presets (alter_fx.c AFX_DLY AFX_REV AFX_MST):
                                  * all 0 (Felucca's own: ECHO, HALL, FLAT), Romain's choice, the effects his to add */
    const uint8_t *user;         /* not 0: a USER scene of Romain's instead, whole (alter_user_*.h, alter_store.c) */
    int8_t fix[AS_NCIRC];        /* (v1.20) each circle's LEVEL corrected, 0.5 dB steps: the mix as before 22/11 (alter_store.c);
                                  * v1.34: redone for the new sound LEVELs and engine gains (the mix of v1.33, 9 dB under the MASTER) */
    const uint8_t *blob;         /* (v1.2.22) not 0: a whole scene of Romain's, the page's patch file (alt_blob_t), blob_n bytes:
                                  * loaded as the page's BLOB is, everything in it (LFOs, lanes, added steps, REC, CONSOLE..);
                                  * none now (v1.2.23: HOUSE back as it was), kept for the next one (a .h from the file's base64) */
    uint16_t blob_n;
} alt_scene_t;
#include "alter_user_ambient.h"   /* AMBIENT: his USER 01 (31/10) */

static const alt_scene_t ALT_SCENE[] = {
    {"TECHNO", "PERC . STAB . TEXTURE", 132, AS_SC_PHRY, 0,
     {{"KICK", 1, 16, 4, 0, 127, 36, AS_CH_OFF, -12, 0, 45, 85, 13, 51},   /* the 4th kick 40 % into the delay */
      {"STAB", 2, 16, 3, 3, 127, 48, AS_CH_TRI, 0, 0},
      {"CRUSH", 0, 16, 11, 0, 110, 60, AS_CH_OFF, 0, 0}}, 0, 0, 0, 0, {14, -14, -19}},
    {"HOUSE", "DEEP . WARM CHORDS . OPEN HAT", 122, AS_SC_DOR, 8,
     {{"KICK", 1, 16, 4, 0, 127, 36, AS_CH_OFF, -12, 0, 45, 85, 0, 0},   /* four to the floor */
      {"WARM", 2, 16, 3, 3, 127, 48, AS_CH_7TH, -4, 0},     /* minor 7th stabs off the beat (steps 3 9 14) */
      {"OPEN HAT", 0, 16, 4, 2, 127, 60, AS_CH_OFF, -6, 0}}, 0, 0, 0, 0, {14, -28, 4}},   /* between the kicks */
    {"AMBIENT", "TWO PAD DRONES . RAIN", 70, AS_SC_MIN, 0,   /* (v1.24, Romain: its engines, not his name) */
     {{"PAD", 3, 32, 1, 0, 127, 48, AS_CH_OCT, 0, 0}, {"PAD", 3, 24, 1, 12, 127, 55, AS_CH_OFF, 0, 0},
      {"RAIN", 0, 16, 3, 0, 70, 72, AS_CH_OFF, 0, 0}}, 0, 0, 0, ALT_USER_AMBIENT, {-27, -28, -23}},   /* (the rest unused: user) */
    {"INDUS", "METAL . NOISE . GONG", 128, AS_SC_PHRY, 0,
     {{"ANVIL", 1, 16, 5, 0, 127, 36, AS_CH_OFF, 0, 0},
      {"METAL", 1, 16, 7, 1, 127, 55, AS_CH_OFF, 0, 0},
      {"GONG", 0, 16, 3, 2, 100, 40, AS_CH_OFF, 0, 0}}, 0, 0, 0, 0, {4, 6, -10}},
};
#define ALT_NSCENE (sizeof ALT_SCENE / sizeof ALT_SCENE[0])
