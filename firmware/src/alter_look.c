/* SPDX-License-Identifier: GPL-3.0-only */
/* ALTER's look (01/11, Romain's choice "A · ALCHIMIE"): every screen is a tarot card. NIGHT: black card, gold leaf,
 * a burgundy ribbon; DAY: a cream card (its paper grain: alt_paper), indigo ink, gold. A double frame with fan corners,
 * the screen's name in a ribbon at the top, its arcana's numeral at the top left (PLAY XIV, SEQ X, FX XVI, LFO XVIII,
 * ENV XIII, SCL XVII, SAVE XX, SETTINGS I ...). Knobs are gold dials with a needle.
 * Faces (build/gen/alter_fonts.h, tools/build.py): TeX Gyre Pagella Bold (a Palatino: titles TT 16 px, labels TC
 * 12 px, big names TL 26 px; GUST font licence) and IBM Plex Mono (values TV 12 px, big numbers TN 16 px; SIL OFL).
 * Included by alter_ui.c after its colours (ALT_*), alt_mixc, alt_disc and alt_at. */
#include "alter_fonts.h"
#define ALT_FS (&AF_S)                           /* labels, tabs: Inter (Romain 01/11: a modern sans, sharp when small) */
#define ALT_FM (&AF_TT)                          /* titles: the ribbon, a sound's name */
#define ALT_FL (&AF_L)                           /* a preset's name, big (Inter) */
#define ALT_FV (&AF_TV)                          /* values */
#define ALT_FN (&AF_TN)                          /* big numbers */

static void alt_tri(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t c);   /* (below) */

/* the point at turn (0 = up, clockwise, a full turn = 2^32) and radius r around cx, cy */
static void alt_pt(int32_t cx, int32_t cy, uint32_t turn, int32_t r, int32_t *x, int32_t *y)
{
    *x = cx + ((r * sine_i(turn)) >> 15);
    *y = cy - ((r * sine_i(turn + 0x40000000u)) >> 15);
}

/* a pixel blended in: a 0..256 of c over what is there (anti-aliasing) */
static void alt_plot(int32_t x, int32_t y, uint16_t c, int32_t a)
{
    uint16_t *p;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || (uint32_t)y >= cv_h || a <= 0)
        return;
    p = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    *p = a >= 256 ? swap16(c) : swap16(alt_mixc(swap16(*p), c, a * 100 / 256));
}

/* an anti-aliased line between two points in 1/16 px (Wu's: each column / row lights its two nearest pixels) */
static void alt_aaline16(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int32_t steep = (y1 > y0 ? y1 - y0 : y0 - y1) > (x1 > x0 ? x1 - x0 : x0 - x1), t, dx, dy, g, xp, xe;
    if (steep) {
        t = x0, x0 = y0, y0 = t;
        t = x1, x1 = y1, y1 = t;
    }
    if (x0 > x1) {
        t = x0, x0 = x1, x1 = t;
        t = y0, y0 = y1, y1 = t;
    }
    dx = x1 - x0;
    dy = y1 - y0;
    g = dx ? (int32_t)(((int64_t)dy << 16) / dx) : 0;
    xe = (x1 + 8) >> 4;
    for (xp = (x0 + 8) >> 4; xp <= xe; xp++) {
        int32_t yf = y0 + (int32_t)(((int64_t)(xp * 16 - x0) * g) >> 16), yi = yf >> 4, fr = yf & 15;
        if (steep) {
            alt_plot(yi, xp, c, (16 - fr) * 16);
            alt_plot(yi + 1, xp, c, fr * 16);
        } else {
            alt_plot(xp, yi, c, (16 - fr) * 16);
            alt_plot(xp, yi + 1, c, fr * 16);
        }
    }
}
static void alt_aaline(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    alt_aaline16(x0 * 16, y0 * 16, x1 * 16, y1 * 16, c);
}

/* an anti-aliased ring: centre and radius in 1/16 px, as short chords */
static void alt_aaring16(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    uint32_t n = (uint32_t)(r >> 4) * 2u + 24u, k;
    int32_t px = cx, py = cy - r, x, y;
    if (n > 200u)
        n = 200u;
    for (k = 1; k <= n; k++) {
        uint32_t tn = (uint32_t)((uint64_t)0x100000000ull * k / n);
        x = cx + (int32_t)(((int64_t)r * sine_i(tn)) >> 15);
        y = cy - (int32_t)(((int64_t)r * sine_i(tn + 0x40000000u)) >> 15);
        alt_aaline16(px, py, x, y, c);
        px = x, py = y;
    }
}

/* an arc of radius r from turn t0 over span (clockwise), as short lines; thick 2: a second one inside */
static void alt_arc(int32_t cx, int32_t cy, int32_t r, uint32_t t0, uint32_t span, uint16_t c, int thick)
{
    uint32_t n = span / (0xFFFFFFFFu / 96u) + 2u, k;
    int32_t px, py, x, y, j;
    for (j = 0; j < thick; j++) {
        int32_t rr = (r - j) * 16;               /* (in 1/16 px: smooth) */
        px = cx * 16 + (int32_t)(((int64_t)rr * sine_i(t0)) >> 15);
        py = cy * 16 - (int32_t)(((int64_t)rr * sine_i(t0 + 0x40000000u)) >> 15);
        for (k = 1; k <= n; k++) {
            uint32_t tn = t0 + (uint32_t)((uint64_t)span * k / n);
            x = cx * 16 + (int32_t)(((int64_t)rr * sine_i(tn)) >> 15);
            y = cy * 16 - (int32_t)(((int64_t)rr * sine_i(tn + 0x40000000u)) >> 15);
            alt_aaline16(px, py, x, y, c);
            px = x, py = y;
        }
    }
}

/* a diamond: half widths rx, ry */
static void alt_diamond(int32_t x, int32_t y, int32_t rx, int32_t ry, uint16_t c)
{
    int32_t dy;
    for (dy = -ry + 1; dy < ry; dy++) {
        int32_t h = rx * (ry - (dy < 0 ? -dy : dy)) / ry;
        cv_rect(x - h, y + dy, 2 * h + 1, 1, c);
    }
    alt_aaline(x, y - ry, x + rx, y, c), alt_aaline(x + rx, y, x, y + ry, c);   /* (its edges smooth) */
    alt_aaline(x, y + ry, x - rx, y, c), alt_aaline(x - rx, y, x, y - ry, c);
}

/* a four-pointed star (a sparkle): arms of a, its core 1 px */
static void alt_star(int32_t x, int32_t y, int32_t a, uint16_t c)
{
    alt_diamond(x, y, 1, a, c);
    alt_diamond(x, y, a, 1, c);
}

static uint16_t alt_gold_line(int32_t pct) { return alt_mixc(ALT_BG, ALT_GOLD, pct); }

/* the card: its double frame, fan corners, the ribbon with the screen's title, the arcana's numeral */
static void alt_card(const char *title, const char *num)
{
    static const uint32_t FAN[4] = {0x40000000u, 0x80000000u, 0xC0000000u, 0u};   /* (each corner's inward quarter) */
    static const int32_t CX[4] = {3, 236, 236, 3}, CY[4] = {3, 3, 236, 236};
    uint16_t rim = alt_gold_line(55), edge = ALT_DAY ? ALT_RIB : ALT_GOLD;
    int32_t tw = text_w(ALT_FM, title), w = tw + 30, x0 = 120 - w / 2, x1 = 120 + w / 2, i;
    cv_frame(0, 0, 240, 240, ALT_FRAME);
    cv_frame(3, 3, 234, 234, rim);
    for (i = 0; i < 4; i++) {
        alt_arc(CX[i], CY[i], 5, FAN[i], 0x40000000u, ALT_GOLD, 1);
        alt_arc(CX[i], CY[i], 8, FAN[i], 0x40000000u, rim, 1);
    }
    /* the ribbon: notched at both ends */
    cv_rect(x0 + 5, 3, w - 10, 17, ALT_RIB);
    alt_tri(x0 * 16, 3 * 16, (x0 + 6) * 16, 3 * 16, (x0 + 6) * 16, 12 * 16, ALT_RIB);
    alt_tri((x0 + 6) * 16, 11 * 16, (x0 + 6) * 16, 20 * 16, x0 * 16, 20 * 16, ALT_RIB);
    alt_tri(x1 * 16, 3 * 16, (x1 - 6) * 16, 3 * 16, (x1 - 6) * 16, 12 * 16, ALT_RIB);
    alt_tri((x1 - 6) * 16, 11 * 16, (x1 - 6) * 16, 20 * 16, x1 * 16, 20 * 16, ALT_RIB);
    cv_rect(x0, 3, w, 1, edge);
    cv_rect(x0, 19, w, 1, edge);
    alt_aaline(x0, 3, x0 + 5, 11, edge), alt_aaline(x0 + 5, 11, x0, 19, edge);
    alt_aaline(x1, 3, x1 - 5, 11, edge), alt_aaline(x1 - 5, 11, x1, 19, edge);
    cv_text_on(120 - tw / 2, 3, ALT_FM, title, ALT_RIBTX, ALT_RIB);
    if (num)
        cv_text(9, 5, ALT_FS, num, ALT_GOLD);
}

/* the chosen circle at the top right: its sigil in its colour, then a word (its number, or what the page says) */
static void alt_chip_txt(const char *t, uint16_t tc)
{
    uint32_t c = au.sel % AS_NCIRC;
    alt_disc(226, 11, 5, ALT_COL[c]);
    alt_disc(226, 11, 3, ALT_BG);
    alt_disc(226, 11, 1, ALT_COL[c]);
    cv_text_r(218, 5, ALT_FS, t, tc, ALT_BG);
}
static void alt_chip(void)
{
    char b[3] = {'C', (char)('1' + au.sel % AS_NCIRC), 0};
    alt_chip_txt(b, ALT_INK);
}

/* tabs spread from x 8 to 232 at y: the one shown in ink, underlined in col; the others dim */
static void alt_tabs(int32_t y, const char *const *names, uint32_t n, uint32_t sel, uint16_t col)
{
    int32_t tot = 0, x = 8, gap;
    uint32_t i;
    for (i = 0; i < n; i++)
        tot += text_w(ALT_FS, names[i]);
    gap = n > 1u ? (224 - tot) / (int32_t)(n - 1u) : 0;
    if (gap > 22)
        gap = 22;
    for (i = 0; i < n; i++) {
        int32_t tw = text_w(ALT_FS, names[i]);
        cv_text(x, y, ALT_FS, names[i], i == sel ? ALT_INK : ALT_DIM);
        if (i == sel)
            cv_rect(x, y + 14, tw, 2, col);
        x += tw + gap;
    }
    cv_rect(8, y + 18, 224, 1, alt_gold_line(30));
}

/* a knob as a gold dial: its name at y; under it the dial (fill of 50, -1: none, the value as words) and its value;
 * lo .. hi (>= 0): the range an LFO gives it, an arc in col outside the dial; wave: an LFO moves it unseen */
#define ALT_DIAL_T0 0xA0000000u                  /* -135 degrees */
#define ALT_DIAL_SPAN 0xC0000000u                /* 270 degrees */
static void alt_dialk(int32_t x0, int32_t y, const char *name, const char *txt, int32_t fill, int32_t lo, int32_t hi,
                      int lit, uint16_t col, int wave)
{
    int32_t cx = x0 + 7, cy = y + 24, nx, ny;
    cv_text(x0, y, ALT_FS, name, lit ? ALT_DIM : ALT_GREY);
    if (wave) {
        int32_t k, wx = x0 + text_w(ALT_FS, name) + 3;
        for (k = 0; k < 8; k++)
            cv_rect(wx + k, y + 6 - ((sine_i((uint32_t)k * 0x20000000u) * 2) >> 15), 1, 2, col);
    }
    if (fill < 0) {
        cv_text(x0, y + 15, ALT_FV, txt, lit ? ALT_INK : ALT_GREY);
        return;
    }
    fill = clamp(fill, 0, 50);
    alt_arc(cx, cy, 7, ALT_DIAL_T0, ALT_DIAL_SPAN, alt_gold_line(30), 2);
    if (lo >= 0 && hi >= lo)
        alt_arc(cx, cy, 9, ALT_DIAL_T0 + (uint32_t)((uint64_t)ALT_DIAL_SPAN * (uint32_t)lo / 50u),
                (uint32_t)((uint64_t)ALT_DIAL_SPAN * (uint32_t)(hi - lo) / 50u) + 0x01000000u, col, 2);
    if (fill)
        alt_arc(cx, cy, 7, ALT_DIAL_T0, (uint32_t)((uint64_t)ALT_DIAL_SPAN * (uint32_t)fill / 50u),
                lit ? ALT_GOLD : ALT_GREY, 2);
    alt_pt(cx, cy, ALT_DIAL_T0 + (uint32_t)((uint64_t)ALT_DIAL_SPAN * (uint32_t)fill / 50u), 5, &nx, &ny);
    alt_aaline(cx, cy, nx, ny, lit ? ALT_INK : ALT_GREY);
    cv_rect(cx - 1, cy - 1, 2, 2, lit ? ALT_INK : ALT_GREY);
    if (txt && txt[0])
        cv_text(x0 + 18, y + 15, ALT_FV, txt, lit ? ALT_INK : ALT_GREY);
}

static void alt_foot(uint32_t q);                         /* (alter_ui.c: a page's four knobs as dials) */
static void alt_menu_head(const char *name, const char *num);   /* (alter_ui.c: the card and the circle's chip) */
