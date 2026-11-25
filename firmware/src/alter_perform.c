/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's PERFORM page (v1.2.10, Romain 25/11, maquettes https://claude.ai/artifact/86M3edMxXWDVTBxjXh1MP4 planches 1b, 1d):
 * one screen to play live. No circle chosen: the four knobs are always I, II, III and MASTER. A bank button tapped
 * changes what they move (tapped again: back to MACRO): FX the REVERB sends + its size, SEL (SCL here) the DELAY sends
 * + its feedback, ENV the DECAYs + the DUCK, SEQ the DENSITY (HITS) + the SWING; EDIT LFO GLO: banks of one's own
 * (for now set ones; to choose: a next step), MACRO: each circle's MACRO (a next step) + the scene's MACRO.
 * PLAY held 3 s opens it (alter_ui.c), HOME leaves it. Every knob goes through MIDI LEARN's setters (alter_learn.c
 * alm_get / alm_set: 0..127 over each setting's range).
 * The screen (planche 1d, « NÉON »): one wheel of three bands (I inside, III outside), a cell per step, the hits
 * outlined in their circle's colour, the step playing filled, step by step; a crown of ticks; the knobs below as
 * segmented bars. Redrawn when a step moves (alt_sig). */

enum { APB_MACRO, APB_FX, APB_SEL, APB_ENV, APB_SEQ, APB_EDIT, APB_LFO, APB_GLO, APB_N };
#define APB_NONE 255u
typedef struct {
    const char *name;
    uint8_t t;                                   /* the circles' setting (ALM_CT row), APB_NONE: none yet */
    uint8_t g;                                   /* the MASTER knob: a GLOBAL (ALG_*), APB_NONE: none */
    const char *kn, *gn;                         /* the knobs' short names */
} aper_bank_t;
static const aper_bank_t APER_B[APB_N] = {
    {"MACRO", APB_NONE, ALG_MACRO, "MACRO", "MACRO"},
    {"REVERB", 18, ALG_RSIZE, "REV", "SIZE"},
    {"DELAY", 17, ALG_DFB, "DLY", "FEEDBK"},
    {"DECAY", 9, ALG_DUCK, "DEC", "DUCK"},
    {"DENSITY", 28, ALG_SWING, "HITS", "SWING"},
    {"EDIT", 0, ALG_FFREQ, "TONE", "FILTER"},
    {"LFO", 12, ALG_FRES, "DRIVE", "RES"},
    {"GLO", 21, ALG_MASTER, "INSERT", "MASTER"},
};
static const uint8_t APER_BTN[APB_N] = {0, B_FX, B_SCL, B_ENV, B_SEQ, B_EDIT, B_LFO, B_GLO};

static struct {
    uint8_t bank;
    int16_t v[4];                                /* the knobs' values shown (0..127, -1: none) */
    uint32_t turn_ms;                            /* the last turn (the values follow the machine after it) */
    uint32_t play_t0;                            /* PLAY's press (held 3 s: this page) */
    uint8_t play_stop, play_held;                /* PLAY pressed while playing: stop at its release, unless held */
} aper;

static uint32_t aper_code(uint32_t i)            /* knob i's MIDI LEARN code in the bank (APB_NONE: none) */
{
    const aper_bank_t *b = &APER_B[aper.bank % APB_N];
    if (i < 3u)
        return b->t == APB_NONE ? APB_NONE : i * 32u + b->t;
    return b->g == APB_NONE ? APB_NONE : ALM_G0 + b->g;
}
/* knob i's setting now / to v (0..127): the ENV bank's DECAY follows each circle's envelope mode (FREE the sound's DECAY,
 * PERC its DECAY, STAB its LENGTH: what the ENV page's knob of the same job moves); the others through MIDI LEARN */
static int aper_dec(uint32_t i)
{
    return aper.bank == APB_ENV && i < 3u && alt_emode[i] % AM_N != AM_FREE;
}
static uint32_t aper_get(uint32_t i)
{
    if (aper_dec(i))
        return alt_envp[i][alt_emode[i] % AM_N == AM_PERC ? AE_PDEC : AE_SLEN];
    return alm_get(aper_code(i));
}
static void aper_set(uint32_t i, uint32_t v)
{
    if (aper_dec(i))
        alt_envp[i][alt_emode[i] % AM_N == AM_PERC ? AE_PDEC : AE_SLEN] = (uint8_t)v;
    else
        alm_set(aper_code(i), v);
}
static void aper_read(void)                      /* the knobs' values from the machine */
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        aper.v[i] = (int16_t)(aper_code(i) == APB_NONE ? -1 : (int32_t)aper_get(i));
}
static void aper_open(void)
{
    au.qp = QP_NONE;
    au.lockc = 0;
    au.scene_open = 0;
    alt_menu_set(QP_PERF);
    aper.bank = APB_MACRO;
    aper_read();
    alt_say("PERFORM");
}
/* PLAY (alter_ui_input): stopped, its press starts at once; playing, it stops at its release (a tap), so that held 3 s
 * it opens this page and the music goes on. 1 = the press is taken here */
#define APER_HOLD_MS 3000u
static void aper_play(uint32_t pressed)
{
    uint32_t dn = (ALT_BTNS >> panel.btn[B_PLAY]) & 1u;
    if ((pressed >> panel.btn[B_PLAY]) & 1u) {
        aper.play_t0 = fm1_ms | 1u;
        aper.play_held = 0;
        aper.play_stop = (uint8_t)(song.playing || seq_counting());
        if (!aper.play_stop)
            transport_req = 1u;
    } else if (dn && aper.play_t0 && !aper.play_held && (int32_t)(fm1_ms - aper.play_t0) >= (int32_t)APER_HOLD_MS) {
        aper.play_held = 1;
        aper.play_stop = 0;
        if (au.mn != QP_PERF)
            aper_open();
    } else if (!dn && aper.play_t0) {
        if (aper.play_stop && !aper.play_held)
            transport_req = 2u;
        aper.play_t0 = 0;
        aper.play_stop = 0;
    }
}
/* the page's buttons: a bank tapped (again: MACRO); taken out of pressed so no other page opens */
static uint32_t aper_buttons(uint32_t pressed)
{
    uint32_t k;
    for (k = 1; k < APB_N; k++)
        if ((pressed >> panel.btn[APER_BTN[k]]) & 1u) {
            aper.bank = (uint8_t)(aper.bank == k ? APB_MACRO : k);
            aper_read();
            alt_say(APER_B[aper.bank].name);
            pressed &= ~(1u << panel.btn[APER_BTN[k]]);
        }
    return pressed;
}
static void aper_turn(uint32_t i, int32_t s)
{
    uint32_t c = aper_code(i);
    int32_t v;
    if (c == APB_NONE)
        return;
    if (aper.v[i] < 0 || (uint32_t)(fm1_ms - aper.turn_ms) > 1000u)
        aper.v[i] = (int16_t)aper_get(i);        /* (moved by something else meanwhile: from where it is) */
    v = clamp((int32_t)aper.v[i] + s * 2, 0, 127);
    aper.v[i] = (int16_t)v;
    aper.turn_ms = fm1_ms;
    aper_set(i, (uint32_t)v);
}
static uint32_t aper_sig(void)                   /* (the screen: redrawn when this moves) */
{
    uint32_t c, s = aper.bank * 7u + 1u;
    if ((uint32_t)(fm1_ms - aper.turn_ms) > 1000u)
        aper_read();
    for (c = 0; c < 4u; c++)
        s = s * 31u + (uint32_t)(aper.v[c] + 1);
    for (c = 0; c < AS_NCIRC; c++)
        s = s * 31u + alt_seq.c[c].pat * 7u + alt_seq.c[c].cfg.len * 131u + alt_seq.c[c].pos * 17u + au.cfg[c].mute * 3u;
    return s * 31u + (uint32_t)song.playing;
}

/* the wheel: band c from r0 to r1, its cells */
#define APER_CX 120
#define APER_CY 112
static void aper_cell(int32_t r0, int32_t r1, uint32_t t0, uint32_t span, uint16_t c, int fill)
{
    int32_t x0, y0, x1, y1, r;
    if (fill) {
        for (r = r0; r <= r1; r++)
            alt_arc(APER_CX, APER_CY, r, t0, span, c, 1);
        return;
    }
    alt_arc(APER_CX, APER_CY, r0, t0, span, c, 1);
    alt_arc(APER_CX, APER_CY, r1, t0, span, c, 1);
    alt_pt(APER_CX, APER_CY, t0, r0, &x0, &y0), alt_pt(APER_CX, APER_CY, t0, r1, &x1, &y1);
    alt_aaline(x0, y0, x1, y1, c);
    alt_pt(APER_CX, APER_CY, t0 + span, r0, &x0, &y0), alt_pt(APER_CX, APER_CY, t0 + span, r1, &x1, &y1);
    alt_aaline(x0, y0, x1, y1, c);
}
static void aper_scene(void)
{
    static const int32_t R0[AS_NCIRC] = {27, 45, 63}, R1[AS_NCIRC] = {40, 58, 76};
    uint32_t c, k, i;
    char b[8];
    alt_card("PERFORM", "XXII");
    for (k = 0; k < 64u; k++) {                  /* the crown of ticks */
        int32_t x0, y0, x1, y1, r = k % 4u ? 80 : 79;
        alt_pt(APER_CX, APER_CY, k << 26, r, &x0, &y0), alt_pt(APER_CX, APER_CY, k << 26, 83, &x1, &y1);
        alt_aaline(x0, y0, x1, y1, k % 4u ? alt_gold_line(25) : alt_gold_line(55));
    }
    for (c = 0; c < AS_NCIRC; c++) {             /* the three bands */
        const as_circ_t *x = &alt_seq.c[c];
        uint32_t len = x->cfg.len ? x->cfg.len : 16u, step = (uint32_t)(0x100000000ull / len), gap = 0x02D82D83u;   /* (4 degrees) */
        uint32_t cur = (x->pos + len - 1u) % len, muted = au.cfg[c].mute;
        uint16_t col = ALT_COL[c];
        for (k = 0; k < len; k++) {
            uint32_t t0 = k * step + gap / 2u, span = step > gap ? step - gap : step, hit = (x->pat >> k) & 1u;
            int now = song.playing && k == cur && !muted;
            if (muted)
                aper_cell(R0[c], R1[c], t0, span, alt_fade(ALT_GREY, 40), 0);
            else if (now)
                aper_cell(R0[c], R1[c], t0, span, hit ? ALT_INK : alt_fade(col, 55), 1);
            else if (hit) {
                aper_cell(R0[c] - 1, R1[c] + 1, t0, span, alt_fade(col, 30), 0);   /* (its glow) */
                aper_cell(R0[c], R1[c], t0, span, col, 0);
            } else
                aper_cell(R0[c], R1[c], t0, span, alt_fade(ALT_GREY, 45), 0);
        }
    }
    cv_text(APER_CX - text_w(ALT_FS, APER_B[aper.bank].name) / 2, APER_CY - 6, ALT_FS, APER_B[aper.bank].name, ALT_GOLD);
    for (i = 0; i < 4u; i++) {                   /* the four knobs: name, value, a bar of ten cells */
        int32_t x = 8 + (int32_t)i * 57, v = aper.v[i], n = v < 0 ? 0 : (v * 10 + 63) / 127, s;
        uint16_t col = i < 3u ? ALT_COL[i] : ALT_GOLD;
        static const char *const CN[4] = {"I ", "II ", "III ", ""};
        char nm[12];
        uint32_t j = 0;
        const char *p = CN[i];
        while (*p)
            nm[j++] = *p++;
        for (p = i < 3u ? APER_B[aper.bank].kn : APER_B[aper.bank].gn; *p && j < sizeof nm - 1u; )
            nm[j++] = *p++;
        nm[j] = 0;
        cv_text(x, 198, ALT_FS, nm, (i < 3u && au.cfg[i].mute) ? ALT_GREY : col);
        if (v < 0)
            alt_scopy(b, "SOON", sizeof b);
        else
            b[0] = (char)('0' + v / 100), b[1] = (char)('0' + v / 10 % 10), b[2] = (char)('0' + v % 10), b[3] = 0;
        cv_text(x, 211, ALT_FV, b, v < 0 ? ALT_GREY : ALT_INK);
        for (s = 0; s < 10; s++)
            cv_rect(x + s * 5, 229, 4, 3, s < n ? col : alt_fade(ALT_GREY, 35));
    }
}
