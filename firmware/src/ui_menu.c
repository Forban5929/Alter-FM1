/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca menu (HOME held): its rows in tabs (1.0.5: DISPLAY CONTROL AUDIO SYSTEM; 1.2: MIDI before SYSTEM; ALGORITHM
 * between them, PRESETS within one; no scrolling), the rows in menu_items.c (the settings, shared with the editor's
 * MENU_DESC / MENU_SET; 1.2: TUNE and MIDI IN, the project's). No BACK row: HOME closes it (ui_input.c). The two-valued
 * rows are bits of ui_prefs (MENU_FLAGS). PRESETS scrolls from ABOUT through all credits (OCT- back to the tabs).
 * ui.menu: 1 the tabs, 2 ABOUT (information and credits), 3 INFO (1.2: the device's state, the old SYSTEM page's USB and
 * CPU readings). */
/* ------------------------------------------------------------ menu --- */
/* (the rows, their values and names, usb_serial_apply: menu_items.c) */



/* All notices are in flash. Wrap at the font's actual width, without dropping URL
 * characters; the same layout computes the scroll limit and draws clipped bands.
 * Mixed case here (long text); S throughout, the title in L. */
#define MENU_DOC_H 196                                /* the document: screen rows 24..220 */
enum { MC_TEXT, MC_TITLE, MC_URL };
typedef struct { const char *text; uint8_t kind; } menu_credit_t;
static const menu_credit_t MENU_CREDITS[] = {
    {"FELUCCA", MC_TITLE},                   /* what ALTER is built on */
    {"Leo Kuroshita, H\xFCgelton Instruments / GPL-3.0-only", MC_TEXT},
    {"https://github.com/hugelton/Felucca", MC_URL},
    {"", MC_TEXT},
    {"FELUCCA-ENGINES", MC_TITLE},
    {"NOISE: Leo Kuroshita / MIT", MC_TEXT},
    {"Fukiai icon font: MIT", MC_TEXT},
    {"https://github.com/hugelton", MC_URL},
    {"", MC_TEXT},
    {"FELUCCA-BSP", MC_TITLE},
    {"The hardware layer: Leo Kuroshita, Charles Vestal, Kerem Kilic and others (its NOTICE) / MPL-2.0", MC_TEXT},
    {"https://github.com/hugelton/felucca-bsp", MC_URL},
    {"", MC_TEXT},
    {"DAISYSP", MC_TITLE},
    {"Electrosmith, Emilie Gillet", MC_TEXT},
    {"MIT", MC_TEXT},
    {"https://github.com/electro-smith/DaisySP", MC_URL},
    {"", MC_TEXT},
    {"RINGS", MC_TITLE},
    {"Emilie Gillet / MIT", MC_TEXT},
    {"https://github.com/pichenettes/eurorack", MC_URL},
    {"", MC_TEXT},
    {"INTER TIGHT", MC_TITLE},
    {"The Inter Project Authors", MC_TEXT},
    {"SIL Open Font License 1.1", MC_TEXT},
    {"https://github.com/rsms/inter-tight", MC_URL},
    {"", MC_TEXT},
    {"TEX GYRE PAGELLA", MC_TITLE},
    {"GUST e-foundry / GUST Font License", MC_TEXT},
    {"", MC_TEXT},
    {"IBM PLEX MONO", MC_TITLE},
    {"IBM Corp. / SIL Open Font License 1.1", MC_TEXT},
};

static int32_t menu_text(const char *s, int32_t y, uint16_t col, int draw)
{
    while (*s) {
        char line[64];
        uint32_t n = 0, space = 0, take, prev = 0;
        int32_t pen = 0;                                 /* 1/16 px, as text_w adds it up */
        while (s[n] && n < sizeof line - 1u) {
            uint32_t c = (uint8_t)s[n];
            line[n] = s[n];
            line[n + 1u] = 0;
            pen += (prev ? kern(&AF_S, prev, c) : 0) + AF_S.g[glyph(&AF_S, c)].adv;
            prev = c;
            if ((pen + 8) >> 4 > 226)
                break;
            if (s[n] == ' ')
                space = n;
            n++;
        }
        take = s[n] && space ? space : n;
        if (!take) take = 1;                         /* progress even for an unusually wide glyph */
        line[take] = 0;
        if (draw && y + AF_S.h > 0 && y < MENU_DOC_H)
            cv_text(6, y, &AF_S, line, col);
        y += 18;
        s += take;
        while (*s == ' ') s++;
    }
    return y;
}

static int32_t menu_document(int32_t y, int draw)
{
    uint32_t i;
    if (draw) {
        cv_text(6, y + 2, &AF_L, "ALTER", T_THEME);
        cv_text(6, y + 38, &AF_S, "Industrial / noise / texture instrument", T_TEXT);
        cv_text(6, y + 58, &AF_M, FELUCCA_VERSION, T_THEME);
        cv_text_r(232, y + 61, &AF_S, __DATE__, T_MID, T_BG);
        cv_text(6, y + 82, &AF_S, "Built on Felucca", T_TEXT);
        cv_text(6, y + 100, &AF_S, "(C) 2026 Leo Kuroshita", T_TEXT);
        cv_text(6, y + 118, &AF_S, "GPL-3.0-only", T_TEXT);
        cv_text(6, y + 138, &AF_S, "github.com/Forban5929", T_MID);
        cv_text(6, y + 156, &AF_S, "/alter-fm1", T_MID);
    }
    y = menu_text("CREDITS", y + 216, T_THEME, draw) + 8;
    for (i = 0; i < NELEM(MENU_CREDITS); i++) {
        const menu_credit_t *c = &MENU_CREDITS[i];
        const char *text = c->text;
        /* Display host/path once; the full source URL stays in the flash table. */
        if (c->kind == MC_URL && str_len(text) >= 8u && !memcmp(text, "https://", 8))
            text += 8;
        if (!text[0]) y += 8;
        else y = menu_text(text, y, c->kind == MC_TITLE ? T_THEME : c->kind == MC_URL ? T_MID : T_TEXT, draw);
    }
    y = menu_text("LICENSES + SOURCE", y + 12, T_THEME, draw);
    y = menu_text("ALTER and Felucca: GPL-3.0-only; Felucca-Engines MIT; felucca-bsp MPL-2.0", y, T_TEXT, draw);
    y = menu_text("Fonts and icons: their licences travel with the source and the release files.", y, T_TEXT, draw);
    y = menu_text("NO WARRANTY", y, T_TEXT, draw);
    y = menu_text("Full notices in source:", y + 8, T_TEXT, draw);
    y = menu_text("github.com/Forban5929/alter-fm1", y, T_MID, draw);
    return y + 4;
}


/* The page (1.0.5): the tabs across the top, the rows of the one shown below, the key hints at the bottom.
 *   header   0..24  the cog and MENU, the REC mark, HOME CLOSE, the tabs as dots at the right (the one shown a THEME
 *                   pill)
 *   tabs    29..53  a 24 px cushion per tab: its icon alone (SURF, MID); the one shown wider, with its name (ACCENT,
 *                   INK; S, LARGE M), MT_GAP between any two. With ANIM ON the cushions grow and shrink into place
 *                   (mt_follow), the gaps kept
 *   rows    59..    one SURF panel (FLAT; LINE: none, a rule under the tabs), a row a label and its value, 1 px rules
 *                   between them; the selected row THEME with INK, as the browser's. A value THEME (M) at the right,
 *                   ON / OFF as a switch, COLOR its palette's five colours, CALIBRATION and ABOUT a chevron (OCT+
 *                   opens them); the labels in M too (a list's rows). LARGE: 28 px rows (a tab of up to 5)
 *   keys   224      ALGO TAB, PRESETS ROW, OCT-/+ VALUE (any of KNOB 1..4 too; CALIBRATION, ABOUT: OCT+ OPEN). HOME
 *                   CLOSE is in the header: the four would not fit in S (cv_key_row drops words, from the last)
 * Drawn in two bands (the canvas holds 124 rows) split in a gap between two rows; while the tabs slide only their
 * strip (24..58) is drawn again */
static const uint16_t MTAB_ICON[MTAB_COUNT] = {ICON_X_EYE, ICON_X_KNOB, ICON_X_SPEAKER, ICON_MIDI, ICON_X_COG};
#define MT_Y 29                                        /* the tab bar */
#define MT_H 24
#define MT_IW 26                                       /* a cushion with its icon alone (the icon centred in it; 1.2, five
                                                        * tabs: 26, up to 1.1.5 32) */
#define MT_NX 24                                       /* the name's pen in the one shown; 10 px after it */
#define MT_X0 6
#define MT_X1 234
#define MT_GAP 5                                       /* between two tabs, always (the bar centred in MT_X0..MT_X1; 1.2, five
                                                        * tabs: 5, up to 1.1.5 8) */
#define MT_ONE 64                                      /* mt.pos per tab */
#define MP_Y 59                                        /* the rows' panel */
#define MP_PAD 3
#define MR_X 8                                         /* a row */
#define MR_W 224
#define MR_LX 16                                       /* its label */
#define MR_VR 224                                      /* its value's right edge */
#define MK_Y 224                                       /* the key hints */

/* the tab bar's place: the tab shown at pos tab * MT_ONE; with ANIM ON it follows in halving steps, a frame each
 * (a frame missed, as when the menu opens: it snaps, as ui_graph.c pr_follow) */
static struct { int16_t pos; uint32_t frame; } mt;

/* MENU > SYSTEM > INFO (1.2, ui.menu 3): what the old GLO > SYSTEM page showed, read only: the firmware, the USB link (the
 * SYSTEM page's MIDI card: OFF no USB, WAIT no bus yet, BUS frames seen, ENUM being set up, MIDI in use) and the CPU load
 * of the audio (its SYSTEM card; the shown value follows every INFO_CPU_MS, not every frame). The rows as the menu's (no
 * tabs: from INFO_Y), the selected look on none; OCT- back to the tabs, HOME closes */
#define INFO_Y 32
#define INFO_N 3u
#define INFO_CPU_MS 500u
static struct { uint8_t cpu; uint32_t t; } info;



