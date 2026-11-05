/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI: NAME, naming a user preset or a project on the device (included by ui_input.c).
 *
 * Where: SAVE > USER, SAVE (KNOB 4, OCT+) and SAVE > PROJECT, SAVE: after the OVERWRITE? dialog when the slot is
 * used, the NAME screen opens before anything is written; OCT+ writes, OCT- cancels (back to the page, nothing
 * written). EDIT on those pages renames the selected slot (only its name is written). The name is prefilled:
 * USER the sound's own (the user preset it came from) or the automatic one ("DIGITAL 14"), PROJECT the current
 * project's (as loaded or last saved; none: "PROJECT A"), so SAVE, OCT+, OCT+ saves with the default name.
 * Writing needs the transport stopped (STOP TO SAVE; the screen stays).
 *
 * The keys type and never sound, record or send MIDI (seq.c keyboard_block: song.grid 2). Upper case, at most 12:
 *   white keys   ABC (letters): AB CD EF GH IJK LM NO PQ RS TU VW XYZ, then 123 456 789 0-. ; phone style: a tap
 *                types the group's first character, another tap of the same key within 0.8 s the next one
 *                (cycling); another key or 0.8 s without a tap keeps it and moves on.
 *                123 (digits and symbols): 1 2 3 4 5 6 7 8 9 0 - . _ / # + , one tap each.
 *   black keys   by name, in both octaves: F# cursor left, G# SPACE, A# cursor right, C# DELETE (the character
 *                before the cursor; held: repeats, as the arrows do), D# ABC / 123.
 *   KNOB 1       the cursor; KNOB 2 the character at the cursor (SPACE A..Z 0..9 - . _ / # +; at the end: a new one).
 *   KNOB 3       (1.2, Discussion #90) USER: the sound's category (BASS LEAD PAD PLUCK KEYS DRUM FX OTHER, category.c),
 *                shown in the field's title row and written with the name (PRESETS > LIST filters by it); it starts at
 *                the slot's own (a rename) or the sound's (a save: its user preset's, its factory preset's).
 *                PROJECT: a random name, a new one each detent (an adjective and a noun, NM_RA / NM_RB; the field
 *                takes it whole, then the keys edit it as any other).
 *   LEDs         every key that types or edits is lit; the key whose letters are being cycled blinks.
 * Spaces at either end are dropped when it is written; an empty name: USER the automatic one, PROJECT none. */
enum { NK_NONE, NK_USER_SAVE, NK_USER_RENAME, NK_PROJ_SAVE, NK_PROJ_RENAME };
#define NM_LEN 12u
#define NM_TAP_MS 800u                                 /* multi-tap: the next letter within this */
#define NM_REP_MS 450u                                 /* a held arrow / DELETE repeats after this, */
#define NM_RATE_MS 90u                                 /* .. every this */
enum { NB_LEFT, NB_SPACE, NB_RIGHT, NB_DEL, NB_MODE, NB_NONE };
static const char *const NM_ABC[16] = {"AB", "CD", "EF", "GH", "IJK", "LM", "NO", "PQ", "RS", "TU", "VW", "XYZ",
                                       "123", "456", "789", "0-."};
static const char NM_NUM[17] = "1234567890-._/#+";
static const char NM_SET[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._/#+";   /* KNOB 2 */
static struct {
    uint8_t kind;                                      /* NK_*: NK_NONE = closed */
    uint8_t slot;
    uint8_t len, cur;                                  /* the name's length; the cursor 0..len */
    uint8_t num;                                       /* the white keys: 0 ABC, 1 123 */
    uint8_t key;                                       /* the white key (place + 1) of the letter being cycled, 0 none */
    uint8_t tap;                                       /* .. that letter's index in its group */
    uint8_t rep;                                       /* the key (+ 1) of a held arrow / DELETE, 0 none */
    uint32_t t, rep_t;                                 /* fm1_ms of the last tap; of the next repeat */
    uint8_t cat;                                       /* USER: the category (CAT_BASS..CAT_OTHER) KNOB 3 sets */
    uint32_t rnd;                                      /* PROJECT: KNOB 3's random names (xorshift; 0 = not seeded) */
    uint32_t sig;                                      /* drawn-state cache */
    char s[NM_LEN + 1u];
    char ph[NM_LEN + 1u];                              /* what an empty name saves as / shows ("PROJECT A") */
} nm __attribute__((section(".pool")));                /* (zero-initialised; main loop only: off the audio code's .bss) */

static int name_on(void) { return nm.kind != NK_NONE; }

static uint32_t nm_black(uint32_t k)                   /* black key k's function: by its name (F# .. D#) */
{
    switch ((k + 5u) % 12u) {
    case 6: return NB_LEFT;
    case 8: return NB_SPACE;
    case 10: return NB_RIGHT;
    case 1: return NB_DEL;
    case 3: return NB_MODE;
    default: return NB_NONE;
    }
}



/* PROJECT, KNOB 3: random names, adjective + noun, at most 5 + 1 + 6 characters (no trademarks, no names) */
static const char *const NM_RA[16] = {"NEON", "GLASS", "AMBER", "SOLAR", "LUNAR", "SILK", "IRON", "PALE",
                                      "DEEP", "WILD", "QUIET", "SLOW", "GOLD", "BLUE", "DUSTY", "HAZY"};
static const char *const NM_RB[16] = {"DRIFT", "PULSE", "ORBIT", "RIVER", "HARBOR", "TIDE", "ECHO", "GARDEN",
                                      "SIGNAL", "MIRROR", "CANAL", "FIELD", "COAST", "CLOUD", "BLOOM", "SAIL"};



/* the key LEDs: every key that does something lit, the key whose letters are cycling blinks */
static uint32_t name_leds(void)
{
    uint32_t k, m = 0, blink = ((fm1_ms / 250u) & 1u) == 0u;
    for (k = 0; k < 27u; k++) {
        uint32_t on = key_black(k) ? nm_black(k) != NB_NONE : nm.key != key_place(k) + 1u || blink;
        m |= on << k;
    }
    return m;
}

/* ----------------------------------------------------------- drawing --- */
/* The header stays. The card band is the name field: a SURF card, "SAVE U07" / "NAME PROJECT A" and the length,
 * then 12 cells (M): the cursor an ACCENT box, the letter being cycled a THEME cell (INK), a space a DIM dot, an
 * empty name its placeholder in DIM. The panel: the cycling key's letters (the one typed THEME, the next ACCENT)
 * or a hint, and ABC / 123; the white keys as two rows of 8 pills (the left octave, then the right one; the
 * cycling key THEME); the black keys' functions as five KEY cells under their names. The footer: OCT+ / OCT-,
 * then the knobs and the keys */
#define NM_CX(i) (12 + 18 * (int32_t)(i))              /* cell i: 18 x 22 at y 18 of the card */
