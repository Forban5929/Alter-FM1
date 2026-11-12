/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's MACRO (v0.77, Romain 12/11: « toute la scène, recettes + modifiables, OCT+ tenu + KNOB 1 »).
 * One knob for the whole scene: up to four targets (any setting MIDI LEARN reaches, alter_learn.c's codes: a circle's,
 * or the same on the three circles, or a GLOBAL), each with an AMOUNT (-64..63: 64 = the whole range at MACRO 127).
 * A RECIPE fills the four (TENSION SPACE DARK DESTROY); then each target and amount can be changed (the recipe's name
 * then reads with a '*'). Kept with the scene (USER slots, patch files, the crash copy).
 * The MACRO moves the targets from where they are: each target's BASE is its value at MACRO 0; a turn writes
 * BASE + AMOUNT x MACRO, so MACRO back to 0 gives the settings back. A target turned by hand meanwhile: its new value
 * is taken as its place (its BASE follows). CHAOS, CHAOS TOTAL, a factory scene, NEW: MACRO at 0 (nothing to give back).
 * OCT+ held on the circles' view: the MACRO page. KNOB 1 the MACRO, KNOB 2 the RECIPE, SELECT a row, KNOB 3 its
 * target, ALGORITHM its circle (I II III, I-III = the three), KNOB 4 its amount. Also a GLOBAL of MIDI LEARN (MACRO). */

#define AMC_NS 4u
#define AMC_NONE 255u
#define AMC_ALL 3u                               /* (cir: the three circles) */
enum { AMC_TENSION, AMC_SPACE, AMC_DARK, AMC_DESTROY, AMC_NR };
typedef struct {
    const char *name;
    uint8_t t[AMC_NS];                           /* a circle's setting (ALM_CT row) or ALM_G0 + a GLOBAL */
    int8_t amt[AMC_NS];
} amc_rcp_t;
/* (circle rows: 12 DRIVE, 13 LOW, 14 MID, 15 HIGH, 16 COMP, 17 DELAY, 18 REVERB, 21 INSERT MIX; on the three circles) */
static const amc_rcp_t AMC_R[AMC_NR] = {
    {"TENSION", {12, 15, 18, 17}, {40, 24, 30, 20}},
    {"SPACE", {18, 17, ALM_G0 + ALG_DFB, ALM_G0 + ALG_RSIZE}, {60, 44, 36, 40}},
    {"DARK", {15, 14, ALM_G0 + ALG_RDAMP, 13}, {-50, -20, 50, 20}},
    {"DESTROY", {12, 21, 16, ALM_G0 + ALG_FDRIVE}, {63, 50, 40, 60}},
};

static struct {
    uint8_t rcp, edited, val, last;              /* the recipe, changed since; the MACRO; the value the targets hold */
    uint8_t t[AMC_NS], cir[AMC_NS];              /* each row: its target (AMC_NONE: none), its circle (0..2, AMC_ALL) */
    int8_t amt[AMC_NS];
    int16_t base[AMC_NS][AS_NCIRC];              /* each target's value at MACRO 0 (-1: not known yet) */
    uint8_t w[AMC_NS][AS_NCIRC], w0[AMC_NS][AS_NCIRC];   /* what the MACRO wrote last, and before (a queued write) */
    uint8_t row, show, used;                     /* the page: its row; shown (OCT+ held); a knob turned in this hold */
    uint32_t oct_t0;
} amc;

static void amc_knob(uint32_t i, int32_t s);
static void amc_circle(int32_t s);
static uint32_t amc_value(void) { return amc.val; }
static int32_t amc_off(int32_t amt, uint32_t v) { return amt * 2 * (int32_t)v / 127; }
static uint32_t amc_code(uint32_t k, uint32_t c)   /* row k's code on circle c (AMC_NONE: none) */
{
    uint32_t t = amc.t[k];
    if (t == AMC_NONE)
        return AMC_NONE;
    if (t >= ALM_G0)
        return c == 0u ? t : AMC_NONE;           /* (a GLOBAL: once) */
    if (amc.cir[k] != AMC_ALL && amc.cir[k] != c)
        return AMC_NONE;
    return c * 32u + t;
}
static void amc_forget(void)                     /* the bases unknown: taken again at the next turn */
{
    uint32_t k, c;
    for (k = 0; k < AMC_NS; k++)
        for (c = 0; c < AS_NCIRC; c++)
            amc.base[k][c] = -1;
}
static void amc_recipe(uint32_t r)               /* the rows from recipe r */
{
    uint32_t k;
    amc.rcp = (uint8_t)(r % AMC_NR);
    amc.edited = 0;
    for (k = 0; k < AMC_NS; k++) {
        amc.t[k] = AMC_R[amc.rcp].t[k];
        amc.amt[k] = AMC_R[amc.rcp].amt[k];
        amc.cir[k] = AMC_ALL;
    }
    amc_forget();
}
static void amc_init(void)
{
    static uint8_t done;
    if (done)
        return;
    done = 1;
    amc_recipe(AMC_TENSION);
}
/* the targets to MACRO v (from amc.last) */
static void amc_move(uint32_t v)
{
    uint32_t k, c;
    amc_init();
    for (k = 0; k < AMC_NS; k++)
        for (c = 0; c < AS_NCIRC; c++) {
            uint32_t code = amc_code(k, c), cur;
            int32_t to;
            if (code == AMC_NONE)
                continue;
            cur = alm_get(code);
            if (amc.base[k][c] < 0 || (cur != amc.w[k][c] && cur != amc.w0[k][c]))   /* (unknown, or turned by hand) */
                amc.base[k][c] = (int16_t)((int32_t)cur - amc_off(amc.amt[k], amc.last));
            to = amc.base[k][c] + amc_off(amc.amt[k], v);
            to = to < 0 ? 0 : to > 127 ? 127 : to;
            amc.w0[k][c] = amc.w[k][c];
            amc.w[k][c] = (uint8_t)to;
            if ((uint32_t)to != cur)
                alm_set(code, (uint32_t)to);
        }
    amc.last = (uint8_t)v;
}
static void amc_turn_to(uint32_t v)
{
    v = v > 127u ? 127u : v;
    amc.val = (uint8_t)v;
    if (v != amc.last)
        amc_move(v);
}
static void amc_edit_begin(void) { if (amc.last) amc_move(0); }   /* (the targets given back before a row changes) */
static void amc_edit_end(void) { if (amc.val) amc_move(amc.val); }
/* CHAOS, CHAOS TOTAL, a factory scene, NEW: the settings are new, nothing to give back */
static void amc_zero(void)
{
    amc_init();
    amc.val = amc.last = 0;
    amc_forget();
}

/* ---- kept with the scene (alter_store.c, alter_link.c BLOB): [0] 1 = stored, recipe | edited << 7, MACRO, the four
 * targets, amounts + 64, circles */
static void amc_get(uint8_t *b)
{
    uint32_t k;
    amc_init();
    b[0] = 1;
    b[1] = (uint8_t)(amc.rcp | amc.edited << 7);
    b[2] = amc.val;
    for (k = 0; k < AMC_NS; k++) {
        b[3u + k] = amc.t[k];
        b[7u + k] = (uint8_t)(amc.amt[k] + 64);
        b[11u + k] = amc.cir[k];
    }
}
static void amc_put(const uint8_t *b)            /* (b 0 or b[0] 0: stored before the MACRO: TENSION at 0) */
{
    uint32_t k;
    amc_init();
    if (!b || b[0] != 1u) {
        amc_recipe(AMC_TENSION);
        amc_zero();
        return;
    }
    amc.rcp = (uint8_t)((b[1] & 127u) % AMC_NR);
    amc.edited = (uint8_t)(b[1] >> 7);
    for (k = 0; k < AMC_NS; k++) {
        uint32_t t = b[3u + k];
        amc.t[k] = (uint8_t)(t == AMC_NONE || (t < ALM_NCT) || (t >= ALM_G0 && t - ALM_G0 < ALG_MACRO) ? t : AMC_NONE);
        amc.amt[k] = (int8_t)((int32_t)(b[7u + k] & 127u) - 64);
        amc.cir[k] = (uint8_t)(b[11u + k] <= AMC_ALL ? b[11u + k] : AMC_ALL);
    }
    amc.val = amc.last = (uint8_t)(b[2] > 127u ? 127u : b[2]);   /* (the settings stored hold the MACRO's moves) */
    amc_forget();
}

/* OCT+ held alone on the circles' view (alter_ui_input, before the knobs): the MACRO page; shown once a knob turns, or
 * after 0.3 s with no key (OCT+ + a DICE stays the dice's) */
static void amc_input(uint32_t keys)
{
    uint32_t i;
    int32_t s;
    uint32_t up = (ALT_BTNS >> panel.btn[B_OCTUP]) & 1u, dn2 = (ALT_BTNS >> panel.btn[B_OCTDN]) & 1u;
    if (up && !dn2 && !au.mn && !au.lockc && !au.scene_open && !afp.open && !anm.open) {
        if (!amc.oct_t0 || keys) {
            amc.oct_t0 = fm1_ms | 1u;
            if (keys)
                amc.used = 0;
        }
        for (i = 0; i < 4u; i++)
            if ((s = panel_enc(EN_K1 + i)) != 0)
                amc_knob(i, s);
        if ((s = panel_enc(EN_SELECT)) != 0) {
            amc.row = (uint8_t)((amc.row + (s > 0 ? 1u : AMC_NS - 1u)) % AMC_NS);
            amc.used = 1;
        }
        if ((s = panel_enc(EN_ALGO)) != 0)
            amc_circle(s);
        (void)panel_enc(EN_PRESET);
        amc.show = (uint8_t)(amc.used || (!keys && fm1_ms - amc.oct_t0 >= 300u));
    } else {
        amc.oct_t0 = 0;
        amc.used = 0;
        amc.show = 0;
    }
}

/* ---- the page (OCT+ held on the circles' view) */
/* a row's target list: none, the circle's settings (ALM_CT), the GLOBALs but MACRO */
#define AMC_NT (1u + ALM_NCT + ALG_MACRO)
static uint32_t amc_tidx(uint32_t t) { return t == AMC_NONE ? 0u : t >= ALM_G0 ? 1u + ALM_NCT + (t - ALM_G0) : 1u + t; }
static uint32_t amc_tof(uint32_t i) { return i == 0u ? AMC_NONE : i <= ALM_NCT ? i - 1u : ALM_G0 + (i - 1u - ALM_NCT); }
static const char *amc_tname(uint32_t k)
{
    static const char *const SND[8] = {"SOUND 1", "SOUND 2", "SOUND 3", "SOUND 4", "SOUND 5", "SOUND 6", "SOUND 7", "SOUND 8"};
    uint32_t t = amc.t[k];
    if (t == AMC_NONE)
        return "-";
    if (t >= ALM_G0)
        return ALM_GT[(t - ALM_G0) % ALG_N];
    if (ALM_CT[t].name)
        return ALM_CT[t].name;
    if (amc.cir[k] != AMC_ALL)                   /* (the sound's own name on one circle) */
        return alm_cname(amc.cir[k] * 32u + t);
    return t < 8u ? SND[t] : "INSERT";
}
static void amc_knob(uint32_t i, int32_t s)
{
    uint32_t k = amc.row % AMC_NS;
    amc_init();
    amc.used = 1;
    switch (i) {
    case 0: {
        int32_t v = (int32_t)amc.val + s * 2;
        amc_turn_to((uint32_t)(v < 0 ? 0 : v > 127 ? 127 : v));
        break;
    }
    case 1:
        amc_edit_begin();
        amc_recipe((uint32_t)(((int32_t)amc.rcp + (s > 0 ? 1 : -1) + AMC_NR) % AMC_NR));
        amc_edit_end();
        break;
    case 2: {
        uint32_t n = (uint32_t)(((int32_t)amc_tidx(amc.t[k]) + (s > 0 ? 1 : -1) + (int32_t)AMC_NT) % (int32_t)AMC_NT);
        amc_edit_begin();
        amc.t[k] = (uint8_t)amc_tof(n);
        amc.edited = 1;
        amc_forget();
        amc_edit_end();
        break;
    }
    default: {
        int32_t a = (int32_t)amc.amt[k] + s;
        amc_edit_begin();
        amc.amt[k] = (int8_t)(a < -64 ? -64 : a > 63 ? 63 : a);
        amc.edited = 1;
        amc_edit_end();
        break;
    }
    }
}
static void amc_circle(int32_t s)                /* ALGORITHM: the row's circle (I II III I-III) */
{
    uint32_t k = amc.row % AMC_NS;
    amc.used = 1;
    if (amc.t[k] == AMC_NONE || amc.t[k] >= ALM_G0)
        return;
    amc_edit_begin();
    amc.cir[k] = (uint8_t)((amc.cir[k] + (s > 0 ? 1u : 3u)) % 4u);
    amc.edited = 1;
    amc_forget();
    amc_edit_end();
}
static uint32_t amc_sig(void)
{
    uint32_t k, h = amc.val * 3u + amc.rcp * 5u + amc.edited * 7u + amc.row * 11u + 1u;
    for (k = 0; k < AMC_NS; k++)
        h = h * 31u + amc.t[k] * 3u + (uint8_t)amc.amt[k] * 5u + amc.cir[k] * 7u;
    return h;
}
static void amc_scene(void)
{
    static const char *const CIR[4] = {"I", "II", "III", "I-III"};
    char b[16];
    uint32_t k, n = 0;
    int32_t w;
    const char *nm = AMC_R[amc.rcp % AMC_NR].name;
    alt_card("MACRO", "XIV");
    while (nm[n] && n < 12u) {
        b[n] = nm[n];
        n++;
    }
    if (amc.edited)
        b[n++] = '*';
    b[n] = 0;
    cv_text(8, 26, ALT_FM, b, ALT_INK);
    alt_num(b, (int32_t)amc.val, 0);
    cv_text_r(232, 26, ALT_FM, b, ALT_GOLD, cv_bg);
    w = (int32_t)amc.val * 220 / 127;            /* the gauge */
    cv_frame(10, 50, 220, 12, alt_gold_line(45));
    if (w > 0)
        cv_rect(11, 51, w - 1 > 218 ? 218 : w - 1, 10, ALT_GOLD);
    for (k = 0; k < AMC_NS; k++) {
        int32_t y = 76 + (int32_t)k * 30, a = amc.amt[k];
        uint32_t on = k == amc.row % AMC_NS, col = amc.t[k] < ALM_G0 && amc.cir[k] < AMC_ALL ? ALT_COL[amc.cir[k]] : ALT_GOLD;
        if (on)
            cv_rect(4, y - 4, 232, 26, alt_gold_line(14));
        alt_diamond(14, y + 7, 4, 4, amc.t[k] == AMC_NONE ? alt_gold_line(40) : (uint16_t)col);
        cv_text(26, y, ALT_FS, amc_tname(k), on ? ALT_INK : ALT_DIM);
        if (amc.t[k] != AMC_NONE && amc.t[k] < ALM_G0)
            cv_text(130, y, ALT_FS, CIR[amc.cir[k] % 4u], (uint16_t)col);
        b[0] = a > 0 ? '+' : a < 0 ? '-' : ' ';
        alt_num(b + 1, a < 0 ? -a : a, 0);
        cv_text_r(232, y, ALT_FS, amc.t[k] == AMC_NONE ? "" : b, on ? ALT_INK : ALT_DIM, cv_bg);
    }
    cv_text(8, 200, ALT_FS, "K1 MACRO  K2 RECIPE  SELECT ROW", ALT_DIM);
    cv_text(8, 216, ALT_FS, "K3 TARGET  K4 AMOUNT  ALGO CIRCLE", ALT_DIM);
}
