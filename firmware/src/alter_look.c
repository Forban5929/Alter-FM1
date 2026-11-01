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
static void alt_chip(void)                       /* the chosen circle's sound by name (Romain: "which instrument am I on?") */
{
    alt_chip_txt(ALT_PRE[au.pre[au.sel % AS_NCIRC] % ALT_NPRE].name, ALT_INK);
}
/* the chosen circle's engine, small, after the arcana's numeral (the sub-menus) */
static void alt_eng_tag(const char *num, const char *t, uint16_t c)
{
    cv_text(9 + (num ? text_w(ALT_FS, num) + 6 : 0), 5, ALT_FS, t, c);
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

/* a knob, no number at rest (Romain, 01/11: "the knob says it"): its name, then
 *  - a MOON in its phase: new at 0, full at the top (fill of 50); lo .. hi (>= 0): the range an LFO gives it, an arc
 *    in col on its orbit; wave: an LFO moves it unseen (a small wave by the name)
 *  - TEMPO: an HOURGLASS, its sand down as the tempo climbs
 *  - a value as a word (FOLD, PHRY, C2) stays written beside it; a number shows only while the knob turns (1 s, the
 *    name in gold), then goes
 *  fill -1: no picture, the value as words (as before) */
#define ALT_DIAL_SHOW_MS 1000u
static char alt_dial_nm[4][10], alt_dial_tx[4][12];
static uint32_t alt_dial_t[4];
static uint32_t alt_dial_live(void)              /* the knobs whose number shows now (alter_draw's signature) */
{
    uint32_t i, m = 0;
    for (i = 0; i < 4u; i++)
        if (alt_dial_t[i] && (int32_t)(fm1_ms - alt_dial_t[i]) < (int32_t)ALT_DIAL_SHOW_MS)
            m |= 1u << i;
    return m;
}
static int alt_streq(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}
static void alt_scopy(char *d, const char *s, uint32_t n)
{
    uint32_t i = 0;
    while (s && s[i] && i + 1u < n)
        d[i] = s[i], i++;
    d[i] = 0;
}
static int alt_isnum(const char *t)              /* 64, -12, +7, 100%, 1/16: a number (shown while turning only) */
{
    if (*t == '-' || *t == '+')
        t++;
    return *t >= '0' && *t <= '9';
}

static void alt_moon(int32_t cx, int32_t cy, int32_t r, int32_t fill, int lit)
{
    int32_t dy, dx, r16 = r * 16 + 8;
    uint16_t dark = ALT_DAY ? alt_mixc(ALT_BG, ALT_INK, 20) : alt_gold_line(16), light = lit ? ALT_GOLD : ALT_GREY;
    for (dy = -r - 1; dy <= r + 1; dy++) {
        int32_t s2 = r16 * r16 - dy * dy * 256, s16 = s2 > 0 ? (int32_t)alt_isqrt((uint32_t)s2) : 0;
        int32_t term = s16 * (50 - 2 * fill) / 50;   /* the terminator: lit on its right */
        for (dx = -r - 1; dx <= r + 1; dx++) {
            int32_t d = (int32_t)alt_isqrt((uint32_t)((dx * dx + dy * dy) * 256)), ae = (r16 - d) * 16 + 128;
            int32_t at = (dx * 16 - term) * 16 + 128;
            uint16_t cc;
            if (ae <= 0)
                continue;
            at = at < 0 ? 0 : at > 256 ? 256 : at;
            if (fill <= 0)
                at = 0;
            else if (fill >= 50)
                at = 256;
            cc = alt_mixc(dark, light, at * 100 / 256);
            if (ae >= 256)
                cv_rect(cx + dx, cy + dy, 1, 1, cc);
            else
                alt_plot(cx + dx, cy + dy, cc, ae);
        }
    }
    alt_aaring16(cx * 16, cy * 16, r16, alt_gold_line(lit ? 50 : 30));
}

static void alt_hourglass(int32_t cx, int32_t cy, int32_t fill)
{
    uint16_t ln = alt_gold_line(70), sand = ALT_GOLD;
    int32_t top = (50 - clamp(fill, 0, 50)) * 7 / 50, bot = 7 - top, y;
    for (y = 1; y <= top; y++) {                 /* the top bulb's sand, its last rows by the neck */
        int32_t w = (y + 1) / 2;
        cv_rect(cx - w, cy - y, 2 * w + 1, 1, sand);
    }
    for (y = 0; y < bot; y++) {                  /* the bottom one's, a heap */
        int32_t w = 4 - y / 2;
        cv_rect(cx - w, cy + 8 - y, 2 * w + 1, 1, sand);
    }
    if (top && bot)
        cv_rect(cx, cy, 1, 8 - bot, alt_gold_line(60));   /* the thread of sand */
    cv_rect(cx - 6, cy - 10, 13, 1, ln);
    cv_rect(cx - 6, cy + 10, 13, 1, ln);
    alt_aaline(cx - 5, cy - 9, cx - 1, cy - 1, ln), alt_aaline(cx - 1, cy + 1, cx - 5, cy + 9, ln);
    alt_aaline(cx + 5, cy - 9, cx + 1, cy - 1, ln), alt_aaline(cx + 1, cy + 1, cx + 5, cy + 9, ln);
    cv_rect(cx - 1, cy - 1, 1, 3, ln);
    cv_rect(cx + 1, cy - 1, 1, 3, ln);
}

static void alt_dialk(int32_t x0, int32_t y, const char *name, const char *txt, int32_t fill, int32_t lo, int32_t hi,
                      int lit, uint16_t col, int wave)
{
    int32_t cx = x0 + 10, cy = y + 24, slot = clamp((x0 - 8 + 28) / 57, 0, 3), turning;
    if (!txt)
        txt = "";
    if (alt_streq(alt_dial_nm[slot], name)) {    /* the same knob: has its value moved? */
        if (!alt_streq(alt_dial_tx[slot], txt))
            alt_dial_t[slot] = fm1_ms | 1u;
    } else {
        alt_scopy(alt_dial_nm[slot], name, sizeof alt_dial_nm[0]);
        alt_dial_t[slot] = 0;                    /* (another page: nothing turned) */
    }
    alt_scopy(alt_dial_tx[slot], txt, sizeof alt_dial_tx[0]);
    turning = (alt_dial_live() >> slot) & 1u;
    cv_text(x0, y, ALT_FS, name, !lit ? ALT_GREY : turning ? ALT_GOLD : ALT_DIM);
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
    if (alt_streq(name, "TEMPO"))
        alt_hourglass(cx, cy, fill);
    else {
        if (lo >= 0 && hi >= lo)
            alt_arc(cx, cy, 10, (uint32_t)((uint64_t)0x100000000ull * (uint32_t)lo / 50u),
                    (uint32_t)((uint64_t)0x100000000ull * (uint32_t)(hi - lo) / 50u) + 0x02000000u, col, 2);
        alt_moon(cx, cy, 7, fill, lit);
    }
    if (txt[0] && (!alt_isnum(txt) || turning))
        cv_text(x0 + 22, y + (turning ? 14 : 16), turning ? ALT_FV : ALT_FS, txt,
                !lit ? ALT_GREY : turning ? ALT_GOLD : ALT_INK);
}

static void alt_foot(uint32_t q);                         /* (alter_ui.c: a page's four knobs as dials) */
static void alt_menu_head(const char *name, const char *num);   /* (alter_ui.c: the card and the circle's chip) */
