/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's PLAY view: the HOME page (Felucca's pages stay behind the other buttons). Three circles, three voices:
 * circle i plays part i (alter_play.c). Included in ui_input.c (after led_put); ui_input calls alter_ui_input first,
 * ui_draw_page alter_draw, ui_leds alter_leds. Every button does what its label says (Romain's rule):
 *   KNOB 1-4     the four main parameters of the chosen circle's engine (engine_t.knob)
 *   SEQ ENV FX LFO EDIT SCL GLO   a quick page: the view stays, KNOB 1-4 become the page's four (ALT_QP: SEQ = LEN
 *                HITS ROT PROB from the next cycle, the other circles greyed; ENV = ATK DEC SUS REL; FX = DIST INSERT
 *                DELAY REVERB; LFO = the chosen circle's LFO: WAVE RATE DEPTH DEST; EDIT = the engine's four others; SCL = SCALE ROOT
 *                CHORD DOSE; GLO = TEMPO LEVEL PAN SWING). A tap keeps it open (the same button again, or HOME, closes
 *                it; another page button switches); held 2 s or more it closes on release. LFO tapped again on its
 *                page: the MATRIX page (alt_matrix_input): tabs LFO 1 2 3 (each circle's LFO and its four matrix
 *                slots) and GLOBAL (alter_lfo.c); ALGORITHM the tab, SELECT the row, KNOB 1 SOURCE (row 0: WAVE),
 *                KNOB 2 DESTINATION (a list drops down; row 0: RATE), KNOB 3 AMOUNT (row 0: SYNC); LFO, HOME or
 *                another page button leaves
 *   FX held 0.5 s  the FX page (alter_fxpage.c): DIST (per circle), DELAY, REVERB (shared, each circle's send) and
 *                MASTER (EQ, COMP, DUCK, CLIP: alter_fx.c), PRESETS a preset; the keys play on. FX tapped or HOME leaves
 *   OCT- + OCT+ held, then a page button: Felucca's own page (HOME comes back); then DICE n: circle n's LOCKS page
 *                (the white keys F3..E4 = RHYTHM PITCH GLIDE ENV TIMBRE FORCE VELOCITY, a tap locks / frees; HOME)
 *   SELECT       the chosen circle (and Felucca's selected part: its pages edit its sound)
 *   PRESETS      the chosen circle's sound, through ALTER's own sounds of the three engines (alter_sounds.c ALT_PRE)
 *   SAVE         a tap: the SCENES page (PRESETS picks, SAVE loads, HOME leaves): the whole instrument from a starting
 *                point (alter_sounds.c ALT_SCENE). The first scene is loaded at power-on
 *   ALGORITHM    the chosen circle's envelope mode: FREE PERC STAB DRONE (alter_play.c AM_*)
 *   DICE 1 + 2 + 3 together: CHAOS, everything drawn to the extreme on the three circles: engine, preset, the
 *                engine's eight, the effects (sends 33 %, DIST and INSERT 10 % at most), the SEQ lanes (each 40 %, RATCH too), its LFO (WAVE RATE DEPTH DEST), the rhythm (at once), the die (DOSE 127), the scale and roots;
 *                not the tempo, not the modes, not the mutes. For good: no undo
 *   PLAY  start / stop      ARP REC: nothing yet (REC stays disarmed: no count-in, no click)
 * The keys (no notes here: alt_kb_own): the whites in lit blocks with a dark key between: MUTE 1 2 3 (a tap),
 * DICE 1 2 3 (a tap: a new draw at once; held 0.6 s: the clean pattern again), SHIFT -1 +1 (a tap: the chosen
 * circle turned a step, kept), LOCK (TURING); the blacks in their groups 3-2-3-2-1 (Romain, 01/11): CIRCLE 1 2 3 (a tap:
 * that circle chosen, as SELECT; the three together: TOTAL CHAOS) | tapped DENSITY - + (a hit less / more, kept) |
 * held, on the chosen circle: DUB DELAY, DUB REVERB, DESTROY | ROLL 1/16, ROLL 1/32 | TRIGGER. The whites C4..G5
 * after LOCK: free. All on the grid (alter_play.c). LEDs: the keys that act faint, bright when engaged (a circle muted, a
 * fill held, a tap just made); the others dark.
 * The screen: three concentric circles (circle 1 outside), each a voice's hits, lit as they play; the chosen one a
 * solid ring, the others dotted; a draw on: a small triangle at the circle's left, its colour; the chosen
 * circle's sound at the top, its knobs (or its rhythm, SEQ held) at the bottom. Drawn only when something changed. */

enum { AK_NONE, AK_MUTE, AK_DIE, AK_SHIFT, AK_DENS, AK_FILL, AK_TLOCK, AK_SEL, AK_TRIG };
typedef struct {
    uint8_t kind, arg;
    const char *name;
} alt_key_t;
static const alt_key_t ALT_KEY[27] = {    /* k: semitones above F3 (whites 0 2 4 6 7 9 11 12 14 16 18 19 ..) */
    [0] = {AK_MUTE, 0, "MUTE 1"}, [2] = {AK_MUTE, 1, "MUTE 2"}, [4] = {AK_MUTE, 2, "MUTE 3"},
    [7] = {AK_DIE, 0, "DICE 1"}, [9] = {AK_DIE, 1, "DICE 2"}, [11] = {AK_DIE, 2, "DICE 3"},
    [14] = {AK_SHIFT, 0, "SHIFT -1"}, [16] = {AK_SHIFT, 1, "SHIFT +1"},   /* (TURING: PREV, NEXT) */
    [18] = {AK_TLOCK, 0, "LOCK"},                                           /* (TURING only: frozen / free) */
    /* the blacks (Romain, 01/11): which circle | its rhythm | the held fills */
    [1] = {AK_SEL, 0, "CIRCLE 1"}, [3] = {AK_SEL, 1, "CIRCLE 2"}, [5] = {AK_SEL, 2, "CIRCLE 3"},
    [8] = {AK_DENS, 0, "DENSITY -"}, [10] = {AK_DENS, 1, "DENSITY +"},
    [13] = {AK_FILL, AF_DLY, "DUB DELAY"}, [15] = {AK_FILL, AF_REV, "DUB REVERB"}, [17] = {AK_FILL, AF_DESTROY, "DESTROY"},
    [20] = {AK_FILL, AF_ROLL16, "ROLL 1/16"}, [22] = {AK_FILL, AF_ROLL32, "ROLL 1/32"},
    [25] = {AK_TRIG, 0, "TRIGGER"},         /* a hit of the chosen circle at once; + REC: added to it (alter_play.c alt_trig_now) */
};
#define ALT_DIE_HOLD_MS 600u
#define ALT_REC_CLEAR_MS 1000u                   /* REC held: the chosen circle's moves cleared */
#define ALT_TAP_MS 150u                   /* a tapped key's bright LED */
#define ALT_SAVE_HOLD_MS 1000u            /* SAVE held this long on its page: store */

/* the quick pages: their buttons, names and four knobs (P_* of the chosen circle's part, or ALT_QK_* below;
 * QK_ENG: the engine's own) */
enum { QP_NONE, QP_SEQ, QP_ENV, QP_FX, QP_LFO, QP_EDIT, QP_SCL, QP_GLO, QP_N };
enum { QK_SCALE = 200, QK_ROOT, QK_DOSE, QK_BPM, QK_SWING, QK_RHY, QK_ENG, QK_ENGX, QK_CHORD, QK_LWAVE, QK_LRATE,
       QK_LDEPTH, QK_LDEST, QK_ENV };
static const uint8_t ALT_QP_BTN[QP_N] = {0, B_SEQ, B_ENV, B_FX, B_LFO, B_EDIT, B_SCL, B_GLO};
static const char *const ALT_QP_NAME[QP_N] = {"", "RHYTHM", "ENV", "FX", "LFO", "EDIT", "SCALE", "GLOBAL"};
static const uint8_t ALT_QP_K[QP_N][4] = {
    {QK_ENG, QK_ENG, QK_ENG, QK_ENG},
    {QK_RHY, QK_RHY, QK_RHY, QK_RHY},
    {QK_ENV, QK_ENV, QK_ENV, QK_ENV},          /* (by the circle's mode: alt_env_*) */
    {P_DIST, P_IA, P_DLY, P_REV},
    {QK_LWAVE, QK_LRATE, QK_LDEPTH, QK_LDEST},
    {QK_ENGX, QK_ENGX, QK_ENGX, QK_ENGX},
    {QK_SCALE, QK_ROOT, QK_CHORD, QK_DOSE},
    {QK_BPM, P_LEVEL, P_PAN, QK_SWING},
};
/* the LOCKS page: white key k (F3..E4) and the lock it toggles */
static const uint8_t ALT_LK_KEY[7] = {0, 2, 4, 6, 7, 9, 11};
static const uint8_t ALT_LK_BIT[7] = {AS_LK_RHYTHM, AS_LK_PITCH, AS_LK_GLIDE, AS_LK_ENV, AS_LK_TIMB, AS_LK_FORCE, AS_LK_VEL};
static const char *const ALT_LK_NAME[7] = {"RHYTHM", "PITCH", "GLIDE", "ENVELOPE", "TIMBRE", "FORCE", "VELOCITY"};

static struct {
    as_cfg_t cfg[AS_NCIRC];      /* the circles' settings as asked for (alt_post_cfg's source) */
    uint8_t ready, sel, drawn, die_clean_done, handed;   /* handed: a Felucca button is down (its release is Felucca's) */
    uint8_t qp, qp_was;          /* the quick page kept open (QP_*); the page buttons down at the last frame (bit QP_*) */
    uint8_t lockc;               /* the LOCKS page open: circle + 1 */
    uint8_t mat_open, mtab, mrow[4], dd;   /* the MATRIX page: its tab (0..2 the circles, 3 GLOBAL), each tab's row
                                 * (0 the LFO, 1..4 the slots); the knob whose list drops down (1..3), shown since dd_ms */
    uint32_t dd_ms;
    uint8_t seq_open, seq_hold, lane, step_turned;   /* the SEQ menu (SEQ held 0.5 s): open; opened by this hold; its
                                 * lane (VELO CHANCE DELAY REVERB PITCH); a knob turned while steps were held (no toggle) */
    uint16_t step_down;          /* the step keys (white keys) held: bit = step */
    uint8_t mn, mhold;           /* the ENV / EDIT / SCL menu open (QP_*, 0: none); the page buttons whose hold opened
                                 * their menu (bit QP_*: its release keeps it) */
    uint8_t set_row;             /* SETTINGS (EDIT held): the row */
    uint8_t scene_open, tab, row[2], save_done;   /* the SAVE page open; its tab (0 SCENES 1 PATTERNS), each tab's row;
                                                 * a long hold has stored (its release loads nothing) */
    uint32_t save_t0;            /* SAVE's press on the page (fm1_ms), 0 = up */
    uint8_t pre[AS_NCIRC];       /* each circle's sound (ALT_PRE) */
    uint32_t qp_t0[QP_N];        /* a page button's press (fm1_ms) */
    uint32_t keys;               /* the keys held at the last frame */
    uint32_t die_t0[AS_NCIRC];   /* a die key's press (fm1_ms | 1: never 0), 0 = up; compared signed (the | 1 may
                                  * put it 1 ms ahead) */
    uint32_t tap_ms[27];         /* a key's last tap (its bright LED) */
    const char *msg;             /* the middle's word .. */
    uint32_t msg_ms;             /* .. shown since */
    uint32_t sig, hits_seen[AS_NCIRC], flash_ms[AS_NCIRC], handed_ms;   /* handed_ms: when every Felucca button was up */
    uint8_t mute_tog, mute_combo;   /* the mutes these held keys toggled; MUTE 1+2+3 held (none of them toggles) */
    uint8_t tc_combo;            /* CIRCLE 1 + 2 + 3 held: TOTAL CHAOS done (they do nothing else) */
    uint8_t sel_was;             /* the chosen circle before the first CIRCLE key went down (put back by the combo) */
    uint32_t tc_ms;              /* the last TOTAL CHAOS (fm1_ms | 1): the same combo within 3 s undoes it */
    uint32_t rec_t0;             /* REC's press (fm1_ms | 1), 0 = up */
    uint8_t rec_done;            /* REC: its hold (or OCT- + OCT+) has cleared (its release does nothing) */
} au;

/* quick page q's name (LFO: the chosen circle's LFO 1) */
static const char *alt_qp_name(uint32_t q)
{
    static const char *const ENV[AM_N] = {"ENV FREE", "ENV PERC", "ENV STAB", "ENV DRONE"};
    static const char *const RHY[AS_NTYPE] = {"EUCLID", "TURING"};
    if (q == QP_ENV)                             /* (the chosen circle's mode: its own four) */
        return ENV[alt_emode[au.sel % AS_NCIRC] % AM_N];
    if (q == QP_SEQ)                             /* (the chosen circle's engine) */
        return RHY[au.cfg[au.sel % AS_NCIRC].type % AS_NTYPE];
    return q == QP_LFO ? "LFO 1" : ALT_QP_NAME[q % QP_N];   /* (LFO 1 of the chosen circle: drawn in its colour) */
}

/* the buttons the view keeps: the page buttons, HOME, PLAY, OCT- OCT+ */
static uint32_t alt_ours(void)
{
    static const uint8_t OURS[] = {B_SEQ, B_ENV, B_FX, B_LFO, B_EDIT, B_SCL, B_GLO, B_HOME, B_SAVE, B_PLAY, B_OCTDN,
                                   B_OCTUP, B_REC, B_ARP};   /* (REC: alter_rec.c; ARP: nothing yet, and not Felucca's) */
    uint32_t i, m = 0;
    for (i = 0; i < sizeof OURS; i++)
        m |= 1u << panel.btn[OURS[i]];
    return m;
}

static int alter_view(void)
{
    return ui.home && !ui.menu && !ui.layer && !ui.confirm && !ui.uboot && !name_on() &&
           !(fm1_in.buttons & ~alt_ours());   /* (SAVE, ARP, REC held: Felucca's) */
}

static void alt_post_circle(uint32_t i)
{
    alt_post &= (uint8_t)~(1u << i);
    alt_post_cfg[i] = au.cfg[i];
    alt_post |= (uint8_t)(1u << i);
}

static void alt_say(const char *s)
{
    au.msg = s;
    au.msg_ms = fm1_ms;
}

/* the presets of the three engines in a row: the n-th as (engine, preset); the place of (engine, preset) */
#define ALT_NENG 4u
static const uint8_t ALT_ENGS[ALT_NENG] = {ENGI_ALTER, ENGI_PHYS, 11, ENGI_RESO};   /* 11: NOISE (engines.c ENGINES) */
/* circle c's added steps set (the UI's; alter_seq.c as_circ_t.add, TRIGGER + REC) (a scene loaded, REC held 1 s: cleared) */
static void alt_add_set(uint32_t c, uint32_t m)
{
    fm1_irq_off();
    alt_seq.c[c % AS_NCIRC].add = m;
    as_repat(&alt_seq.c[c % AS_NCIRC]);
    fm1_irq_on();
}
/* circle c's REC'd fills (alter_play.c alt_fxr: ROLL 1/32, the dubs, per step) from b, or none (b = 0) */
static void alt_fxr_put(uint32_t c, const uint8_t *b)
{
    uint32_t k;
    fm1_irq_off();
    for (k = 0; k < AS_MAXLEN; k++)
        alt_fxr[c % AS_NCIRC][k] = b ? b[k] : 0u;
    fm1_irq_on();
}

#include "alter_sounds.c"

/* circle c's sound: ALT_PRE[i] on its part (the engine, its defaults, then the sound's values) */
/* the circles' level alignment (alter_fx.c): measure anew after a random draw, or a gain as it is (Q15, 0 dB = AC_UNITY) */
static void alt_cal(uint32_t c)
{
    alt_cal_cmd[c % AS_NCIRC] = 0;
    alt_cal_cnt[c % AS_NCIRC]++;
}
static void alt_cal_to(uint32_t c, int32_t g)
{
    alt_cal_cmd[c % AS_NCIRC] = g;
    alt_cal_cnt[c % AS_NCIRC]++;
}

static void alt_load_sound(uint32_t c, uint32_t i)
{
    track_t *t = &trk[c];
    const alt_pre_t *pr = &ALT_PRE[i % ALT_NPRE];
    uint32_t e = ALT_ENGS[pr->eng % ALT_NENG], j;
    au.pre[c] = (uint8_t)(i % ALT_NPRE);
    if (t->eng_req != e)
        set_engine_of(t, e);                     /* (the IRQ off inside: the engine and its values together) */
    fm1_irq_off();                               /* (the ISR's offsets and the sound's values together) */
    apply_preset_to(t, 0);                       /* (everything else at the engine's first preset's) */
    t->p[P_LD_PIT] = t->p[P_LD_FLT] = t->p[P_LD_SHP] = t->p[P_LD_AMP] = 0;   /* (the circle's LFOs are ours, they stay:
                                                                            * alter_lfo.c) */
    for (j = 0; j < 8u; j++)
        t->p[P_E0 + j] = pr->e[j];
    t->p[P_ATK] = pr->env[0];
    t->p[P_DEC] = pr->env[1];
    t->p[P_SUS] = pr->env[2];
    t->p[P_REL] = pr->env[3];
    for (j = 0; j < 4u; j++)
        t->p[P_DIST + j] = pr->fx[j];
    t->p[P_LEVEL] = pr->level;
    t->p[P_VOICE] = V_POLY;
    t->p[P_ITYPE] = 0;
    for (j = 0; j < ALT_NSLOT; j++)
        alt_applied[c][j] = 0;
    alt_note[c][0] = alt_note[c][1] = alt_note[c][2] = 0;   /* (apply_preset_to released what sounded) */
    alt_gate_left[c] = 0;
    fm1_irq_on();
    afx_sel[c] = 0;                              /* (its DIST: the sound's own again, alter_fx.c) */
    alt_cal_to(c, AC_UNITY);                     /* (a sound chosen: its own level, 0 dB; a random draw measures after) */
    afx_edit &= (uint8_t)~(1u << c);
    alt_preset_cnt[c]++;
}

/* PRESETS: circle c's sound s on */
static void alt_preset_step(uint32_t c, int32_t s)
{
    uint32_t i = (uint32_t)(((int32_t)au.pre[c] + s % (int32_t)ALT_NPRE + (int32_t)ALT_NPRE) % (int32_t)ALT_NPRE);
    alt_load_sound(c, i);
    alt_say(ALT_PRE[i].name);
}

#include "alter_store.c"

/* SEQ held: knob i of the chosen circle, s detents */
/* the rhythm's four knobs, by the circle's engine (alter_seq.c AS_T_*) */
static const char *const ALT_RHY_NAME[AS_NTYPE][4] = {
    {"LEN", "HITS", "ROT", "PROB"}, {"LEN", "LOCK", "DENS", "RANGE"},
};
/* SEQ: knob i of the chosen circle's rhythm, s detents (LEN HITS ROT PROB / LEN LOCK DENS RANGE) */
static void alt_edit_circle(uint32_t i, int32_t s)
{
    as_cfg_t *c = &au.cfg[au.sel];
    uint32_t t = c->type % AS_NTYPE, mx = t == AS_T_EUCLID ? AS_MAXLEN : 16u;
    if (i == 0) {
        c->len = (uint8_t)clamp((int32_t)c->len + s, 1, (int32_t)mx);
        if (c->hits > c->len)
            c->hits = c->len;
        c->rot %= c->len;
        c->frot %= c->len;
    } else if (i == 3u && t != AS_T_TURING) {
        c->prob = (uint8_t)clamp((int32_t)c->prob + s * 8, 0, 127);
    } else if (i == 2u && t != AS_T_TURING) {
        c->rot = as_wrap((int32_t)c->rot + s, c->len);
    } else if (t == AS_T_EUCLID) {
        c->hits = (uint8_t)clamp((int32_t)c->hits + s, 0, c->len);
    } else if (i == 1u) {
        c->tlock = (uint8_t)clamp((int32_t)c->tlock + s * 4, 0, 127);
    } else if (i == 2u) {
        c->tdens = (uint8_t)clamp((int32_t)c->tdens + s * 4, 0, 127);
    } else {
        c->trange = (uint8_t)clamp((int32_t)c->trange + (s > 0 ? 1 : -1), 0, 14);
    }
    alt_post_circle(au.sel);
}

/* the id knob i edits on quick page q (the chosen circle): P_*, or QK_* */
static uint32_t alt_qk(uint32_t q, uint32_t i)
{
    const engine_t *e = ENGINES[eng_idx(trk[au.sel].eng_req)];
    uint32_t id = ALT_QP_K[q][i];
    if (id == QK_ENG)
        return e->knob[i];
    if (id == QK_ENGX) {                         /* the engine's E1..E8 its four knobs leave, in order */
        uint32_t k, j, n = 0;
        for (k = 0; k < 8u; k++) {
            int used = 0;
            for (j = 0; j < 4u; j++)
                used |= e->knob[j] == P_E0 + k;
            if (!used && n++ == i)
                return P_E0 + k;
        }
        return P_E0 + 4u + i;
    }
    return id;
}

static void alt_lfo_turn(uint32_t tb, uint32_t i, int32_t s);   /* (the LFOs, below) */
static void alt_qpush(uint32_t c, uint32_t id, int32_t d);

/* ---- the ENV page, by the chosen circle's mode (alter_lfo.c AE_*; FREE: the sound's ADSR) ------------------------ */
static const char *const ALT_ENV_NAME[AM_N][4] = {
    {"ATK", "DEC", "SUS", "REL"}, {"ATTACK", "HOLD", "DECAY", "PUNCH"}, {"LENGTH", "SUSTAIN", "PLUCK", "CHORD"},
    {"SWELL", "BODY", "TAIL", "OPEN"},
};
static const int8_t ALT_ENV_K[AM_N][4] = {       /* each knob's setting (AE_*), -1: the sound's own / the chord */
    {-1, -1, -1, -1}, {AE_PATK, AE_PHOLD, AE_PDEC, AE_PPUNCH}, {AE_SLEN, AE_SSUS, AE_SPLUCK, -1},
    {AE_DSWELL, AE_DBODY, AE_DTAIL, AE_DOPEN},
};
/* the LFO destinations (bit GD_*) that move knob i of the ENV page in mode m */
static uint32_t alt_env_gd(uint32_t m, uint32_t i)
{
    static const uint16_t G[AM_N][4] = {
        {1u << GD_EATK, 1u << GD_EDEC | 1u << GD_ELEN, 1u << GD_ESUS, 1u << GD_ESUS | 1u << GD_ELEN},
        {1u << GD_EATK, 1u << GD_EDEC | 1u << GD_ELEN, 1u << GD_ESUS | 1u << GD_ELEN, 0},
        {1u << GD_EDEC | 1u << GD_ELEN, 1u << GD_ESUS, 0, 0},
        {1u << GD_EATK, 1u << GD_EDEC, 1u << GD_ESUS | 1u << GD_ELEN, 0},
    };
    return G[m % AM_N][i & 3u];
}

static void alt_env_turn(uint32_t i, int32_t s)
{
    uint32_t c = au.sel % AS_NCIRC, m = alt_emode[c] % AM_N;
    int32_t k = ALT_ENV_K[m][i & 3u];
    if (m == AM_FREE) {
        alt_qpush(c, P_ATK + i, s * 2);
    } else if (k >= 0) {
        alt_envp[c][k] = (uint8_t)clamp((int32_t)alt_envp[c][k] + s * 2, 0, 127);
    } else {                                     /* STAB's CHORD */
        as_cfg_t *cf = &au.cfg[c];
        cf->chord = as_ch_step(cf->chord, s);
        alt_post_circle(c);
    }
}
static void alt_select(uint32_t i);

/* a knob of quick page q turned s detents */
static void alt_turn(uint32_t q, uint32_t i, int32_t s)
{
    uint32_t id = alt_qk(q, i);
    as_cfg_t *c = &au.cfg[au.sel];
    switch (id) {
    case QK_RHY:
        alt_edit_circle(i, s);
        return;
    case QK_ENV:
        alt_env_turn(i, s);
        return;
    case QK_LWAVE: case QK_LRATE: case QK_LDEPTH: case QK_LDEST:
        alt_lfo_turn(0, id - QK_LWAVE, s);    /* (the chosen circle's LFO 1) */
        return;
    case QK_SCALE:
        alt_seq.scale = (uint8_t)(((int32_t)alt_seq.scale + s + 16 * AS_NSCALE) % AS_NSCALE);
        return;
    case QK_ROOT:
        c->root = (uint8_t)clamp((int32_t)c->root + s, 12, 96);
        alt_post_circle(au.sel);
        return;
    case QK_DOSE:
        c->dose = (uint8_t)clamp((int32_t)c->dose + s * 4, 0, 127);
        alt_post_circle(au.sel);
        return;
    case QK_CHORD:
        c->chord = as_ch_step(c->chord, s);
        alt_post_circle(au.sel);
        return;
    case QK_BPM:
        song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + s, GP[G_BPM].min, GP[G_BPM].max);
        return;
    case QK_SWING:
        song.g[G_SWING] = (int16_t)clamp(song.g[G_SWING] + s * 2, GP[G_SWING].min, GP[G_SWING].max);
        return;
    default: {
        const param_desc_t *d = param_desc_of(eng_idx(trk[au.sel].eng_req), id);
        uint8_t w = alt_qw, nx = (uint8_t)((w + 1u) % ALT_QN);
        if (nx == alt_qr)                        /* (full: the turn is lost, the next one goes) */
            return;
        alt_q[w].c = au.sel;
        alt_q[w].p = (uint8_t)id;
        alt_q[w].d = (int8_t)clamp(d->fmt == F_ENUM ? s : s * 2, -100, 100);
        alt_qw = nx;
        return;
    }
    }
}

/* parameter id of circle c's part moved by d (through the queue: the audio side applies it) */
static void alt_qpush(uint32_t c, uint32_t id, int32_t d)
{
    uint8_t w = alt_qw, nx = (uint8_t)((w + 1u) % ALT_QN);
    if (nx == alt_qr)                            /* (full: the turn is lost, the next one goes) */
        return;
    alt_q[w].c = (uint8_t)c;
    alt_q[w].p = (uint8_t)id;
    alt_q[w].d = (int8_t)clamp(d, -100, 100);
    alt_qw = nx;
}

/* ---- the LFOs: the chosen circle's three and the GLOBAL, four settings each (alter_lfo.c) ---------------------- */
#define ALT_TAB_GL CL_N                          /* the LFO page's tabs: LFO 1 2 3 of the chosen circle, the GLOBAL, MTX */
#define ALT_TAB_MTX (CL_N + 1u)
#define ALT_DD_MS 1200u                          /* a list stays down this long after its knob */
static const char *const ALT_LFO_KNOB[4] = {"WAVE", "RATE", "DEPTH", "DEST"};

/* setting i (0 WAVE 1 RATE 2 DEPTH 3 DEST) of LFO tb (0..2 the chosen circle's, ALT_TAB_GL) as a number */
static int32_t alt_lfo_val(uint32_t tb, uint32_t i)
{
    const volatile alt_cl_t *l = &alt_cl[au.sel % AS_NCIRC][tb % CL_N];
    if (tb == ALT_TAB_GL)
        return i == 0 ? alt_gl.wave : i == 1 ? alt_gl.rate : i == 2 ? alt_gl.amt[0] : alt_gl.dst[0];
    return i == 0 ? l->wave : i == 1 ? l->rate : i == 2 ? l->depth : l->dst;
}

/* .. as words (buf: room for 6) */
static const char *alt_lfo_text(uint32_t i, int32_t v, char *buf)
{
    if (i == 2) {
        int32_t n = 0, q = v < 0 ? -v : v;
        buf[n++] = v < 0 ? '-' : '+';
        if (q >= 10)
            buf[n++] = (char)('0' + q / 10);
        buf[n++] = (char)('0' + q % 10);
        buf[n] = 0;
        return buf;
    }
    if (i == 3)
        return GL_DST_NAME[clamp(v, 0, GD_N - 1)];
    return i == 0 ? GL_WAVE_NAME[clamp(v, 0, GW_N - 1)] : GL_RATE_NAME[clamp(v, 0, GL_NRATE - 1)];
}

/* setting i of LFO tb turned s detents (bytes the audio side reads: written here) */
static void alt_lfo_turn(uint32_t tb, uint32_t i, int32_t s)
{
    int32_t d = s > 0 ? 1 : -1;
    volatile uint8_t *w, *r, *g;
    volatile int8_t *a;
    if (tb == ALT_TAB_GL) {
        w = &alt_gl.wave, r = &alt_gl.rate, g = &alt_gl.dst[0], a = &alt_gl.amt[0];
    } else {
        volatile alt_cl_t *l = &alt_cl[au.sel % AS_NCIRC][tb % CL_N];
        w = &l->wave, r = &l->rate, g = &l->dst, a = &l->depth;
    }
    if (i == 0)
        *w = (uint8_t)clamp((int32_t)*w + d, 0, GW_N - 1);
    else if (i == 1)
        *r = (uint8_t)clamp((int32_t)*r + d, 0, GL_NRATE - 1);
    else if (i == 2)
        *a = (int8_t)clamp((int32_t)*a + s, -64, 63);
    else {
        *g = (uint8_t)clamp((int32_t)*g + d, 0, GD_N - 1);
        au.dd = 4;                               /* DEST: its list drops down */
        au.dd_ms = fm1_ms;
    }
}

/* ---- MTX: the chosen circle's four matrix slots (Felucca's: SRC DST AMT) ------------------------------------- */
static void alt_mtx_turn(uint32_t i, int32_t s)
{
    uint32_t c = au.sel % AS_NCIRC, r = au.mrow[0] % NMSLOT;
    if (i > 2u)
        return;
    alt_qpush(c, P_M1SRC + 3u * r + i, s);
    if (i < 2u) {                                /* a SOURCE or a DESTINATION: its list drops down */
        au.dd = (uint8_t)(i + 1u);
        au.dd_ms = fm1_ms;
    }
}

/* ---- the SEQ menu: the chosen circle's steps, its lanes (VELO CHANCE DELAY REVERB PITCH), its engine ------------- */
enum { LN_VELO, LN_CHANCE, LN_DLY, LN_REV, LN_PITCH, LN_RATCH, LN_N };
static const char *const ALT_LANE_NAME[LN_N] = {"VELO", "CHANCE", "DELAY", "REVERB", "PITCH", "RATCH"};
static const char *const ALT_LANE_TAB[LN_N] = {"VELO", "CHNC", "DLY", "REV", "PITCH", "RATCH"};   /* (the tabs: six fit) */
static const uint8_t ALT_WHITE[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};   /* step i's key */

/* lane ln's value at step k of circle c: 0..127 (PITCH: -7..7) */
static int32_t alt_lane_val(uint32_t c, uint32_t ln, uint32_t k)
{
    const as_circ_t *sc = &alt_seq.c[c % AS_NCIRC];
    k %= AS_NLANE;
    return ln == LN_VELO ? sc->svel[k] : ln == LN_CHANCE ? sc->chance[k] : ln == LN_DLY ? sc->sdly[k] :
           ln == LN_REV ? sc->srev[k] : ln == LN_RATCH ? sc->srat[k] : sc->spitch[k];
}

/* the held steps' lane value moved by s (written here: the audio side reads the bytes) */
static void alt_lane_turn(int32_t s)
{
    as_circ_t *sc = &alt_seq.c[au.sel % AS_NCIRC];
    uint32_t k;
    for (k = 0; k < AS_NLANE; k++) {
        if (!((au.step_down >> k) & 1u))
            continue;
        if (au.lane == LN_VELO)
            sc->svel[k] = (uint8_t)clamp((int32_t)sc->svel[k] + s * 4, 1, 127);
        else if (au.lane == LN_CHANCE) {
            int32_t v = clamp((int32_t)sc->chance[k] + s * 8, 0, 127);
            sc->chance[k] = (uint8_t)v;
            if (k + AS_NLANE < AS_MAXLEN)        /* (a 32-step circle repeats the lane) */
                sc->chance[k + AS_NLANE] = (uint8_t)v;
        } else if (au.lane == LN_DLY)
            sc->sdly[k] = (uint8_t)clamp((int32_t)sc->sdly[k] + s * 8, 0, 127);
        else if (au.lane == LN_REV)
            sc->srev[k] = (uint8_t)clamp((int32_t)sc->srev[k] + s * 8, 0, 127);
        else if (au.lane == LN_RATCH)            /* x1 .. x4 */
            sc->srat[k] = (uint8_t)clamp((int32_t)sc->srat[k] + (s > 0 ? 1 : -1), 0, 3);
        else
            sc->spitch[k] = (int8_t)clamp((int32_t)sc->spitch[k] + (s > 0 ? 1 : -1), -7, 7);
    }
    au.step_turned = 1;
}

/* lane ln's bounds and a knob detent's size */
static void alt_lane_lim(uint32_t ln, int32_t *lo, int32_t *hi, int32_t *st)
{
    *lo = ln == LN_VELO ? 1 : ln == LN_PITCH ? -7 : 0;
    *hi = ln == LN_PITCH ? 7 : ln == LN_RATCH ? 3 : 127;
    *st = ln == LN_VELO ? 4 : ln == LN_PITCH || ln == LN_RATCH ? 1 : 8;
}

/* KNOB 3 (b = 0) / KNOB 4 (b = 1) with steps held: the bottom / top of their RANGE (from their value at first); each
 * pass then plays a new value between the two (alter_seq.c as_rng). KNOB 1 gives them a single value again */
static void alt_lane_range(uint32_t b, int32_t s)
{
    as_circ_t *sc = &alt_seq.c[au.sel % AS_NCIRC];
    uint32_t k, ln = au.lane % LN_N;
    int32_t lo, hi, st;
    alt_lane_lim(ln, &lo, &hi, &st);
    for (k = 0; k < AS_NLANE; k++) {
        int8_t *p;
        if (!((au.step_down >> k) & 1u))
            continue;
        if (!((sc->rmask[ln] >> k) & 1u)) {
            int32_t v = alt_lane_val(au.sel % AS_NCIRC, ln, k);
            sc->rlo[ln][k] = sc->rhi[ln][k] = (int8_t)v;
        }
        p = b ? &sc->rhi[ln][k] : &sc->rlo[ln][k];
        *p = (int8_t)clamp((int32_t)*p + (st == 1 ? (s > 0 ? 1 : -1) : s * st), lo, hi);
        sc->rmask[ln] |= (uint16_t)(1u << k);
    }
    au.step_turned = 1;
}

/* lane ln of the chosen circle flat again: VELO normal, CHANCE sure, no DELAY REVERB PITCH RATCH, no RANGE */
static void alt_lane_flat(uint32_t ln)
{
    as_circ_t *sc = &alt_seq.c[au.sel % AS_NCIRC];
    uint32_t k;
    for (k = 0; k < AS_NLANE; k++) {
        if (ln == LN_VELO)
            sc->svel[k] = AS_VEL[AS_NORMAL];
        else if (ln == LN_CHANCE)
            sc->chance[k] = sc->chance[k + AS_NLANE] = 127;
        else if (ln == LN_DLY)
            sc->sdly[k] = 0;
        else if (ln == LN_REV)
            sc->srev[k] = 0;
        else if (ln == LN_PITCH)
            sc->spitch[k] = 0;
        else
            sc->srat[k] = 0;
    }
    sc->rmask[ln % AS_NLR] = 0;
}

/* two steps held + KNOB 2: the lane shown drawn anew on its 16 steps, hits or not, single values (no RANGE), over its
 * whole height but VELO and CHANCE kept audible (30 % and up) */
static uint32_t alt_cr(uint32_t n);              /* (below: CHAOS's draws) */
static uint32_t alt_crng;
static void alt_lane_shuffle(void)
{
    as_circ_t *sc = &alt_seq.c[au.sel % AS_NCIRC];
    uint32_t k, ln = au.lane % LN_N;
    int32_t lo, hi, st;
    alt_lane_lim(ln, &lo, &hi, &st);
    alt_crng ^= fm1_ms * 0x9E3779B9u;
    if (ln == LN_VELO || ln == LN_CHANCE)
        lo = 38;
    for (k = 0; k < AS_NLANE; k++) {
        int32_t v = lo + (int32_t)alt_cr((uint32_t)(hi - lo + 1));
        if (ln == LN_VELO)
            sc->svel[k] = (uint8_t)v;
        else if (ln == LN_CHANCE)
            sc->chance[k] = sc->chance[k + AS_NLANE] = (uint8_t)v;
        else if (ln == LN_DLY)
            sc->sdly[k] = (uint8_t)v;
        else if (ln == LN_REV)
            sc->srev[k] = (uint8_t)v;
        else if (ln == LN_PITCH)
            sc->spitch[k] = (int8_t)v;
        else
            sc->srat[k] = (uint8_t)v;
    }
    sc->rmask[ln] = 0;
    au.step_turned = 1;
    alt_say("SHUFFLE");
}

/* SEQ held: ALGORITHM picks the chosen circle's engine (from its next cycle) */
static void alt_seq_type(int32_t s)
{
    as_cfg_t *c = &au.cfg[au.sel % AS_NCIRC];
    c->type = (uint8_t)clamp((int32_t)c->type + (s > 0 ? 1 : -1), 0, AS_NTYPE - 1);
    if (c->type != AS_T_EUCLID && c->len > 16u)
        c->len = 16;
    alt_post_circle(au.sel % AS_NCIRC);
    alt_say(AS_TYPE_NAME[c->type]);
}

/* the SEQ menu's black keys (Romain, 01/11): a lane each, then EUCLID / TURING, the circle before, the circle after */
enum { ASK_LANE = 0, ASK_TYPE = 100, ASK_PREV, ASK_NEXT, ASK_NONE = 255 };
static const uint8_t ALT_SEQ_BK[27] = {
    [0 ... 26] = ASK_NONE,
    [1] = LN_VELO, [3] = LN_CHANCE, [5] = LN_DLY, [8] = LN_REV, [10] = LN_PITCH, [13] = LN_RATCH,
    [15] = ASK_TYPE, [20] = ASK_PREV, [22] = ASK_NEXT,
};
/* the SEQ menu has this frame: the step keys, the lanes, the knobs (no fills here) */
static void alt_seq_input(uint32_t keys)
{
    uint32_t down = keys & ~au.keys, up = au.keys & ~keys, k, i;
    int32_t s;
    if ((s = panel_enc(EN_ALGO)) != 0)
        au.lane = (uint8_t)clamp((int32_t)au.lane + (s > 0 ? 1 : -1), 0, LN_N - 1);
    if ((s = panel_enc(EN_SELECT)) != 0) {
        alt_select((uint32_t)((int32_t)au.sel + s + 3 * 16));
        au.step_down = 0;
    }
    (void)panel_enc(EN_PRESET);
    for (k = 0; k < 27u; k++)                    /* the black keys: shortcuts */
        if ((down >> k) & 1u && ALT_SEQ_BK[k] != ASK_NONE) {
            uint32_t b = ALT_SEQ_BK[k];
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if (b < LN_N && (fm1_in.buttons & both) == both) {   /* OCT- + OCT+ held: that lane back to flat */
                alt_lane_flat(b);
                au.lane = (uint8_t)b;
                alt_say("FLAT");
            } else if (b < LN_N) {
                au.lane = (uint8_t)b;
                alt_say(ALT_LANE_NAME[b]);
            } else if (b == ASK_TYPE)
                alt_seq_type(au.cfg[au.sel % AS_NCIRC].type == AS_T_EUCLID ? 1 : -1);
            else {
                alt_select((uint32_t)((int32_t)au.sel + (b == ASK_NEXT ? 1 : -1) + 3 * 16));
                au.step_down = 0;
            }
        }
    for (k = 0; k < 16u; k++) {
        uint32_t key = ALT_WHITE[k];
        if ((down >> key) & 1u) {
            if (!au.step_down)
                au.step_turned = 0;
            au.step_down |= (uint16_t)(1u << k);
        } else if ((up >> key) & 1u) {
            au.step_down &= (uint16_t)~(1u << k);
        }
    }
    for (i = 0; i < 4u; i++)
        if ((s = panel_enc(EN_K1 + i)) != 0) {
            if (!au.step_down)
                alt_edit_circle(i, s);           /* no step held: the rhythm's four */
            else if (i == 0u) {                  /* steps held: KNOB 1 their value (a single one: no RANGE) */
                alt_lane_turn(s);
                alt_seq.c[au.sel % AS_NCIRC].rmask[au.lane % LN_N] &= (uint16_t)~au.step_down;
            } else if (i == 1u) {                /* KNOB 2, two steps held at least (no slip in a live set): */
                if (au.step_down & (au.step_down - 1u))   /* a new draw of the whole lane, its 16 steps */
                    alt_lane_shuffle();
            } else                               /* KNOB 3 / 4 the bottom / top of their RANGE */
                alt_lane_range(i - 2u, s);
        }
    alt_hold = 0;
}

/* the LFO page has this frame (1), or it has just been left (0: the frame goes on as usual) */
static int alt_matrix_input(uint32_t pressed)
{
    uint32_t i;
    int32_t s;
    (void)pressed;
    if ((s = panel_enc(EN_ALGO)) != 0) {
        au.mtab = (uint8_t)clamp((int32_t)au.mtab + (s > 0 ? 1 : -1), 0, ALT_TAB_MTX);
        au.dd = 0;
    }
    if ((s = panel_enc(EN_SELECT)) != 0) {       /* MTX: the slot; the LFO tabs: the circle (its three LFOs) */
        if (au.mtab == ALT_TAB_MTX)
            au.mrow[0] = (uint8_t)clamp((int32_t)au.mrow[0] + s, 0, NMSLOT - 1);
        else
            alt_select((uint32_t)((int32_t)au.sel + s + 3 * 16));
    }
    for (i = 0; i < 4u; i++)
        if ((s = panel_enc(EN_K1 + i)) != 0) {
            if (au.mtab == ALT_TAB_MTX)
                alt_mtx_turn(i, s);
            else
                alt_lfo_turn(au.mtab, i, s);
        }
    (void)panel_enc(EN_PRESET);
    alt_hold = 0;
    return 1;
}

/* the quick page showing: a page button held, else the one kept open */
static uint32_t alt_page(void)
{
    uint32_t q;
    for (q = 1; q < QP_N; q++)
        if ((fm1_in.buttons >> panel.btn[ALT_QP_BTN[q]]) & 1u)
            return q;
    return au.qp;
}

#define ALT_DICE_KEYS ((1u << 7) | (1u << 9) | (1u << 11))   /* DICE 1 2 3 (ALT_KEY) */
#define ALT_MUTE_KEYS ((1u << 0) | (1u << 2) | (1u << 4))    /* MUTE 1 2 3 */
#define ALT_TC_KEYS ((1u << 1) | (1u << 3) | (1u << 5))     /* CIRCLE 1 2 3 (the first three blacks) */

static uint32_t alt_cr(uint32_t n)               /* 0 .. n-1 */
{
    uint32_t x = alt_crng ? alt_crng : (fm1_ms * 2654435761u) | 1u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    alt_crng = x;
    return n ? x % n : 0u;
}
static int16_t alt_crange(const param_desc_t *d, int32_t div)   /* anywhere in d's range (div > 1: toward its middle) */
{
    int32_t span = d->max - d->min, mid = d->min + span / 2;
    return (int16_t)(mid - span / (2 * div) + (int32_t)alt_cr((uint32_t)(span / div + 1)));
}

/* CHAOS: the three circles drawn to the extreme */
static void alt_chaos(void)
{
    static const uint8_t FXP[6] = {P_DIST, P_ITYPE, P_IA, P_IMIX, P_DLY, P_REV};
    static const uint8_t LDST[6] = {GD_SHAPE, GD_FILT, GD_LEVEL, GD_PAN, GD_SHAPE, GD_FILT};   /* (not DIST nor the sends) */
    uint32_t c, k;
    alt_crng ^= fm1_ms * 0x9E3779B9u;
    alt_seq.scale = (uint8_t)alt_cr(AS_NSCALE);
    for (k = 0; k < NELEM(alt_gl.amt); k++)      /* the GLOBAL off (one LFO per circle, no more) */
        alt_gl.amt[k] = 0;
    for (c = 0; c < AS_NCIRC; c++) {
        track_t *t = &trk[c];
        uint32_t pi = alt_cr(ALT_NPRE), e = ALT_ENGS[ALT_PRE[pi].eng % ALT_NENG], ei;
        const engine_t *en = ENGINES[e];
        as_cfg_t *cf = &au.cfg[c];
        alt_load_sound(c, pi);                   /* a sound of ours, then everything in it drawn */
        fm1_irq_off();
        for (k = 0; k < 8u; k++)                 /* the engine's eight, anywhere */
            t->p[P_E0 + k] = alt_crange(&en->edit[k], 1);
        for (k = 0; k < sizeof FXP; k++)         /* the effects, kept low (Romain: DIST and INSERT 10 % at most, */
            t->p[FXP[k]] = FXP[k] == P_ITYPE ? (int16_t)alt_cr(IT_CRUSH + 1u)   /* the sends 33 %) */
                         : (int16_t)alt_cr(FXP[k] == P_DLY || FXP[k] == P_REV ? 43u : 14u);
        afx_edit |= (uint8_t)(1u << c);          /* (the FX page shows its DIST changed: SOUND *) */
        alt_cl[c][0].wave = (uint8_t)alt_cr(GW_N);   /* its LFO 1: any wave and rate, a DEST of the timbre or the mix */
        alt_cl[c][0].rate = (uint8_t)alt_cr(GL_NRATE);
        alt_cl[c][0].dst = LDST[alt_cr(6u)];
        alt_cl[c][0].depth = (int8_t)((int32_t)alt_cr(33u) - 16);   /* (Romain 01/11: light, -16 .. 16 of 64) */
        for (k = 1; k < CL_N; k++)               /* one LFO per circle at most: LFO 2 and 3 off */
            alt_cl[c][k].depth = 0;
        for (k = 0; k < NMSLOT; k++)             /* the matrix: one slot at most, light (AMT -25 .. 25), half the time */
            t->p[P_M1SRC + 3u * k] = t->p[P_M1DST + 3u * k] = t->p[P_M1AMT + 3u * k] = 0;
        if (alt_cr(2u)) {
            static const uint8_t MSRC[6] = {MS_LFO, MS_ENV, MS_VEL, MS_RAND, MS_SLEW, MS_FORCE};
            static const uint8_t MDST[5] = {MD_CUT, MD_SHP, MD_PAN, MD_E1, MD_E1 + 1};   /* (not the pitch) */
            int32_t a = 6 + (int32_t)alt_cr(20u);
            t->p[P_M1SRC] = MSRC[alt_cr(6u)];
            t->p[P_M1DST] = MDST[alt_cr(5u)];
            t->p[P_M1AMT] = (int16_t)(alt_cr(2u) ? a : -a);
        }
        for (k = 0; k < ALT_NSLOT; k++)
            alt_applied[c][k] = 0;
        alt_note[c][0] = alt_note[c][1] = alt_note[c][2] = 0;
        alt_gate_left[c] = 0;
        fm1_irq_on();
        alt_preset_cnt[c]++;
        ei = 5u + alt_cr(28u);                   /* the rhythm: LEN 5..32, anything in it, at once */
        cf->len = (uint8_t)ei;
        cf->hits = (uint8_t)(1u + alt_cr(ei));
        cf->rot = (uint8_t)alt_cr(ei);
        cf->frot = (uint8_t)alt_cr(ei);
        cf->prob = (uint8_t)(80u + alt_cr(48u));
        cf->dose = 127;
        cf->root = (uint8_t)(24u + alt_cr(37u)); /* C1 .. C4 */
        {                                        /* the SEQ menu's lanes: each (VELO CHANCE DELAY REVERB PITCH) drawn with a 40 %
                                                  * chance, on about half its steps, else plain (Romain: under 50 % each,
                                                  * DELAY and REVERB 40 % of the bar at most) */
            as_circ_t *sc = &alt_seq.c[c];
            uint32_t pick = 0;
            for (k = 0; k < AS_NLR; k++)
                sc->rmask[k] = 0;                /* (no RANGE: CHAOS draws single values) */
            for (k = 0; k < 6u; k++)
                if (alt_cr(100u) < 40u)
                    pick |= 1u << k;
            for (k = 0; k < AS_NLANE; k++) {
                uint8_t ch = (pick & 2u) && alt_cr(2u) ? (uint8_t)(40u + alt_cr(88u)) : 127u;
                sc->svel[k] = (pick & 1u) && alt_cr(2u) ? (uint8_t)(40u + alt_cr(88u)) : AS_VEL[AS_NORMAL];
                sc->chance[k] = sc->chance[k + AS_NLANE] = ch;
                sc->sdly[k] = (pick & 4u) && alt_cr(2u) ? (uint8_t)alt_cr(52u) : 0u;   /* (0..51: 40 %) */
                sc->srev[k] = (pick & 8u) && alt_cr(2u) ? (uint8_t)alt_cr(52u) : 0u;
                sc->spitch[k] = (int8_t)((pick & 16u) && alt_cr(2u) ? (int32_t)alt_cr(15u) - 7 : 0);
                sc->srat[k] = (uint8_t)((pick & 32u) && !alt_cr(4u) ? 1u + alt_cr(2u) : 0u);   /* (RATCH: x2 x3, rarely) */
            }
        }
        alt_post_now |= (uint8_t)(1u << c);
        alt_post_circle(c);
        alt_roll_cnt[c]++;                       /* and the die at full DOSE */
        alt_cal(c);                              /* (its level aligned on its first hits) */
    }
    alt_say("CHAOS");
}

/* MUTE 1+2+3 (Romain, 01/11): the chosen circle's LEN (4..16) and HITS drawn, at once, and its VELO, CHANCE and
 * PITCH lanes drawn anew on their 16 steps, single
 * values (no RANGE), as the SEQ menu's SHUFFLE draws them (VELO and CHANCE kept audible: 30 % and up) */
static void alt_lanes_random(void)
{
    static const uint8_t LN[3] = {LN_VELO, LN_CHANCE, LN_PITCH};
    as_circ_t *sc = &alt_seq.c[au.sel % AS_NCIRC];
    uint32_t i, k;
    alt_crng ^= fm1_ms * 0x9E3779B9u;
    for (i = 0; i < 3u; i++) {
        int32_t lo, hi, st;
        alt_lane_lim(LN[i], &lo, &hi, &st);
        if (LN[i] != LN_PITCH)
            lo = 38;
        for (k = 0; k < AS_NLANE; k++) {
            int32_t v = lo + (int32_t)alt_cr((uint32_t)(hi - lo + 1));
            if (LN[i] == LN_VELO)
                sc->svel[k] = (uint8_t)v;
            else if (LN[i] == LN_CHANCE)
                sc->chance[k] = sc->chance[k + AS_NLANE] = (uint8_t)v;
            else
                sc->spitch[k] = (int8_t)v;
        }
        sc->rmask[LN[i]] = 0;
    }
    {                                            /* and its rhythm (Romain): LEN 4..16, HITS 1..LEN, at once */
        uint32_t c = au.sel % AS_NCIRC, n = 4u + alt_cr(13u);
        as_cfg_t *cf = &au.cfg[c];
        cf->len = (uint8_t)n;
        cf->hits = (uint8_t)(1u + alt_cr(n));
        cf->rot = (uint8_t)(cf->rot % n);
        cf->frot = (uint8_t)(cf->frot % n);
        alt_post_now |= (uint8_t)(1u << c);
        alt_post_circle(c);
        alt_cal(c);
    }
    alt_say("SHUFFLE");
}

/* TOTAL CHAOS (SHIFT -1 + SHIFT +1 + REVERSE, Romain 01/11): CHAOS without its limits, on the three circles: engine and
 * sound, the eight values, ADSR, envelope mode and its settings, DIST and INSERT (any type), sends, the three LFOs
 * (made light 01/11: LFO 1 only, DEPTH -20..20; one matrix slot, AMT -25..25; never the pitch), rhythm (EUCLID / TURING, LEN, HITS, ..), the six lanes,
 * chord, root, the scale, the shared delay and reverb; the die at full DOSE. Not: tempo, swing, mutes, the MASTER.
 * The volume kept (Romain): each sound's LEVEL as it is, INSERT LEVEL at most 0 dB, delay FEEDBACK under the howl.
 * The same combo within 3 s puts everything back as it was (alt_tc_undo) */
#define ALT_TC_UNDO_MS 3000u
static const uint8_t ALT_DT[11] = {3, 5, 2, 11, 4, 1, 10, 0, 12, 6, 7};   /* (the delay's times: params.c N_DTIME) */
static const uint8_t ALT_TC_G[7] = {G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX, G_RTYPE, G_RSIZE, G_RDAMP};
static struct {
    uint8_t ok;
    uint8_t eng[AS_NCIRC], pre[AS_NCIRC], mode[AS_NCIRC], scale;
    int16_t p[AS_NCIRC][P_COUNT];
    as_cfg_t cfg[AS_NCIRC];
    alt_cl_t cl[AS_NCIRC][CL_N];
    uint8_t env[AS_NCIRC][AE_N];
    uint8_t svel[AS_NCIRC][AS_NLANE], chance[AS_NCIRC][AS_MAXLEN], sdly[AS_NCIRC][AS_NLANE], srev[AS_NCIRC][AS_NLANE];
    int8_t spitch[AS_NCIRC][AS_NLANE];
    uint8_t srat[AS_NCIRC][AS_NLANE];
    uint16_t rmask[AS_NCIRC][AS_NLR];
    int8_t rlo[AS_NCIRC][AS_NLR][AS_NLANE], rhi[AS_NCIRC][AS_NLR][AS_NLANE];
    int16_t g[sizeof ALT_TC_G];
    uint8_t afx_sel[AFX_NS], afx_edit;
    int32_t cal[AS_NCIRC];
} alt_tcu __attribute__((section(".pool")));   /* (main loop only) */

static void alt_tc_snap(void)
{
    uint32_t c, k;
    for (c = 0; c < AS_NCIRC; c++) {
        as_circ_t *sc = &alt_seq.c[c];
        alt_tcu.eng[c] = trk[c].eng_req;
        alt_tcu.pre[c] = au.pre[c];
        alt_tcu.mode[c] = alt_emode[c];
        for (k = 0; k < P_COUNT; k++)
            alt_tcu.p[c][k] = (int16_t)alt_base(c, k);   /* (without the die's offsets) */
        alt_tcu.cfg[c] = au.cfg[c];
        alt_tcu.cal[c] = ac_tg[c];
        for (k = 0; k < CL_N; k++)
            alt_tcu.cl[c][k] = alt_cl[c][k];
        for (k = 0; k < AE_N; k++)
            alt_tcu.env[c][k] = alt_envp[c][k];
        memcpy(alt_tcu.svel[c], sc->svel, sizeof sc->svel);
        memcpy(alt_tcu.chance[c], sc->chance, sizeof sc->chance);
        memcpy(alt_tcu.sdly[c], sc->sdly, sizeof sc->sdly);
        memcpy(alt_tcu.srev[c], sc->srev, sizeof sc->srev);
        memcpy(alt_tcu.spitch[c], sc->spitch, sizeof sc->spitch);
        memcpy(alt_tcu.srat[c], sc->srat, sizeof sc->srat);
        memcpy(alt_tcu.rmask[c], sc->rmask, sizeof sc->rmask);
        memcpy(alt_tcu.rlo[c], sc->rlo, sizeof sc->rlo);
        memcpy(alt_tcu.rhi[c], sc->rhi, sizeof sc->rhi);
    }
    alt_tcu.scale = alt_seq.scale;
    for (k = 0; k < sizeof ALT_TC_G; k++)
        alt_tcu.g[k] = song.g[ALT_TC_G[k]];
    memcpy(alt_tcu.afx_sel, afx_sel, sizeof afx_sel);
    alt_tcu.afx_edit = afx_edit;
    alt_tcu.ok = 1;
}

static void alt_tc_undo(void)
{
    uint32_t c, k;
    for (c = 0; c < AS_NCIRC; c++) {
        track_t *t = &trk[c];
        as_circ_t *sc = &alt_seq.c[c];
        uint8_t mute = au.cfg[c].mute;
        if (t->eng_req != alt_tcu.eng[c])
            set_engine_of(t, alt_tcu.eng[c]);
        fm1_irq_off();
        for (k = 0; k < P_COUNT; k++)
            t->p[k] = alt_tcu.p[c][k];
        for (k = 0; k < ALT_NSLOT; k++)
            alt_applied[c][k] = 0;
        alt_note[c][0] = alt_note[c][1] = alt_note[c][2] = 0;
        alt_gate_left[c] = 0;
        fm1_irq_on();
        au.pre[c] = alt_tcu.pre[c];
        alt_emode[c] = alt_tcu.mode[c];
        au.cfg[c] = alt_tcu.cfg[c];
        au.cfg[c].mute = mute;                   /* (the mutes are the player's: as they are now) */
        for (k = 0; k < CL_N; k++)
            alt_cl[c][k] = alt_tcu.cl[c][k];
        for (k = 0; k < AE_N; k++)
            alt_envp[c][k] = alt_tcu.env[c][k];
        memcpy(sc->svel, alt_tcu.svel[c], sizeof sc->svel);
        memcpy(sc->chance, alt_tcu.chance[c], sizeof sc->chance);
        memcpy(sc->sdly, alt_tcu.sdly[c], sizeof sc->sdly);
        memcpy(sc->srev, alt_tcu.srev[c], sizeof sc->srev);
        memcpy(sc->spitch, alt_tcu.spitch[c], sizeof sc->spitch);
        memcpy(sc->srat, alt_tcu.srat[c], sizeof sc->srat);
        memcpy(sc->rmask, alt_tcu.rmask[c], sizeof sc->rmask);
        memcpy(sc->rlo, alt_tcu.rlo[c], sizeof sc->rlo);
        memcpy(sc->rhi, alt_tcu.rhi[c], sizeof sc->rhi);
        alt_preset_cnt[c]++;
        alt_post_now |= (uint8_t)(1u << c);
        alt_post_circle(c);
        alt_clear_cnt[c]++;                      /* (the die CHAOS rolled: clean again) */
        alt_cal_to(c, alt_tcu.cal[c]);           /* (its level as it was) */
    }
    alt_seq.scale = alt_tcu.scale;
    for (k = 0; k < sizeof ALT_TC_G; k++)
        song.g[ALT_TC_G[k]] = alt_tcu.g[k];
    memcpy(afx_sel, alt_tcu.afx_sel, sizeof afx_sel);
    afx_edit = alt_tcu.afx_edit;
    alt_say("CHAOS UNDONE");
}

static void alt_total_chaos(void)
{
    uint32_t c, k;
    if (au.tc_ms && alt_tcu.ok && (int32_t)(fm1_ms - au.tc_ms) < (int32_t)ALT_TC_UNDO_MS) {
        au.tc_ms = 0;
        alt_tc_undo();
        return;
    }
    alt_tc_snap();
    au.tc_ms = fm1_ms | 1u;
    alt_crng ^= fm1_ms * 0x9E3779B9u;
    alt_seq.scale = (uint8_t)alt_cr(AS_NSCALE);
    song.g[G_DTIME] = ALT_DT[alt_cr(sizeof ALT_DT)];   /* the shared delay and reverb, anywhere (FEEDBACK under the */
    song.g[G_DFDBK] = (int16_t)alt_cr(101u);            /* howl: 0..100 of 120) */
    song.g[G_DCOLOR] = (int16_t)alt_cr(128u);
    song.g[G_DMIX] = (int16_t)(40u + alt_cr(88u));
    song.g[G_RTYPE] = (int16_t)alt_cr(3u);
    song.g[G_RSIZE] = (int16_t)alt_cr(128u);
    song.g[G_RDAMP] = (int16_t)alt_cr(128u);
    afx_edit |= 1u << AFX_S_DLY | 1u << AFX_S_REV;
    for (c = 0; c < AS_NCIRC; c++) {
        track_t *t = &trk[c];
        uint32_t pi = alt_cr(ALT_NPRE), e = ALT_ENGS[ALT_PRE[pi].eng % ALT_NENG], n;
        const engine_t *en = ENGINES[e];
        as_cfg_t *cf = &au.cfg[c];
        as_circ_t *sc = &alt_seq.c[c];
        alt_load_sound(c, pi);                   /* any engine, any sound, then everything in it drawn */
        fm1_irq_off();
        for (k = 0; k < 8u; k++)
            t->p[P_E0 + k] = alt_crange(&en->edit[k], 1);
        for (k = 0; k < 4u; k++)                 /* ATK DEC SUS REL */
            t->p[P_ATK + k] = (int16_t)alt_cr(128u);
        t->p[P_DIST] = (int16_t)alt_cr(128u);
        t->p[P_ITYPE] = (int16_t)alt_cr(IT_N);
        t->p[P_IA] = (int16_t)alt_cr(128u);
        t->p[P_IB] = (int16_t)alt_cr(128u);
        t->p[P_IC] = (int16_t)alt_cr(113u);      /* (its LEVEL: 112 = 0 dB at most) */
        t->p[P_IMIX] = (int16_t)alt_cr(128u);
        t->p[P_DLY] = (int16_t)alt_cr(128u);
        t->p[P_REV] = (int16_t)alt_cr(128u);
        /* the matrix and the LFOs, light (Romain 01/11: "not musical"): one matrix slot at most (70 %), AMT -25 .. 25;
         * one LFO at most (LFO 1), DEPTH -20 .. 20 of 64; never the pitch (matrix PITCH / VIB, LFO PITCH) */
        for (k = 0; k < NMSLOT; k++)
            t->p[P_M1SRC + 3u * k] = t->p[P_M1DST + 3u * k] = t->p[P_M1AMT + 3u * k] = 0;
        if (alt_cr(10u) < 7u) {
            int32_t a = 4 + (int32_t)alt_cr(22u);
            uint32_t d = 2u + alt_cr(MD_DEPTH - 3u);   /* CUT .. E8, VIB skipped */
            t->p[P_M1SRC] = (int16_t)(1u + alt_cr(MS_N - 1u));
            t->p[P_M1DST] = (int16_t)(d >= MD_VIB ? d + 1u : d);
            t->p[P_M1AMT] = (int16_t)(alt_cr(2u) ? a : -a);
        }
        for (k = 0; k < CL_N; k++)
            alt_cl[c][k].depth = 0;
        alt_cl[c][0].wave = (uint8_t)alt_cr(GW_N);
        alt_cl[c][0].rate = (uint8_t)alt_cr(GL_NRATE);
        alt_cl[c][0].dst = (uint8_t)(GD_SHAPE + alt_cr(GD_N - GD_SHAPE));
        alt_cl[c][0].depth = (int8_t)((int32_t)alt_cr(41u) - 20);
        for (k = 0; k < ALT_NSLOT; k++)
            alt_applied[c][k] = 0;
        alt_note[c][0] = alt_note[c][1] = alt_note[c][2] = 0;
        alt_gate_left[c] = 0;
        fm1_irq_on();
        afx_edit |= (uint8_t)(1u << c);
        alt_emode[c] = (uint8_t)alt_cr(AM_N);    /* FREE PERC STAB DRONE, and its settings */
        for (k = 0; k < AE_N; k++)
            alt_envp[c][k] = (uint8_t)alt_cr(128u);
        cf->type = (uint8_t)alt_cr(AS_NTYPE);    /* the rhythm: EUCLID or TURING, anything in it, at once */
        n = cf->type == AS_T_EUCLID ? 1u + alt_cr(32u) : 1u + alt_cr(16u);
        cf->len = (uint8_t)n;
        cf->hits = (uint8_t)(1u + alt_cr(n));
        cf->rot = (uint8_t)alt_cr(n);
        cf->frot = (uint8_t)alt_cr(n);
        cf->prob = (uint8_t)(64u + alt_cr(64u));
        cf->tlock = (uint8_t)alt_cr(128u);
        cf->tdens = (uint8_t)alt_cr(128u);
        cf->trange = (uint8_t)alt_cr(15u);
        cf->dose = 127;
        cf->chord = (uint8_t)alt_cr(AS_NCHORD);
        cf->root = (uint8_t)(24u + alt_cr(49u)); /* C1 .. C5 */
        for (k = 0; k < AS_NLR; k++)
            sc->rmask[k] = 0;
        for (k = 0; k < AS_NLANE; k++) {         /* the six lanes, every step (CHANCE: something still plays) */
            sc->svel[k] = (uint8_t)(1u + alt_cr(127u));
            sc->chance[k] = sc->chance[k + AS_NLANE] = (uint8_t)(20u + alt_cr(108u));
            sc->sdly[k] = (uint8_t)alt_cr(128u);
            sc->srev[k] = (uint8_t)alt_cr(128u);
            sc->spitch[k] = (int8_t)((int32_t)alt_cr(15u) - 7);
            sc->srat[k] = (uint8_t)alt_cr(4u);
        }
        alt_post_now |= (uint8_t)(1u << c);
        alt_post_circle(c);
        alt_roll_cnt[c]++;                       /* and the die at full DOSE */
        alt_cal(c);                              /* (its level aligned on its first hits) */
    }
    alt_say("TOTAL CHAOS");
}

static void alt_select(uint32_t i)
{
    au.sel = (uint8_t)(i % AS_NCIRC);
    alt_sel = au.sel;
    track_select(au.sel);                        /* Felucca's pages (ENV, FX, EDIT ..) edit this circle's sound */
}

/* 1: the PLAY view had this frame's input (ui_input stops); 0: Felucca's (another page, or a button that opens one) */
/* the page buttons, all alike (Romain, 01/11): a tap = the quick page (the four knobs over the circles; tapped again:
 * gone), held 0.5 s = the button's menu, which stays (the button tapped, HOME or another page button: gone).
 * Menus: SEQ (lanes), FX (the FX page), LFO (LFO 1 2 3 GLOBAL MTX), ENV (the envelope drawn), EDIT (SETTINGS), SCL (the
 * scales). GLO has none yet: held = tapped */
#define ALT_MENU_HOLD_MS 500u
static uint32_t alt_menu_cur(void)
{
    return au.seq_open ? QP_SEQ : afp.open ? QP_FX : au.mat_open ? QP_LFO : au.mn;
}
static void alt_menu_set(uint32_t q)
{
    au.seq_open = (uint8_t)(q == QP_SEQ);
    afp.open = (uint8_t)(q == QP_FX);
    au.mat_open = (uint8_t)(q == QP_LFO);
    au.mn = (uint8_t)(q == QP_ENV || q == QP_EDIT || q == QP_SCL ? q : 0u);
    au.qp = (uint8_t)(q == QP_ENV || q == QP_SCL ? q : QP_NONE);   /* (ENV, SCL: their quick page's knobs, drawn big) */
    au.step_down = 0;
    au.dd = 0;
    au.set_row = 0;
}
static const char *alt_menu_name(uint32_t q)
{
    return q == QP_SEQ ? "SEQ" : q == QP_FX ? "FX" : q == QP_LFO ? "LFO" : q == QP_ENV ? "ENVELOPE" :
           q == QP_EDIT ? "SETTINGS" : q == QP_SCL ? "SCALES" : "";
}

static void afp_input(uint32_t pressed);                     /* the FX page (alter_fxpage.c, with the screen below) */
static void alt_set_input(void);                 /* SETTINGS (with the screens below) */
static int alter_ui_input(uint32_t pressed)
{
    uint32_t keys = fm1_in.notes, down, k, i, ours = alt_ours(), q, octs, both;
    int32_t s;
    both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
    octs = (fm1_in.buttons & both) == both;
    if (octs && alter_view() && !au.handed && ((pressed >> panel.btn[B_HOME]) & 1u)) {   /* OCT- + OCT+ + HOME: NEW
                                                  * SCENE at once (Romain 01/11), every page closed */
        alt_row_load(0, 0);
        au.qp = QP_NONE;
        au.qp_was = 0;
        au.scene_open = 0;
        au.lockc = 0;
        alt_menu_set(0);
        au.mhold = 0;
        alt_hold = 0;
        alt_say("NEW SCENE");
        au.keys = keys;
        return 1;
    }
    if (octs && alter_view() && !au.handed)      /* OCT- + OCT+ + a page button: Felucca's own page */
        for (q = 1; q <= QP_N; q++)              /* (QP_N: SAVE, Felucca's save pages) */
            if ((pressed >> panel.btn[q < QP_N ? ALT_QP_BTN[q] : B_SAVE]) & 1u) {
                au.handed = 1;
                au.handed_ms = fm1_ms;
                au.qp = QP_NONE;
                au.qp_was = 0;
                au.scene_open = 0;
                alt_menu_set(0);
                au.mhold = 0;
                alt_hold = 0;
                au.keys = keys;
                return 0;
            }
    if ((pressed | fm1_in.buttons) & ~ours) {    /* a Felucca button (a page, HOME, SAVE ..): Felucca's until every */
        au.handed = 1;                           /* one has been up 0.4 s (those that act on release, a tap told */
        au.handed_ms = fm1_ms;                   /* from a hold) */
    } else if (au.handed && fm1_ms - au.handed_ms > 400u)
        au.handed = 0;
    if (!alter_view() || !alt_ready || au.handed) {
        alt_kb_own = (uint8_t)(alter_view() && alt_ready);   /* (the keys stay silent on the view) */
        alt_hold = 0;
        if (!alter_view())
            au.drawn = 0;
        au.keys = keys;
        return 0;
    }
    alt_kb_own = 1;
    if (!au.ready) {
        for (i = 0; i < AS_NCIRC; i++)
            au.cfg[i] = alt_seq.c[i].next;
        au.ready = 1;
        alt_select(song.sel < AS_NCIRC ? song.sel : 0u);
        alt_banks_load();                        /* the USER slots, from the flash */
        alt_load_scene(0);                       /* power-on: the first scene */
    }
    if (song.rec)                                /* (no recording in ALTER: an armed REC would count in and click) */
        song.rec = 0;
    if (song.sel != au.sel && song.sel < AS_NCIRC)   /* (chosen elsewhere: TRACKS, the editor) */
        au.sel = alt_sel = song.sel;
    if ((pressed >> panel.btn[B_PLAY]) & 1u)
        transport_req = song.playing || seq_counting() ? 2u : 1u;
    {                                            /* REC (alter_rec.c): tap = overdub on / off; held 1 s = the chosen
                                                  * circle cleared; OCT- + OCT+ + REC = every circle cleared */
        uint32_t rd = (fm1_in.buttons >> panel.btn[B_REC]) & 1u;
        if ((pressed >> panel.btn[B_REC]) & 1u) {
            au.rec_t0 = fm1_ms | 1u;
            au.rec_done = 0;
            if (octs) {
                for (i = 0; i < AS_NCIRC; i++) {
                    ar_clr_cnt[i]++;
                    alt_add_set(i, 0);           /* (and the steps TRIGGER added, the fills REC'd) */
                    alt_fxr_put(i, 0);
                }
                ar_on = 0;
                au.rec_done = 1;
                alt_say("REC CLEAR ALL");
            }
        } else if (rd && au.rec_t0 && !au.rec_done && (int32_t)(fm1_ms - au.rec_t0) >= (int32_t)ALT_REC_CLEAR_MS) {
            ar_clr_cnt[au.sel % AS_NCIRC]++;
            alt_add_set(au.sel, 0);              /* (and the steps TRIGGER added, the fills REC'd) */
            alt_fxr_put(au.sel, 0);
            au.rec_done = 1;
            alt_say("REC CLEAR");
        } else if (!rd && au.rec_t0) {
            if (!au.rec_done) {
                ar_on ^= 1u;
                alt_say(ar_on ? "REC ON" : "REC OFF");
            }
            au.rec_t0 = 0;
        }
    }
    if ((pressed >> panel.btn[B_HOME]) & 1u) {   /* HOME: the circles, no page */
        au.qp = QP_NONE;
        au.lockc = 0;
        au.scene_open = 0;
        alt_menu_set(0);
    }
    if (((pressed >> panel.btn[B_SAVE]) & 1u) && !au.scene_open) {   /* SAVE: the SAVE page (its press opens it) */
        au.scene_open = 1;
        au.save_t0 = 0;
        au.qp = QP_NONE;
        au.lockc = 0;
        alt_menu_set(0);
    } else if (au.scene_open) {                  /* on it: a tap loads, held 1 s stores */
        uint32_t dn = (fm1_in.buttons >> panel.btn[B_SAVE]) & 1u, t = au.tab, r = au.row[t];
        if (((pressed >> panel.btn[B_SAVE]) & 1u)) {
            au.save_t0 = fm1_ms | 1u;
            au.save_done = 0;
        } else if (dn && au.save_t0 && !au.save_done && (int32_t)(fm1_ms - au.save_t0) >= (int32_t)ALT_SAVE_HOLD_MS) {
            int rc = alt_row_save(t, r);
            au.save_done = 1;
            alt_say(rc > 0 ? "SAVED" : rc == 0 ? "PROTECTED" : "SAVE FAILED");
        } else if (!dn && au.save_t0) {
            if (!au.save_done) {
                static char nb[12];                  /* (the middle shows it a while: kept) */
                if (alt_row_load(t, r)) {
                    alt_say(alt_row_name(t, r, nb));
                    au.scene_open = 0;
                } else
                    alt_say("EMPTY");
            }
            au.save_t0 = 0;
        }
    }
    if (au.scene_open) {                         /* the SAVE page: SELECT picks the row, ALGORITHM the tab */
        if ((s = panel_enc(EN_SELECT)) != 0)
            au.row[au.tab] = (uint8_t)clamp((int32_t)au.row[au.tab] + s, 0, ALT_NLIST - 1);
        if ((s = panel_enc(EN_ALGO)) != 0)
            au.tab = (uint8_t)(s > 0 ? 1 : 0);
        for (i = 0; i < 4u; i++)
            (void)panel_enc(EN_K1 + i);
        (void)panel_enc(EN_PRESET);
        alt_hold = 0;
        au.keys = keys;
        return 1;
    }
    for (q = 1; q < QP_N; q++) {                 /* the page buttons: a tap the quick page, held 0.5 s the menu */
        uint32_t dn = (fm1_in.buttons >> panel.btn[ALT_QP_BTN[q]]) & 1u, was = (au.qp_was >> q) & 1u;
        if (dn && !was) {
            au.qp_t0[q] = fm1_ms;
            au.lockc = 0;
            au.mhold &= (uint8_t)~(1u << q);
            if (alt_menu_cur() && alt_menu_cur() != q)
                alt_menu_set(0);                 /* (another page: the menu open closes) */
            alt_say(alt_qp_name(q));
        } else if (dn && q != QP_GLO && alt_menu_cur() != q && !((au.mhold >> q) & 1u) &&
                   (int32_t)(fm1_ms - au.qp_t0[q]) >= (int32_t)ALT_MENU_HOLD_MS) {
            alt_menu_set(q);                     /* held: its menu */
            au.mhold |= (uint8_t)(1u << q);
            alt_say(alt_menu_name(q));
        } else if (!dn && was) {
            if ((au.mhold >> q) & 1u)            /* the hold's release: the menu stays */
                au.mhold &= (uint8_t)~(1u << q);
            else if (alt_menu_cur() == q)        /* tapped on its menu: back to the circles */
                alt_menu_set(0);
            else
                au.qp = (uint8_t)(au.qp == q ? QP_NONE : q);
        }
        au.qp_was = (uint8_t)((au.qp_was & ~(1u << q)) | (dn << q));
    }
    if (au.mat_open && alt_matrix_input(pressed)) {
        au.keys = keys;
        return 1;
    }
    if (au.mn == QP_EDIT) {                      /* SETTINGS */
        alt_set_input();
        au.keys = keys;
        return 1;
    }
    if (((fm1_in.buttons >> panel.btn[B_SEQ]) & 1u) && (s = panel_enc(EN_ALGO)) != 0)
        alt_seq_type(s);                         /* SEQ held + ALGORITHM: the chosen circle's engine */
    if (au.seq_open) {                           /* the SEQ menu */
        alt_seq_input(keys);
        au.keys = keys;
        return 1;
    }
    q = alt_page();
    if (afp.open)                                /* the FX page: its encoders (the keys play on: mutes, dice, fills) */
        afp_input(pressed);
    for (i = 0; i < 4u; i++)
        if ((s = panel_enc(EN_K1 + i)) != 0 && !au.lockc)
            alt_turn(q, i, s);
    if ((s = panel_enc(EN_SELECT)) != 0 && !au.lockc)
        alt_select((uint32_t)((int32_t)au.sel + s + 3 * 16));
    if ((s = panel_enc(EN_ALGO)) != 0 && !au.lockc) {   /* the chosen circle's envelope mode */
        alt_emode[au.sel] = (uint8_t)clamp((int32_t)alt_emode[au.sel] + (s > 0 ? 1 : -1), 0, AM_N - 1);
        alt_say(ALT_MODE_NAME[alt_emode[au.sel]]);
    }
    if ((s = panel_enc(EN_PRESET)) != 0 && !au.lockc)
        alt_preset_step(au.sel, s);
    down = keys & ~au.keys;
    if (au.lockc) {                              /* the LOCKS page: the keys are its seven locks */
        as_circ_t *lc = &alt_seq.c[au.lockc - 1u];
        for (i = 0; i < 7u; i++)
            if ((down >> ALT_LK_KEY[i]) & 1u)
                lc->locks ^= ALT_LK_BIT[i];
        alt_hold = 0;
        au.keys = keys;
        return 1;
    }
    /* the combos: MUTE 1+2+3 = the chosen circle's lanes drawn (the mutes they toggled put back); CIRCLE 1+2+3 = TOTAL
     * CHAOS (the chosen circle put back) */
    if ((keys & ALT_MUTE_KEYS) == ALT_MUTE_KEYS && (down & ALT_MUTE_KEYS) && !au.mute_combo) {
        for (i = 0; i < AS_NCIRC; i++)
            if ((au.mute_tog >> i) & 1u) {
                au.cfg[i].mute ^= 1u;
                alt_post_circle(i);
            }
        au.mute_tog = 0;
        au.mute_combo = 1;
        alt_lanes_random();
    }
    if ((down & ALT_TC_KEYS) && !(au.keys & ALT_TC_KEYS))   /* the first CIRCLE key: where the selection was */
        au.sel_was = au.sel;
    if ((keys & ALT_TC_KEYS) == ALT_TC_KEYS && (down & ALT_TC_KEYS) && !au.tc_combo) {
        if (au.sel != au.sel_was)
            alt_select(au.sel_was);
        au.tc_combo = 1;
        alt_total_chaos();
    }
    /* the keys */
    for (k = 0; k < 27u; k++) {
        const alt_key_t *a = &ALT_KEY[k];
        uint32_t i2 = a->arg;
        int tur = au.cfg[au.sel].type == AS_T_TURING;
        if ((down >> k) & 1u && a->kind && (a->kind != AK_TLOCK || tur)) {
            int combo = (a->kind == AK_MUTE && au.mute_combo) || (au.tc_combo && ((ALT_TC_KEYS >> k) & 1u));
            if (!combo && a->kind != AK_MUTE && a->kind != AK_SHIFT && a->kind != AK_SEL)
                alt_say(a->name);
            if (combo) {
                ;                                /* (part of a combo: nothing else) */
            } else if (a->kind == AK_MUTE) {
                alt_say(a->name);
                au.cfg[i2].mute ^= 1u;
                au.mute_tog |= (uint8_t)(1u << i2);
                alt_post_circle(i2);
            } else if (a->kind == AK_DIE && octs) {   /* OCT- + OCT+ + DICE n: circle n's LOCKS */
                au.lockc = (uint8_t)(i2 + 1u);
                au.qp = QP_NONE;
                alt_say("LOCKS");
                au.keys = keys;
                alt_hold = 0;
                return 1;
            } else if (a->kind == AK_DIE) {
                au.die_t0[i2] = fm1_ms | 1u;
                au.die_clean_done &= (uint8_t)~(1u << i2);
                if ((keys & ALT_DICE_KEYS) == ALT_DICE_KEYS) {   /* the three dice at once: CHAOS (no roll, no clean) */
                    au.die_clean_done = 7u;
                    alt_chaos();
                }
            } else if (a->kind == AK_DENS) {
                int32_t d = i2 ? 1 : -1;
                alt_dens_tot[au.sel] += d;
                au.cfg[au.sel].hits = (uint8_t)clamp((int32_t)au.cfg[au.sel].hits + d, 0, au.cfg[au.sel].len);
                au.tap_ms[k] = fm1_ms | 1u;
            } else if (a->kind == AK_SHIFT) {
                int32_t d = i2 ? 1 : -1;
                alt_say(tur ? (i2 ? "NEXT" : "PREV") : a->name);
                alt_shift_tot[au.sel] += d;      /* (TURING: the audio side takes it as NEXT / PREV) */
                if (!tur)
                    au.cfg[au.sel].rot = as_wrap((int32_t)au.cfg[au.sel].rot + d, au.cfg[au.sel].len);
                au.tap_ms[k] = fm1_ms | 1u;
            } else if (a->kind == AK_TRIG) {     /* TRIGGER: a hit at once; REC held or on: on the nearest step too */
                uint32_t c2 = au.sel % AS_NCIRC, rd = (fm1_in.buttons >> panel.btn[B_REC]) & 1u;
                alt_trig_tot[c2]++;
                if (rd || ar_on) {
                    alt_trig_add_tot[c2]++;
                    alt_say("TRIG ADDED");
                    if (rd)
                        au.rec_done = 1;         /* (REC's release: no on / off, no clear) */
                }
                au.tap_ms[k] = fm1_ms | 1u;
            } else if (a->kind == AK_SEL) {      /* CIRCLE n: chosen (as SELECT) */
                if (au.sel != i2)
                    alt_select(i2);
            } else if (a->kind == AK_TLOCK) {    /* TURING: frozen, or back to its LOCK knob */
                alt_seq.c[au.sel].tfreeze ^= 1u;
                alt_say(alt_seq.c[au.sel].tfreeze ? "LOCKED" : "FREE");
            }
        }
        if (a->kind != AK_DIE)
            continue;
        if ((keys >> k) & 1u) {                  /* held long: the clean pattern */
            if (au.die_t0[i2] && (int32_t)(fm1_ms - au.die_t0[i2]) >= (int32_t)ALT_DIE_HOLD_MS && !((au.die_clean_done >> i2) & 1u)) {
                au.die_clean_done |= (uint8_t)(1u << i2);
                alt_clear_cnt[i2]++;
                alt_cal(i2);
                alt_say("CLEAN");
            }
        } else if (au.die_t0[i2]) {              /* let go: a tap rolls, at once */
            if (!((au.die_clean_done >> i2) & 1u)) {
                alt_roll_cnt[i2]++;
                alt_cal(i2);
                au.tap_ms[k] = fm1_ms | 1u;
                alt_say("NEW DRAW");
            }
            au.die_t0[i2] = 0;
        }
    }
    {
        uint32_t h = 0;
        for (k = 0; k < 27u; k++)
            if ((keys >> k) & 1u && ALT_KEY[k].kind == AK_FILL && !(au.tc_combo && ((ALT_TC_KEYS >> k) & 1u)))
                h |= 1u << ALT_KEY[k].arg;
        alt_hold = (uint16_t)h;
        if (h && au.msg)                         /* a fill held: its name stays in the middle */
            au.msg_ms = fm1_ms;
    }
    for (i = 0; i < AS_NCIRC; i++)               /* the combos' keys let go */
        if (!((keys >> (2u * i)) & 1u))
            au.mute_tog &= (uint8_t)~(1u << i);
    if (!(keys & ALT_MUTE_KEYS))
        au.mute_combo = 0;
    if (!(keys & ALT_TC_KEYS))
        au.tc_combo = 0;
    au.keys = keys;
    return 1;
}

/* the key LEDs: the keys that act faint, bright when engaged; the gaps dark */
static void alter_leds(uint8_t *nl, uint8_t *nd, uint8_t *nb, uint8_t *nm)
{
    uint32_t k;
    if (!alter_view() || !au.ready)
        return;
    for (k = 0; k < 27u; k++) {
        const alt_key_t *a = &ALT_KEY[k];
        int lit = 0, faint = 0;
        if (au.seq_open) {                       /* the SEQ menu: the white keys are the steps (a hit half lit, the
                                                  * others faint, the step playing and the held ones lit: the light moves
                                                  * along); the black keys its shortcuts (the lane shown lit) */
            uint32_t j, c = au.sel % AS_NCIRC, n = au.cfg[c].len < 16u ? au.cfg[c].len : 16u, mid = 0;
            const as_circ_t *sc = &alt_seq.c[c];
            uint32_t now = sc->cfg.len ? (sc->pos + sc->cfg.len - 1u) % sc->cfg.len : 0u;
            for (j = 0; j < 16u; j++)
                if (ALT_WHITE[j] == k && j < n) {
                    lit = (int)((au.step_down >> j) & 1u) || (song.playing && now % 16u == j);
                    mid = !lit && ((sc->pat >> j) & 1u);
                    faint = !lit && !mid;
                }
            if (ALT_SEQ_BK[k] != ASK_NONE) {
                lit = ALT_SEQ_BK[k] == au.lane || (int)((fm1_in.notes >> k) & 1u);
                faint = !lit;
            }
            led_clear(nl, 14u + k);
            led_clear(nd, 14u + k);
            led_clear(nb, 14u + k);
            led_clear(nm, 14u + k);
            led_put(nl, 14u + k, lit);
            led_put(nm, 14u + k, (int)mid);
            led_put(nd, 14u + k, faint);
            continue;
        }
        if (au.lockc) {                          /* the LOCKS page: a lock lit, a free one faint */
            uint32_t j;
            for (j = 0; j < 7u; j++)
                if (ALT_LK_KEY[j] == k) {
                    lit = (alt_seq.c[au.lockc - 1u].locks & ALT_LK_BIT[j]) != 0;
                    faint = !lit;
                }
            led_clear(nl, 14u + k);
            led_clear(nd, 14u + k);
            led_clear(nb, 14u + k);
            led_clear(nm, 14u + k);
            led_put(nl, 14u + k, lit);
            led_put(nd, 14u + k, faint);
            continue;
        }
        if (a->kind == AK_MUTE)
            lit = au.cfg[a->arg].mute;
        else if (a->kind == AK_SEL)              /* the chosen circle's key lit */
            lit = au.sel == a->arg;
        else if (a->kind == AK_DIE || a->kind == AK_SHIFT || a->kind == AK_DENS || a->kind == AK_TRIG)
            lit = (int)((fm1_in.notes >> k) & 1u) || (au.tap_ms[k] && (int32_t)(fm1_ms - au.tap_ms[k]) < (int32_t)ALT_TAP_MS);
        else if (a->kind == AK_FILL)
            lit = (int)((fm1_in.notes >> k) & 1u);
        else if (a->kind == AK_TLOCK)            /* (dark unless the chosen circle is TURING) */
            lit = au.cfg[au.sel].type == AS_T_TURING && alt_seq.c[au.sel].tfreeze;
        faint = a->kind && !lit && (a->kind != AK_TLOCK || au.cfg[au.sel].type == AS_T_TURING);
        led_clear(nl, 14u + k);
        led_clear(nd, 14u + k);
        led_clear(nb, 14u + k);
        led_clear(nm, 14u + k);
        led_put(nl, 14u + k, lit);
        led_put(nd, 14u + k, faint);
    }
    led_clear(nl, panel.btn[B_REC]);             /* REC: lit while it writes, faint when the chosen circle has moves */
    led_clear(nd, panel.btn[B_REC]);
    led_put(nl, panel.btn[B_REC], ar_on != 0);
    led_put(nd, panel.btn[B_REC], !ar_on && ar_n[au.sel % AS_NCIRC]);
}

/* ------------------------------------------------------------------ the screen --- */
#define ALT_CX 120
#define ALT_CY 116
static const uint8_t ALT_R[AS_NCIRC] = {80, 60, 40};
/* the two themes (EDIT held: SETTINGS > THEME, Romain 01/11): NIGHT and DAY, both tarot cards (alter_look.c). The
 * theme is Felucca's palette setting (settings.palette, saved with the settings): DAY = PAPER, NIGHT = GREY; ALTER
 * draws in its own colours, light when the palette is (ux.light). NIGHT: crimson, verdigris, old gold on black and
 * gold leaf; DAY: coral, teal, sun on cream, indigo ink */
#define ALT_DAY (ux.light != 0)
static const uint16_t ALT_COLS[2][AS_NCIRC] = {
    {RGB(201, 80, 74), RGB(79, 168, 140), RGB(212, 174, 82)},
    {RGB(224, 102, 74), RGB(42, 157, 143), RGB(217, 154, 34)},
};
#define ALT_COL (ALT_COLS[ALT_DAY])
#define ALT_BG (ALT_DAY ? RGB(243, 230, 200) : RGB(14, 12, 16))     /* the card (DAY: cream paper, textured) */
#define ALT_GREY (ALT_DAY ? RGB(172, 152, 112) : RGB(100, 86, 60))  /* what rests (inactive words, marks) */
#define ALT_DARK (ALT_DAY ? RGB(232, 216, 184) : RGB(30, 26, 22))   /* faint fills */
#define ALT_INK (ALT_DAY ? RGB(46, 43, 85) : RGB(235, 217, 168))    /* indigo / ivory */
#define ALT_DIM (ALT_DAY ? RGB(111, 96, 75) : RGB(154, 132, 92))
#define ALT_GOLD (ALT_DAY ? RGB(200, 150, 46) : RGB(201, 162, 77))
#define ALT_FRAME (ALT_DAY ? RGB(46, 43, 85) : RGB(201, 162, 77))
#define ALT_RIB (ALT_DAY ? RGB(46, 43, 85) : RGB(74, 26, 31))      /* the title's ribbon */
#define ALT_RIBTX (ALT_DAY ? RGB(243, 230, 200) : RGB(235, 217, 168))
#define ALT_GO ALT_GOLD                                             /* PLAY */
#define ALT_RED (ALT_DAY ? RGB(200, 40, 40) : RGB(226, 60, 52))     /* REC */
/* c towards the background by (100 - pct) %, in colour whatever the palette (a grey palette's ux_mix would grey it) */
static uint16_t alt_mixc(uint16_t a, uint16_t c, int32_t pct)
{
    static const uint32_t SH[3] = {11, 5, 0}, MK[3] = {31, 63, 31};
    uint32_t k, out = 0, p = (uint32_t)clamp(pct, 0, 100);
    for (k = 0; k < 3u; k++) {
        int32_t x = (int32_t)((a >> SH[k]) & MK[k]), y = (int32_t)((c >> SH[k]) & MK[k]);
        out |= (uint32_t)(x + (y - x) * (int32_t)p / 100) << SH[k];
    }
    return (uint16_t)out;
}

static void alt_plot(int32_t x, int32_t y, uint16_t c, int32_t a);   /* (alter_look.c) */
/* a disc, its edge anti-aliased (each edge pixel as much of c as the disc covers of it) */
static void alt_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy, dx, r16 = r * 16 + 8;
    for (dy = -r - 1; dy <= r + 1; dy++)
        for (dx = -r - 1; dx <= r + 1; dx++) {
            int32_t d = (int32_t)alt_isqrt((uint32_t)((dx * dx + dy * dy) * 256)), a = (r16 - d) * 16 + 128;
            if (a >= 256)
                cv_rect(cx + dx, cy + dy, 1, 1, c);
            else if (a > 0)
                alt_plot(cx + dx, cy + dy, c, a);
        }
}

static void alt_at(uint32_t turn, int32_t r, int32_t *x, int32_t *y)   /* turn: 0 = top, clockwise */
{
    *x = ALT_CX + ((r * sine_i(turn)) >> 15);
    *y = ALT_CY - ((r * sine_i(turn + 0x40000000u)) >> 15);
}

static uint16_t alt_fade(uint16_t c, int32_t pct) { return alt_mixc(ALT_BG, c, pct); }

#include "alter_look.c"

static void alt_num(char *d, int32_t q, int pct)
{
    int32_t n = 0;
    if (q < 0) {
        d[n++] = '-';
        q = -q;
    }
    if (q >= 100)
        d[n++] = (char)('0' + q / 100);
    if (q >= 10)
        d[n++] = (char)('0' + q / 10 % 10);
    d[n++] = (char)('0' + q % 10);
    if (pct)
        d[n++] = '%';
    d[n] = 0;
}

/* knob i of quick page q as shown: its name, and its value as text (txt[0] set), or as a dial (*fill of 50; its
 * number in txt too);
 * the return: a number the picture depends on */
static uint32_t c_chord_of(uint32_t c) { return au.cfg[c % AS_NCIRC].chord % AS_NCHORD; }
static int32_t alt_kv(uint32_t q, uint32_t i, const char **name, char *txt, int32_t *fill)
{
    static const char *const NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const track_t *t = &trk[au.sel];
    const as_cfg_t *c = &au.cfg[au.sel];
    uint32_t id = alt_qk(q, i);
    int32_t v;
    txt[0] = 0;
    *fill = -1;
    switch (id) {
    case QK_ENV: {
        uint32_t c = au.sel % AS_NCIRC, m = alt_emode[c] % AM_N;
        int32_t k = ALT_ENV_K[m][i & 3u];
        const char *n = 0;
        *name = ALT_ENV_NAME[m][i & 3u];
        if (m == AM_FREE) {
            v = clamp(t->p[P_ATK + i] - alt_applied[c][alt_slot(P_ATK + i)], 0, 127);
            *fill = v * 50 / 127;
            alt_num(txt, v, 0);
            return v;
        }
        if (k < 0)
            n = AS_CHORD_NAME[(v = c_chord_of(c))];
        if (n) {
            int32_t j = 0;
            while (n[j] && j < 7) {
                txt[j] = n[j];
                j++;
            }
            txt[j] = 0;
            return v + 300;
        }
        v = alt_envp[c][k];
        *fill = v * 50 / 127;
            alt_num(txt, v, 0);
        return v;
    }
    case QK_RHY: {
        uint32_t ty = c->type % AS_NTYPE;
        *name = ALT_RHY_NAME[ty][i & 3u];
        if (i == 0u)
            v = c->len;
        else if (ty == AS_T_TURING)
            v = i == 1u ? c->tlock * 100 / 127 : i == 2u ? c->tdens * 100 / 127 : c->trange;
        else if (i == 1u)
            v = c->hits;
        else
            v = i == 2u ? c->rot : c->prob * 100 / 127;
        alt_num(txt, v, (ty == AS_T_TURING && (i == 1u || i == 2u)) || (ty != AS_T_TURING && i == 3u));
        return v + (int32_t)ty * 1000;
    }
    case QK_LWAVE: case QK_LRATE: case QK_LDEPTH: case QK_LDEST: {
        char lb[8];
        const char *n;
        int32_t k = 0;
        *name = ALT_LFO_KNOB[id - QK_LWAVE];
        v = alt_lfo_val(0, id - QK_LWAVE);
        n = alt_lfo_text(id - QK_LWAVE, v, lb);
        while (n[k] && k < 7) {
            txt[k] = n[k];
            k++;
        }
        txt[k] = 0;
        return v;
    }
    case QK_SCALE:
        *name = "SCALE";
        v = alt_seq.scale % AS_NSCALE;
        { const char *n = AS_SCALE_NAME[v]; int32_t k = 0; while (n[k]) { txt[k] = n[k]; k++; } txt[k] = 0; }
        return v;
    case QK_ROOT: {
        const char *n;
        int32_t k = 0;
        *name = "ROOT";
        v = c->root;
        n = NOTE[v % 12];
        while (*n)
            txt[k++] = *n++;
        txt[k++] = (char)('0' + (v / 12 - 1) % 10);
        txt[k] = 0;
        return v;
    }
    case QK_CHORD:
        *name = "CHORD";
        v = c->chord % AS_NCHORD;
        { const char *n = AS_CHORD_NAME[v]; int32_t k = 0; while (n[k]) { txt[k] = n[k]; k++; } txt[k] = 0; }
        return v;
    case QK_DOSE:
        *name = "DOSE";
        v = c->dose;
        *fill = v * 50 / 127;
            alt_num(txt, v, 0);
        return v;
    case QK_BPM:
        *name = "TEMPO";                         /* (an hourglass: alter_look.c alt_dialk) */
        v = song.g[G_BPM];
        *fill = clamp(v - 40, 0, 200) / 4;
        alt_num(txt, v, 0);
        return v;
    case QK_SWING:
        *name = "SWING";
        v = song.g[G_SWING];
        *fill = clamp(v, 0, 100) / 2;
        alt_num(txt, v, 1);
        return v;
    default: {
        const param_desc_t *d = track_desc(t, id);
        int32_t span = d->max - d->min;
        v = t->p[id];
        if (alt_slot(id) >= 0)
            v -= alt_applied[au.sel][alt_slot(id)];   /* (the base, without the die's offset) */
        v = clamp(v, d->min, d->max);
        *name = d->label;
        if (d->fmt == F_ENUM && d->names) {
            const char *n = d->names[v - d->min];
            int32_t k = 0;
            while (n[k] && k < 7) {
                txt[k] = n[k];
                k++;
            }
            txt[k] = 0;
        } else {
            *fill = span > 0 ? (v - d->min) * 50 / span : 0;
            alt_num(txt, v, 0);
        }
        return v;
    }
    }
}

/* knob i of quick page q (its bar at fill of 50 px): where the LFOs and the matrix have it, on the same scale: ext 0
 * now, +1 / -1 at the LFOs' two ends (mod.c mod_show); -1 when nothing moves it */
static int32_t alt_live(uint32_t q, uint32_t i, int32_t fill, int ext)
{
    const track_t *t = &trk[au.sel];
    uint32_t id = alt_qk(q, i);
    const param_desc_t *d;
    int32_t g, off, span, v;
    int on;
    if (id >= P_COUNT)
        return -1;
    off = mod_show(t, id, &g, &on, ext);
    if (!on)
        return -1;
    d = track_desc(t, id);
    span = d->max - d->min;
    if (span <= 0)
        return -1;
    v = d->min + fill * span / 50;
    v = clamp(v + off, d->min, d->max);
    if (g < 32767)
        v = d->min + (int32_t)(((int64_t)(v - d->min) * g) >> 15);
    return (v - d->min) * 50 / span;
}

/* the moved knob's bar: lo .. hi the range its LFOs reach (a zone in the chosen circle's colour, fixed); *on 1 when
 * something moves it (else lo, hi -1) */
static void alt_live_bar(uint32_t q, uint32_t i, int32_t fill, int32_t *lo, int32_t *hi, int32_t *on)
{
    int32_t a = alt_live(q, i, fill, 1), b = alt_live(q, i, fill, -1);
    *lo = *hi = *on = -1;
    if (a < 0 || b < 0)
        return;
    *lo = a < b ? a : b;
    *hi = a < b ? b : a;
    *on = 1;
}

/* an LFO (the chosen circle's three, the GLOBAL) moves knob i of quick page q in a way its bar cannot show (PROB, the
 * envelope's settings, read at each hit): its colour's small wave goes by its name */
static int alt_knob_lfo(uint32_t q, uint32_t i)
{
    uint32_t id = alt_qk(q, i), m = 0, g, j, c = au.sel % AS_NCIRC;
    if (id == QK_RHY && i == 3u)
        m = 1u << GD_PROB;
    else if (id == QK_ENV)
        m = alt_env_gd(alt_emode[c], i);
    for (g = 1; m && g < GD_N; g++)
        if ((m >> g) & 1u) {
            if (alt_gl_has(g))
                return 1;
            for (j = 0; j < CL_N; j++)
                if (alt_cl_on(c, j) == g)
                    return 1;
        }
    return 0;
}

/* the small wave: 10 x 5 px at x, y */
static void alt_mini_wave(int32_t x, int32_t y, uint16_t col)
{
    int32_t k;
    for (k = 0; k < 10; k++)
        cv_rect(x + k, y + 2 - ((sine_i((uint32_t)k * 0x1999999Au) * 2) >> 15), 1, 2, col);
}

static void alt_locks_scene(void)
{
    uint32_t c = au.lockc - 1u, i, lk = alt_seq.c[c].locks;
    alt_card("LOCKS", "XI");
    alt_disc(226, 11, 5, ALT_COL[c]), alt_disc(226, 11, 3, ALT_BG), alt_disc(226, 11, 1, ALT_COL[c]);
    cv_text(8, 24, ALT_FS, "WHITE KEYS F3..E4   HOME: BACK", ALT_DIM);
    for (i = 0; i < 7u; i++) {
        int32_t y = 46 + (int32_t)i * 22, on = (lk & ALT_LK_BIT[i]) != 0;
        alt_diamond(17, y + 6, 5, 5, on ? ALT_COL[c] : alt_gold_line(40));
        if (!on)
            alt_diamond(17, y + 6, 3, 3, ALT_BG);
        cv_text(32, y, ALT_FS, ALT_LK_NAME[i], on ? ALT_INK : ALT_DIM);
        cv_text_r(228, y, ALT_FS, on ? "LOCKED" : "FREE", on ? ALT_COL[c] : ALT_DIM, cv_bg);
    }
}

static void alt_scenes_scene(void)
{
    static const char *const TAB[2] = {"SCENES", "PATTERNS"};
    uint32_t t = au.tab, r = au.row[t], i, top;
    char nb[12];
    alt_card("SAVE", "XX");
    alt_tabs(23, TAB, 2, t, ALT_COL[0]);
    if (au.msg && fm1_ms - au.msg_ms < 1500u)   /* SAVED, PROTECTED, EMPTY .. */
        cv_text(8, 214, ALT_FS, au.msg, ALT_COL[1]);
    else
        cv_text(8, 214, ALT_FS, "SELECT  SAVE: LOAD  HOLD: STORE", ALT_DIM);
    top = r < 2u ? 0u : r - 2u;                  /* five rows, the chosen one second or third */
    if (top > ALT_NLIST - 5u)
        top = ALT_NLIST - 5u;
    for (i = 0; i < 5u; i++) {
        uint32_t row = top + i;
        int32_t y = 48 + (int32_t)i * 30, on = row == r, user = row > ALT_NFACT;
        const char *nm = alt_row_name(t, row, nb);
        uint16_t rb = on ? alt_gold_line(16) : ALT_BG;
        if (on) {
            cv_rect(6, y - 3, 228, 29, rb);
            alt_diamond(12, y + 5, 3, 3, ALT_COL[0]);
        }
        if (row && !user) {                      /* the factory ones: a gold lock */
            int32_t lx = 220, ly = y + 2;
            alt_arc(lx + 3, ly + 3, 3, 0xC0000000u, 0x80000000u, ALT_GOLD, 1);
            cv_rect(lx - 1, ly + 3, 9, 7, on ? ALT_GOLD : alt_gold_line(60));
        }
        cv_text_on(22, y - 1, ALT_FS, nm, on ? ALT_INK : (nm[0] == 'E' ? ALT_GREY : ALT_DIM), rb);
        cv_text_in(22, y + 11, 192, ALT_FS, alt_row_about(t, row), on ? ALT_COL[t + 1u] : ALT_GREY, rb);
    }
}

/* the MTX tab's own words for the sources (mod.c MS_*) and what each one is */
static const char *const ALT_SRC_NAME[MS_N] = {"OFF", "LFO", "ENV", "VELO", "NOTE", "RANDOM", "MODWHL", "AFTTCH",
                                               "EXPR", "LFO S&H", "LFO SLW", "POS", "FORCE"};
static const char *const ALT_SRC_ABOUT[MS_N] = {
    "NO SOURCE: THIS SLOT RESTS",
    "THIS CIRCLE'S LFO (ITS TAB)",
    "THE NOTE'S ENVELOPE, UP THEN DOWN",
    "HOW HARD THE HIT IS (FORCE, DIE)",
    "THE NOTE: HIGHER NOTES, MORE",
    "A NEW RANDOM VALUE AT EACH HIT",
    "MIDI MOD WHEEL (CC 1)",
    "MIDI AFTERTOUCH",
    "MIDI EXPRESSION (CC 11)",
    "RANDOM STEPS, ONE PER LFO CYCLE",
    "RANDOM GLIDES, ONE PER LFO CYCLE",
    "A RAMP ACROSS THE CIRCLE'S CYCLE",
    "GHOST LOW, NORMAL HALF, ACCENT FULL",
};
_Static_assert(NELEM(ALT_SRC_NAME) == MS_N && NELEM(ALT_SRC_ABOUT) == MS_N, "a name and a line per source");

/* knob i (0 SRC 1 DST 2 AMT) of MTX row r: its value, and its words */
static int32_t alt_mtx_val(uint32_t r, uint32_t i) { return trk[au.sel % AS_NCIRC].p[P_M1SRC + 3u * (r % NMSLOT) + i]; }
static const char *alt_mtx_name(uint32_t i, int32_t v, char *buf)
{
    if (i == 0)
        return ALT_SRC_NAME[clamp(v, 0, MS_N - 1)];
    if (i == 1)
        return mod_dst_name(&trk[au.sel % AS_NCIRC], v);
    alt_num(buf, v, 0);
    return buf;
}

/* LFO tb's wave, rate and random seed */
static void alt_lfo_def(uint32_t tb, uint32_t *w, uint32_t *r, uint32_t *seed)
{
    uint32_t c = au.sel % AS_NCIRC;
    if (tb == ALT_TAB_GL) {
        *w = alt_gl.wave, *r = alt_gl.rate, *seed = 9u;
    } else {
        *w = alt_cl[c][tb % CL_N].wave, *r = alt_cl[c][tb % CL_N].rate, *seed = c * 3u + tb % CL_N;
    }
}

/* LFO tb acts (a DEST and a DEPTH, and not taken by the GLOBAL) */
static int alt_lfo_acts(uint32_t tb)
{
    return tb == ALT_TAB_GL ? alt_gl.dst[0] && alt_gl.amt[0] : alt_cl_on(au.sel % AS_NCIRC, tb % CL_N) != 0;
}

#define ALT_WAVE_W 224
#define ALT_WAVE_X 8
/* what the LFO page's picture depends on */
static uint32_t alt_mat_sig(void)
{
    uint32_t tb = au.mtab, i, sg = tb * 7u + au.mrow[0] * 11u + au.sel * 13u + alt_gl_take() * 3u;
    if (tb == ALT_TAB_MTX) {
        for (i = 0; i < 3u * NMSLOT; i++)
            sg = sg * 31u + (uint32_t)trk[au.sel % AS_NCIRC].p[P_M1SRC + i];
    } else {
        uint32_t px = alt_lfo_p * 16u / ALT_WAVE_W;   /* (the wave scrolls a pixel at a time) */
        for (i = 0; i < 4u; i++)
            sg = sg * 31u + (uint32_t)alt_lfo_val(tb, i);
        sg = sg * 31u + (px ? alt_gl_n / px : 0u) + (uint32_t)alt_lfo_acts(tb) * 5u;
    }
    if (au.dd && fm1_ms - au.dd_ms < ALT_DD_MS)
        sg = sg * 31u + au.dd * 977u;
    return sg;
}

/* a list dropped down at x, y (its rows 15 px): the values around cur, cur lit */
static void alt_dropdown(int32_t lx, int32_t ly, int32_t cur, int32_t n, uint16_t col, uint32_t tb, uint32_t ki)
{
    char b[8];
    int32_t k;
    cv_rect(lx - 2, ly - 2, 84, 5 * 15 + 4, col);
    cv_rect(lx - 1, ly - 1, 82, 5 * 15 + 2, ALT_BG);
    for (k = -2; k <= 2; k++) {
        int32_t v = cur + k, yy = ly + (k + 2) * 15;
        if (v < 0 || v >= n)
            continue;
        if (!k)
            cv_rect(lx, yy, 80, 15, ALT_DARK);
        cv_text(lx + 4, yy + 1, ALT_FS, tb == ALT_TAB_MTX ? alt_mtx_name(ki, v, b) : alt_lfo_text(3, v, b),
                k ? ALT_DIM : col);
    }
}

/* an LFO tab: its title, one bar of time with its beats and steps, the wave scrolling through it (now at the right
 * edge: it comes in from there), its four settings (KNOB 1-4) */
static void alt_lfo_tab(uint32_t tb, uint16_t col)
{
    char b[8];
    uint32_t i, w, r, seed, p = alt_lfo_p ? alt_lfo_p : 1u, now = alt_gl_n, bar = 16u * p;
    int on = alt_lfo_acts(tb);
    int32_t x, py = 0, y0 = 50, h = 112, mid = y0 + h / 2;
    uint16_t wc = on ? col : ALT_DIM;
    alt_lfo_def(tb, &w, &r, &seed);
    /* the grid: a bar, steps faint, beats brighter, the bar's ends; fixed to the beat as time scrolls past */
    for (i = 0; i <= 16u; i++) {
        uint32_t back = (now % p) + (16u - i) * p;     /* samples before now of the step line i */
        int32_t xx = ALT_WAVE_X + ALT_WAVE_W - (int32_t)((uint64_t)back * ALT_WAVE_W / bar), beat = ((now / p) + i) % 4u == 0u;
        if (xx >= ALT_WAVE_X && xx <= ALT_WAVE_X + ALT_WAVE_W) {
            cv_rect(xx, y0, 1, h, beat ? alt_gold_line(45) : alt_gold_line(16));
            if (beat)                            /* a beat: a gold diamond over its line */
                alt_diamond(xx, y0 - 4, 3, 3, ALT_GOLD);
        }
    }
    for (x = 0; x < ALT_WAVE_W; x += 3)
        cv_pset(ALT_WAVE_X + x, mid, alt_gold_line(35));
    {                                            /* the wave over the last bar, now at the right: as DEPTH scales it
                                                 * (smaller, upside down below 0), over its full shape faint */
        int32_t dep = alt_lfo_val(tb, 2), ghost[ALT_WAVE_W];
        for (x = 0; x < ALT_WAVE_W; x++) {
            uint32_t back = (uint32_t)((uint64_t)(ALT_WAVE_W - 1 - x) * bar / ALT_WAVE_W);
            ghost[x] = alt_lfo_at(w, r, seed, now - back, p, 0);
            if (!alt_wave_uni(w))
                ghost[x] = clamp(ghost[x], -32767, 32767);
        }
        for (x = 0; x < ALT_WAVE_W; x++)         /* the full shape, faint */
            if (!(x & 1))
                cv_pset(ALT_WAVE_X + x, mid - ((ghost[x] * (h / 2 - 3)) >> 15), ALT_DIM);
        for (x = 0; x < ALT_WAVE_W; x++) {       /* the wave as it moves the sound */
            int32_t y = mid - (((ghost[x] * dep / 64) * (h / 2 - 3)) >> 15);
            if (x && (y - py > 1 || py - y > 1))
                cv_rect(ALT_WAVE_X + x, y < py ? y : py, 2, (y < py ? py - y : y - py) + 1, wc);
            else
                cv_rect(ALT_WAVE_X + x, y - 1, 1, 3, wc);
            py = y;
        }
    }
    cv_rect(ALT_WAVE_X + ALT_WAVE_W, y0, 2, h, wc);   /* now */
    if (tb < ALT_TAB_GL && alt_lfo_val(tb, 3) && alt_lfo_val(tb, 2) && !on)
        cv_text(8, 176, ALT_FS, "THE GLOBAL LEADS ON THIS DEST", ALT_DIM);
    else if (!on)
        cv_text(8, 176, ALT_FS, "SET A DEST AND A DEPTH", ALT_DIM);
    cv_rect(8, 196, 224, 1, alt_gold_line(30));
    alt_diamond(120, 196, 2, 2, ALT_GOLD);
    for (i = 0; i < 4u; i++) {                   /* the four settings, as dials (DEPTH: its middle at the top) */
        int32_t v = alt_lfo_val(tb, i);
        alt_dialk(8 + (int32_t)i * 57, 200, ALT_LFO_KNOB[i], alt_lfo_text(i, v, b), i == 2u ? (v + 64) * 50 / 127 : -1,
                  -1, -1, !(i == 3u && !v), col, 0);
    }
    if (au.dd == 4u && fm1_ms - au.dd_ms < ALT_DD_MS)
        alt_dropdown(150, 62, alt_lfo_val(tb, 3), GD_N, col, tb, 3);
}

/* the MTX tab: the chosen circle's four slots, under its own title */
static void alt_mtx_tab(uint16_t col)
{
    char b0[8], b1[8], b2[8];
    uint32_t row = au.mrow[0] % NMSLOT, r, take = alt_gl_take();
    for (r = 0; r < NMSLOT; r++) {
        int32_t y = 48 + (int32_t)r * 38, on = r == row;
        int32_t v0 = alt_mtx_val(r, 0), v1 = alt_mtx_val(r, 1), v2 = alt_mtx_val(r, 2);
        int led = ((take >> v1) & 1u) && (v0 == MS_LFO || v0 == MS_SH || v0 == MS_SLEW), act = v0 && v1 && v2;
        int32_t bw = (v2 < 0 ? -v2 : v2) * 24 / 64;
        uint16_t rb = on ? alt_gold_line(16) : cv_bg, vc = led ? ALT_GREY : act ? col : ALT_DIM;
        if (on)
            cv_rect(6, y - 2, 228, 34, rb);
        b0[0] = (char)('1' + r);
        b0[1] = 0;
        cv_text_on(8, y + 3, ALT_FS, b0, ALT_GREY, rb);
        cv_text_on(24, y + 3, ALT_FS, alt_mtx_name(0, v0, b1), on ? ALT_INK : ALT_DIM, rb);
        cv_text_on(88, y + 3, ALT_FS, ">", ALT_GREY, rb);
        cv_text_on(100, y + 3, ALT_FS, alt_mtx_name(1, v1, b2), on ? ALT_INK : ALT_DIM, rb);
        cv_rect(168, y + 9, 1, 7, ALT_GREY);     /* the amount: a bar from the middle, and its number */
        if (v2 > 0)
            cv_rect(169, y + 10, bw, 5, vc);
        else if (v2 < 0)
            cv_rect(168 - bw, y + 10, bw, 5, vc);
        alt_num(b0, v2, 0);
        cv_text_r(234, y + 3, ALT_FS, b0, vc, rb);
        if (led)
            cv_text_on(24, y + 17, ALT_FS, "GLOBAL LEADS: THIS SLOT RESTS", ALT_GREY, rb);
        else if (on)
            cv_text_on(24, y + 17, ALT_FS, ALT_SRC_ABOUT[clamp(v0, 0, MS_N - 1)], ALT_GREY, rb);
    }
    cv_text(8, 216, ALT_FS, "SELECT SLOT  K1 SRC  K2 DEST  K3 AMT", ALT_DIM);
    if (au.dd && au.dd <= 2u && fm1_ms - au.dd_ms < ALT_DD_MS) {   /* the list, dropped down over its column */
        uint32_t ki = au.dd - 1u;
        int32_t ly = 48 + (int32_t)row * 38 + 6 - 2 * 15;
        ly = clamp(ly, 26, 206 - 5 * 15);
        alt_dropdown(ki == 0 ? 20 : 96, ly, alt_mtx_val(row, ki), ki == 0 ? MS_N : MD_N, col, ALT_TAB_MTX, ki);
    }
}

/* the SEQ menu: the chosen circle's engine, its steps with their lane as bars (XO-like), the engine's four knobs */
static void alt_seq_scene(void)
{
    uint32_t c = au.sel % AS_NCIRC, i, k, ln = au.lane % LN_N;
    const as_circ_t *sc = &alt_seq.c[c];
    const as_cfg_t *cf = &au.cfg[c];
    uint32_t len = cf->len ? cf->len : 1u, n = len < 16u ? len : 16u, now = (sc->pos + sc->cfg.len - 1u) % sc->cfg.len;
    uint16_t col = ALT_COL[c];
    int32_t x = 8, w = 224 / 16, y0 = 50, h = 104, mid = y0 + h / 2;
    alt_card("SEQ", "X");                        /* the head: the card, the circle and its engine (SEQ held +
                                                  * ALGORITHM) at the right */
    alt_chip();                                  /* (the sound at the right, the engine of the rhythm at the left) */
    alt_eng_tag("X", AS_TYPE_NAME[cf->type % AS_NTYPE], col);
    alt_tabs(23, ALT_LANE_TAB, LN_N, ln, col);  /* the lanes (ALGORITHM) */
    if (ln == LN_PITCH)
        cv_rect(x, mid, 224, 1, ALT_DARK);
    for (k = 0; k < 16u; k++) {                  /* the steps: a bar each, their lane's value */
        int32_t bx = x + (int32_t)k * w, v = alt_lane_val(c, ln, k), on = k < n && ((sc->pat >> k) & 1u);
        int held = (au.step_down >> k) & 1u, here = song.playing && k == now % 16u && k < n;
        uint16_t bc = k >= n ? ALT_DARK : on ? col : alt_gold_line(45);
        if (k >= n) {
            cv_rect(bx + 2, y0 + h - 2, w - 4, 2, ALT_DARK);
            continue;
        }
        if ((sc->rmask[ln] >> k) & 1u) {         /* a RANGE: a band from its bottom to its top, the bar's colour faded */
            int32_t lo, hi, st, a2 = sc->rlo[ln][k], b2 = sc->rhi[ln][k], y1, y2;
            alt_lane_lim(ln, &lo, &hi, &st);
            if (a2 > b2) {
                int32_t t = a2;
                a2 = b2;
                b2 = t;
            }
            y1 = y0 + h - 2 - (b2 - lo) * (h - 4) / (hi - lo);
            y2 = y0 + h - 2 - (a2 - lo) * (h - 4) / (hi - lo);
            cv_rect(bx + 2, y1, w - 4, y2 - y1 + 2, alt_fade(bc, 45));
            cv_rect(bx + 2, y1, w - 4, 2, bc);
            cv_rect(bx + 2, y2, w - 4, 2, bc);
        } else if (ln == LN_PITCH) {
            int32_t bh = (v < 0 ? -v : v) * (h / 2 - 2) / 7;
            cv_rect(bx + 2, v >= 0 ? mid - bh : mid, w - 4, bh ? bh : 1, bc);
        } else if (ln == LN_RATCH) {             /* x1 .. x4: one block per hit in the step */
            int32_t j2, bhh = (h - 6) / 4;
            for (j2 = 0; j2 <= v; j2++)
                cv_rect(bx + 2, y0 + h - (j2 + 1) * (bhh + 2), w - 4, bhh, bc);
        } else {                                 /* a pillar with a diamond on top */
            int32_t bh = v * (h - 10) / 127;
            cv_rect(bx + 4, y0 + h - bh, w - 8, bh ? bh : 1, bc);
            alt_diamond(bx + w / 2, y0 + h - bh - 4, 3, 3, bc);
        }
        if (held)                                /* held: the column lit, a gold frame */
            cv_frame(bx, y0 - 2, w, h + 4, ALT_GOLD);
        if (here)                                /* the beats under them, the playing step a star */
            alt_star(bx + w / 2, y0 + h + 8, 4, ALT_INK);
        else if (k % 4u == 0u)
            alt_diamond(bx + w / 2, y0 + h + 8, 2, 2, ALT_GOLD);
        else
            cv_rect(bx + w / 2 - 1, y0 + h + 7, 2, 2, ALT_DIM);
    }
    {
        char b[8];
        int32_t v = au.step_down ? alt_lane_val(c, ln, (uint32_t)__builtin_ctz(au.step_down)) : 0;
        if (au.step_down) {                      /* the held step's value, or its RANGE (bottom .. top) */
            uint32_t k0 = (uint32_t)__builtin_ctz(au.step_down), rg = (sc->rmask[ln] >> k0) & 1u, j2;
            int32_t vv[2], x2 = 8;
            vv[0] = rg ? sc->rlo[ln][k0] : v;
            vv[1] = rg ? sc->rhi[ln][k0] : v;
            x2 = cv_text(x2, 176, ALT_FS, ALT_LANE_NAME[ln], ALT_DIM) + 6;   /* (cv_text: the x after the text) */
            for (j2 = 0; j2 < (rg ? 2u : 1u); j2++) {
                int32_t w2 = vv[j2];
                if (ln == LN_PITCH) {
                    int32_t q = w2 < 0 ? -w2 : w2;
                    b[0] = w2 < 0 ? '-' : '+';
                    b[1] = (char)('0' + q);
                    b[2] = 0;
                } else if (ln == LN_RATCH) {
                    b[0] = 'X';
                    b[1] = (char)('1' + w2);
                    b[2] = 0;
                } else
                    alt_num(b, w2 * 100 / 127, 1);
                if (j2)
                    x2 = cv_text(x2, 174, ALT_FV, "..", ALT_DIM) + 2;
                x2 = cv_text(x2, 174, ALT_FV, b, ALT_INK) + 2;
            }
            if (rg)
                cv_text(x2 + 6, 176, ALT_FS, "RANDOM", ALT_COL[c]);
        }
    }
    alt_foot(QP_SEQ);                            /* the engine's four knobs */
}

/* what the SEQ menu's picture depends on */
static uint32_t alt_seq_sig(void)
{
    uint32_t c = au.sel % AS_NCIRC, k, sg = c * 7u + au.lane * 11u + au.step_down * 13u + au.cfg[c].type * 17u;
    const as_circ_t *sc = &alt_seq.c[c];
    sg = sg * 31u + sc->pat + sc->pos * 977u + au.cfg[c].len * 3u + (uint32_t)song.playing;
    for (k = 0; k < 16u; k++)
        sg = sg * 31u + (uint32_t)alt_lane_val(c, au.lane, k);
    for (k = 0; k < 16u; k++)
        sg = sg * 31u + (uint32_t)(uint8_t)sc->rlo[au.lane % LN_N][k] * 7u + (uint32_t)(uint8_t)sc->rhi[au.lane % LN_N][k];
    sg = sg * 31u + sc->rmask[au.lane % LN_N];
    for (k = 0; k < 4u; k++) {
        const char *nm;
        char tx[12];
        int32_t f;
        sg = sg * 31u + (uint32_t)alt_kv(QP_SEQ, k, &nm, tx, &f);
    }
    return sg;
}

static void alt_matrix_scene(void)
{
    static const char *const TAB[5] = {"LFO 1", "LFO 2", "LFO 3", "GLOBAL", "MTX"};
    uint32_t tb = au.mtab;
    uint16_t sc = ALT_COL[au.sel % AS_NCIRC], col = tb == ALT_TAB_GL ? ALT_INK : sc;
    alt_menu_head("LFO", "XVIII");
    alt_tabs(23, TAB, 5, tb, tb == ALT_TAB_GL ? ALT_GOLD : sc);
    if (tb == ALT_TAB_MTX)
        alt_mtx_tab(col);
    else
        alt_lfo_tab(tb, col);
}

/* the foot: the page's four knobs as gold dials (the circles, the ENV and SCL menus) */
static void alt_foot(uint32_t q)
{
    uint32_t i;
    cv_rect(8, 196, 224, 1, alt_gold_line(30));
    alt_diamond(120, 196, 2, 2, ALT_GOLD);
    for (i = 0; i < 4u; i++) {
        int32_t x = 8 + (int32_t)i * 57, fill, lo = -1, hi = -1, pct = -1;
        const char *name;
        char txt[12];
        alt_kv(q, i, &name, txt, &fill);
        if (fill >= 0)
            alt_live_bar(q, i, fill, &lo, &hi, &pct);
        alt_dialk(x, 200, name, txt, fill, pct >= 0 ? lo : -1, hi, 1, ALT_COL[au.sel % AS_NCIRC],
                  pct < 0 && alt_knob_lfo(q, i));
    }
}


/* ---- the ENV, EDIT and SCL menus (their button held 0.5 s) ---- */
/* the head: the chosen circle's point, the menu's name, a word in its colour */
static void alt_menu_head(const char *name, const char *num)
{
    alt_card(name, num);
    alt_chip();
    alt_eng_tag(num, ENGINES[eng_idx(trk[au.sel % AS_NCIRC].eng_req)]->name, ALT_COL[au.sel % AS_NCIRC]);
}

/* ENV (XIII): the chosen circle's envelope as its mode shapes it (the ENV page's four knobs, the same names), a
 * picture, not a measure: the times drawn longer as the knobs go up, the area under it engraved. The modes as tabs
 * (ALGORITHM). PERC: PUNCH as a falling line; STAB / DRONE: PLUCK / OPEN (the filter the envelope opens) dotted */
static void alt_env_scene(void)
{
    uint32_t c = au.sel % AS_NCIRC, m = alt_emode[c] % AM_N, i;
    const track_t *t = &trk[c];
    uint16_t col = ALT_COL[c];
    int32_t v[4], a, d, h, sus, r, x0 = 10, y0 = 184, top = 54, H = y0 - top, px, x, prev = y0, filt = -1;
    for (i = 0; i < 4u; i++) {
        int32_t k = ALT_ENV_K[m][i];
        v[i] = m == AM_FREE ? clamp(t->p[P_ATK + i] - alt_applied[c][alt_slot(P_ATK + i)], 0, 127) :
               k >= 0 ? (int32_t)alt_envp[c][k] : 0;
    }
    alt_menu_head("ENV", "XIII");
    alt_tabs(23, ALT_MODE_NAME, AM_N, m, col);
    switch (m) {                                 /* segment widths (px) and the sustain level (0..H) */
    case AM_PERC:  a = 2 + v[0] * 20 / 127; h = v[1] * 60 / 127; d = 8 + v[2] * 120 / 127; sus = 0; r = 0; break;
    case AM_STAB:  a = 2; d = 8 + v[0] * 60 / 127; h = 30; sus = v[1] * H / 127; r = 6 + v[0] * 50 / 127; filt = v[2]; break;
    case AM_DRONE: a = 20 + v[0] * 70 / 127; d = 16; h = 40; sus = 20 + v[1] * (H - 20) / 127; r = 24 + v[2] * 60 / 127; filt = v[3]; break;
    default:       a = 2 + v[0] * 50 / 127; d = 4 + v[1] * 50 / 127; h = 40; sus = v[2] * H / 127; r = 4 + v[3] * 60 / 127; break;
    }
    if (m == AM_PERC) {                          /* (hold: up there, then the decay down to nothing) */
        sus = H;
    }
    cv_rect(x0, y0 + 1, 220, 1, alt_gold_line(60));   /* the ground */
    px = a + d + h + r;
    if (m == AM_PERC)
        px = a + h + d;
    if (m == AM_PERC || m == AM_FREE) {          /* the segments' ends, dotted */
        int32_t e1 = x0 + a, e2 = x0 + a + (m == AM_PERC ? h : d), yy;
        for (yy = top; yy < y0; yy += 4) {
            cv_pset(e1, yy, alt_gold_line(40));
            cv_pset(e2, yy, alt_gold_line(40));
        }
        alt_diamond(e1, top - 4, 3, 3, ALT_GOLD);
        alt_diamond(e2, top - 4, 3, 3, ALT_GOLD);
    }
    for (x = 0; x <= px && x0 + x < 232; x++) {  /* the curve: rises straight, falls as a square law (faster first) */
        int32_t lv, y, yy;
        if (x < a)
            lv = H * x / (a ? a : 1);
        else if (m == AM_PERC) {
            int32_t u = x - a - h;
            if (u < 0)
                lv = H;
            else {
                int32_t w = d - u < 0 ? 0 : d - u;
                lv = H * w / d * w / d;
            }
        } else if (x < a + d) {
            int32_t w = a + d - x;
            lv = sus + (H - sus) * w / d * w / d;
        } else if (x < a + d + h)
            lv = sus;
        else {
            int32_t w = px - x;
            lv = sus * w / r * w / r;
        }
        y = y0 - lv;
        for (yy = y + 2; yy < y0; yy++)          /* (the area under it, engraved: diagonal hatching) */
            if (((x + yy) & 3) == 0)
                cv_pset(x0 + x, yy, alt_fade(col, 55));
        cv_rect(x0 + x, y < prev ? y : prev, 2, (y < prev ? prev - y : y - prev) + 2, col);
        prev = y;
    }
    if (filt >= 0)                               /* PLUCK / OPEN: the filter the envelope opens, dotted */
        for (x = 0; x < px && x0 + x < 232; x += 4) {
            int32_t w = px - x, lv = H * filt / 127 * w / px * w / px;
            cv_rect(x0 + x, y0 - lv, 2, 2, ALT_INK);
        }
    if (m == AM_PERC && v[3])                    /* PUNCH: the pitch dives at the hit */
        for (x = 0; x < 40; x += 3)
            cv_rect(x0 + 4 + x, top + 4 + x * x * v[3] / 127 / 40, 2, 2, ALT_INK);
    if (m == AM_STAB) {                          /* CHORD: its name */
        char b[8];
        const char *n = AS_CHORD_NAME[c_chord_of(c)];
        for (i = 0; n[i] && i < 7u; i++)
            b[i] = n[i];
        b[i] = 0;
        cv_text_r(230, 46, ALT_FS, b, ALT_DIM, ALT_BG);
    }
    alt_foot(QP_ENV);
}

/* SCL (XVII, the Star): the scale list (KNOB 1 picks, as on the quick page) and the wheel of the twelve notes: the
 * scale's notes from the chosen circle's root joined into its sigil, the root in full colour, the chord on the root
 * inside it, dashed */
static void alt_scl_scene(void)
{
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    uint32_t c = au.sel % AS_NCIRC, sc = alt_seq.scale % AS_NSCALE, root = au.cfg[c].root % 12u, i, k, ch;
    uint16_t col = ALT_COL[c];
    uint16_t lit = 0, chm = 0;
    int32_t wx = 168, wy = 112, px[12], py[12];
    for (k = 0; AS_SCALES[sc][k] >= 0; k++)
        lit |= (uint16_t)(1u << ((root + (uint32_t)AS_SCALES[sc][k]) % 12u));
    ch = au.cfg[c].chord % AS_NCHORD;            /* the chord on the root (alter_seq.c as_step) */
    if (ch) {
        static const int8_t T2[AS_NCHORD][3] = {{0, 0, 0}, {0, 7, 14}, {0, 14, 24}, {0, 24, 0}, {0, 7, 21}, {7, 21, 28}};
        uint32_t r0 = au.cfg[c].root;
        for (k = 0; k < 3u; k++) {
            int32_t t2 = T2[ch][k], n;
            if (k && !t2)
                continue;
            n = ch == AS_CH_5TH || ch == AS_CH_OCT ? (int32_t)r0 + t2 / 2 :
                t2 ? (int32_t)as_cnote(&alt_seq, r0, 0, t2) : (int32_t)r0;
            chm |= (uint16_t)(1u << ((uint32_t)n % 12u));
        }
    }
    alt_menu_head("SCL", "XVII");
    for (i = 0; i < AS_NSCALE; i++) {            /* the list */
        int32_t y = 28 + (int32_t)i * 22;
        if (i == sc) {
            cv_rect(8, y - 3, 84, 19, alt_fade(col, 22));
            alt_diamond(15, y + 6, 3, 3, col);
        }
        cv_text(23, y, ALT_FS, AS_SCALE_NAME[i], i == sc ? ALT_INK : ALT_DIM);
    }
    alt_aaring16(wx * 16, wy * 16, 46 * 16, alt_gold_line(60));   /* the wheel */
    alt_aaring16(wx * 16, wy * 16, 50 * 16, alt_gold_line(25));
    for (k = 0; k < 12u; k++)
        alt_pt(wx, wy, k * (0xFFFFFFFFu / 12u), 46, &px[k], &py[k]);
    {                                            /* the scale's sigil, then the chord's, dashed */
        int32_t first = -1, prev = -1;
        for (k = 0; k < 12u; k++) {
            uint32_t n = (root + k) % 12u;
            if (!((lit >> n) & 1u))
                continue;
            if (prev >= 0)
                alt_aaline(px[prev], py[prev], px[n], py[n], col);
            else
                first = (int32_t)n;
            prev = (int32_t)n;
        }
        if (prev >= 0 && first >= 0 && prev != first)
            alt_aaline(px[prev], py[prev], px[first], py[first], col);
        first = prev = -1;
        for (k = 0; k < 12u; k++) {
            uint32_t n = (root + k) % 12u;
            if (!((chm >> n) & 1u))
                continue;
            if (prev >= 0) {
                int32_t j;
                for (j = 0; j < 16; j += 2)       /* (dashed) */
                    alt_aaline(px[prev] + (px[n] - px[prev]) * j / 16, py[prev] + (py[n] - py[prev]) * j / 16,
                            px[prev] + (px[n] - px[prev]) * (j + 1) / 16, py[prev] + (py[n] - py[prev]) * (j + 1) / 16,
                            ALT_INK);
            } else
                first = (int32_t)n;
            prev = (int32_t)n;
        }
        if (prev >= 0 && first >= 0 && prev != first) {
            int32_t j;
            for (j = 0; j < 16; j += 2)
                alt_aaline(px[prev] + (px[first] - px[prev]) * j / 16, py[prev] + (py[first] - py[prev]) * j / 16,
                        px[prev] + (px[first] - px[prev]) * (j + 1) / 16, py[prev] + (py[first] - py[prev]) * (j + 1) / 16,
                        ALT_INK);
        }
    }
    for (k = 0; k < 12u; k++) {                  /* the notes: the scale's in gold, the root in its colour */
        int32_t lx, ly, on = (lit >> k) & 1u;
        if (k == root)
            alt_diamond(px[k], py[k], 5, 5, col);
        else if (on)
            alt_diamond(px[k], py[k], 3, 3, ALT_GOLD);
        else
            cv_rect(px[k] - 1, py[k] - 1, 2, 2, ALT_DIM);
        alt_pt(wx, wy, k * (0xFFFFFFFFu / 12u), 60, &lx, &ly);
        cv_text(lx - text_w(ALT_FS, NN[k]) / 2, ly - 6, ALT_FS, NN[k], k == root ? col : on ? ALT_INK : ALT_DIM);
    }
    alt_star(wx, wy, 6, ALT_GOLD);
    alt_foot(QP_SCL);
}

/* SETTINGS (I, the Magician; EDIT held): rows, SELECT picks one, KNOB 1 or ALGORITHM changes it. THEME: NIGHT / DAY,
 * its emblem (the moon, the sun) under it */
enum { SET_THEME, SET_N };
static const char *const SET_NAME[SET_N] = {"THEME"};
#define ALT_PAL_NIGHT UI_GREY_INDEX
#define ALT_PAL_DAY 6u                            /* PAPER (build/gen/ui_palettes.h) */
static const char *alt_set_text(uint32_t r)
{
    if (r == SET_THEME)
        return ALT_DAY ? "DAY" : "NIGHT";
    return "";
}
static void alt_set_turn(uint32_t r, int32_t s)
{
    if (r == SET_THEME) {
        uint32_t day = s > 0;
        if (day == (uint32_t)ALT_DAY)
            return;
        palette_choose(day ? ALT_PAL_DAY : ALT_PAL_NIGHT);
        settings_save();
        au.drawn = 0;
    }
}
static void alt_set_input(void)
{
    int32_t s;
    uint32_t i;
    if ((s = panel_enc(EN_SELECT)) != 0)
        au.set_row = (uint8_t)clamp((int32_t)au.set_row + s, 0, SET_N - 1);
    if ((s = panel_enc(EN_ALGO)) != 0)
        alt_set_turn(au.set_row, s);
    for (i = 0; i < 4u; i++)
        if ((s = panel_enc(EN_K1 + i)) != 0 && i == 0u)
            alt_set_turn(au.set_row, s);
    (void)panel_enc(EN_PRESET);
    alt_hold = 0;
}
static void alt_set_scene(void)
{
    uint32_t r, k;
    uint16_t col = ALT_COL[au.sel % AS_NCIRC], hl = alt_gold_line(16), moon = ALT_DAY ? ALT_GREY : ALT_INK,
             sun = ALT_DAY ? ALT_GOLD : ALT_GREY;
    alt_card("SETTINGS", "I");
    for (r = 0; r < SET_N; r++) {
        int32_t y = 32 + (int32_t)r * 24;
        if (r == au.set_row) {
            cv_rect(8, y - 3, 224, 21, hl);
            alt_diamond(15, y + 7, 3, 3, col);
        }
        cv_text_on(24, y, ALT_FM, SET_NAME[r], ALT_INK, r == au.set_row ? hl : ALT_BG);
        cv_text_r(226, y, ALT_FM, alt_set_text(r), r == au.set_row ? col : ALT_DIM, r == au.set_row ? hl : ALT_BG);
    }
    alt_aaring16(120 * 16, 122 * 16, 52 * 16, alt_gold_line(60));   /* the emblem: the moon and the sun, the side in use lit */
    alt_aaring16(120 * 16, 122 * 16, 47 * 16, alt_gold_line(25));
    cv_rect(120, 72, 1, 100, alt_gold_line(45));
    alt_disc(97, 122, 15, moon);
    alt_disc(103, 117, 13, ALT_BG);
    alt_disc(143, 122, 9, sun);
    for (k = 0; k < 8u; k++) {
        int32_t xa, ya, xb, yb;
        alt_pt(143, 122, k * 0x20000000u, 12, &xa, &ya);
        alt_pt(143, 122, k * 0x20000000u, 17, &xb, &yb);
        alt_aaline(xa, ya, xb, yb, sun);
    }
    cv_text(97 - text_w(ALT_FS, "NIGHT") / 2, 180, ALT_FS, "NIGHT", ALT_DAY ? ALT_DIM : ALT_INK);
    cv_text(143 - text_w(ALT_FS, "DAY") / 2, 180, ALT_FS, "DAY", ALT_DAY ? ALT_INK : ALT_DIM);
    cv_rect(8, 196, 224, 1, alt_gold_line(30));
    cv_frame(8, 208, text_w(ALT_FV, "KNOB 1") + 8, 18, ALT_GOLD);
    cv_text(12, 208, ALT_FV, "KNOB 1", ALT_INK);
    cv_text(26 + text_w(ALT_FV, "KNOB 1"), 211, ALT_FS, "CHANGE", ALT_INK);
}

#include "alter_fxpage.c"


/* how bright circle i's piece of the middle is: 4 just hit .. 0 at rest (about a quarter second) */
static uint32_t alt_glow(uint32_t i)
{
    uint32_t t = fm1_ms - au.flash_ms[i % AS_NCIRC];
    return t < 50u ? 4u : t < 100u ? 3u : t < 160u ? 2u : t < 240u ? 1u : 0u;
}
/* a filled triangle (screen coordinates, 1/16 px), each row from edge to edge */
static void alt_tri(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t c)
{
    int32_t t, y;
#define ALT_SWAP(a, b) (t = a, a = b, b = t)
    if (y1 < y0) { ALT_SWAP(x0, x1); ALT_SWAP(y0, y1); }
    if (y2 < y0) { ALT_SWAP(x0, x2); ALT_SWAP(y0, y2); }
    if (y2 < y1) { ALT_SWAP(x1, x2); ALT_SWAP(y1, y2); }
#undef ALT_SWAP
    if (y2 == y0)
        return;
    for (y = (y0 + 15) / 16; y * 16 <= y2; y++) {
        int32_t yy = y * 16, xa, xb, xl, xr;
        xa = x0 + (x2 - x0) * (yy - y0) / (y2 - y0);                     /* the long edge */
        if (yy < y1)
            xb = y1 == y0 ? x1 : x0 + (x1 - x0) * (yy - y0) / (y1 - y0);
        else
            xb = y2 == y1 ? x1 : x1 + (x2 - x1) * (yy - y1) / (y2 - y1);
        xl = (xa < xb ? xa : xb) + 8;
        xr = (xa < xb ? xb : xa) + 8;
        if (xr / 16 > xl / 16)
            cv_rect(xl / 16, y, xr / 16 - xl / 16, 1, c);
    }
}
/* the middle (Romain, 01/11): a triangle cut from its centre into three, one piece per circle in its colour, faint at
 * rest; a hit of that circle lights its piece a little and pushes it out 2 px at most, both fading over a quarter
 * second (Romain: smaller, a discreet pulse). Muted: grey. Each piece set in gold (the tarot look); the chosen
 * circle's piece carries a dot */
static void alt_prism(void)
{
    static const int32_t V[3][2] = {{0, -22 * 16}, {-19 * 16, 11 * 16}, {19 * 16, 11 * 16}};   /* top, left, right */
    static const uint8_t E[AS_NCIRC][2] = {{0, 1}, {0, 2}, {1, 2}};   /* circle i's piece: the centre + these two */
    int32_t cx = ALT_CX * 16, cy = ALT_CY * 16;
    uint32_t i;
    for (i = 0; i < AS_NCIRC; i++) {
        const int32_t *a = V[E[i][0]], *b = V[E[i][1]];
        int32_t mx = (a[0] + b[0]) / 2, my = (a[1] + b[1]) / 2;      /* out: towards its outer edge's middle */
        int32_t gx = (a[0] + b[0]) / 3, gy = (a[1] + b[1]) / 3;      /* its own centre (the triangle's is 0, 0) */
        uint32_t lv = alt_glow(i);
        int32_t len = mx * mx + my * my, push = (int32_t)lv * 8, ox, oy, k;
        int32_t px[3], py[3];
        uint16_t col = au.cfg[i].mute ? ALT_GREY : ALT_COL[i];
        {                                        /* (the unit outward step: |m| is 11 px for every piece) */
            (void)len;
            ox = mx * push / (11 * 16);
            oy = my * push / (11 * 16);
        }
        px[0] = 0; py[0] = 0;
        px[1] = a[0]; py[1] = a[1];
        px[2] = b[0]; py[2] = b[1];
        for (k = 0; k < 3; k++) {                /* drawn a little smaller than its place: a gap between the pieces */
            px[k] = gx + (px[k] - gx) * 7 / 8 + cx + ox;
            py[k] = gy + (py[k] - gy) * 7 / 8 + cy + oy;
        }
        alt_tri(px[0], py[0], px[1], py[1], px[2], py[2], alt_fade(col, au.cfg[i].mute ? 45 : 32 + (int32_t)lv * 11));
        for (k = 0; k < 3; k++) {                /* set in gold */
            int32_t k2 = (k + 1) % 3;
            alt_aaline((px[k] + 8) / 16, (py[k] + 8) / 16, (px[k2] + 8) / 16, (py[k2] + 8) / 16, alt_gold_line(75));
        }
        if (i == au.sel)                         /* the chosen circle: a dot in its piece */
            alt_disc((cx + gx + ox) / 16, (cy + gy + oy) / 16, 2, col);
    }
}

/* VERSION TEST (03/11) : la charge du processeur en haut à gauche de PLAY, relue 2 fois par seconde : moyenne et pic
 * de la seconde passée en %, puis les voix coupées par la protection de Felucca (> 85 %, audio.c shed_*) et les
 * trous dans le son (demi-blocs en retard). Rouge dès qu'il y a eu une coupure ou un trou. Navigateur : rien. */
#ifndef ALTER_CPU_SHOW
#ifdef FELUCCA_WEB
#define ALTER_CPU_SHOW 0
#else
#define ALTER_CPU_SHOW 1
#endif
#endif
#if ALTER_CPU_SHOW
static uint32_t alt_cpu_ms, alt_cpu_avg, alt_cpu_pk, alt_cpu_cut;
static void alt_cpu_poll(void)
{
    if (fm1_ms - alt_cpu_ms < 500u)
        return;
    alt_cpu_ms = fm1_ms;
    alt_cpu_avg = song.cpu_q8 * 100u / 256u;
    alt_cpu_pk = alt_cpu_pk_q8 * 100u / 256u;
    alt_cpu_cut = shed_count + alt_cpu_late;
}
static void alt_cpu_num(char *b, uint32_t *n, uint32_t v)
{
    if (v > 999u)
        v = 999u;
    if (v >= 100u)
        b[(*n)++] = (char)('0' + v / 100u);
    if (v >= 10u)
        b[(*n)++] = (char)('0' + v / 10u % 10u);
    b[(*n)++] = (char)('0' + v % 10u);
}
static void alt_cpu_line(void)
{
    char b[16];
    uint32_t n = 0;
    uint16_t col = alt_cpu_cut ? ALT_RED : alt_cpu_pk >= 75u ? ALT_GOLD : ALT_DIM;
    b[n++] = 'C', b[n++] = 'P', b[n++] = 'U', b[n++] = ' ';
    alt_cpu_num(b, &n, alt_cpu_avg);
    b[n++] = '/';
    alt_cpu_num(b, &n, alt_cpu_pk);
    b[n++] = '%';
    b[n] = 0;
    cv_text(9, 19, ALT_FS, b, col);
    if (alt_cpu_cut) {
        n = 0;
        b[n++] = 'C', b[n++] = 'U', b[n++] = 'T', b[n++] = ' ';
        alt_cpu_num(b, &n, alt_cpu_cut);
        b[n] = 0;
        cv_text(9, 33, ALT_FS, b, ALT_RED);
    }
}
#endif

static void alt_scene(uint32_t q, uint32_t flash)
{
    const track_t *t = &trk[au.sel];
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    char b[24];
    uint32_t i, j;
    if (au.lockc) {
        alt_locks_scene();
        return;
    }
    if (au.scene_open) {
        alt_scenes_scene();
        return;
    }
    if (au.mat_open) {
        alt_matrix_scene();
        return;
    }
    if (au.seq_open) {
        alt_seq_scene();
        return;
    }
    if (afp.open) {
        afp_scene();
        return;
    }
    if (au.mn == QP_ENV) {
        alt_env_scene();
        return;
    }
    if (au.mn == QP_SCL) {
        alt_scl_scene();
        return;
    }
    if (au.mn == QP_EDIT) {
        alt_set_scene();
        return;
    }
    /* the card's engraving: rays from the centre (under everything, kept off the knobs) */
    for (i = 0; i < 48u; i++) {
        uint32_t tn = i * (0xFFFFFFFFu / 48u);
        int32_t cs = sine_i(tn + 0x40000000u), r1 = 98, x0, y0, x1, y1;
        if (cs < 0 && r1 * -cs > (76 << 15))
            r1 = (76 << 15) / -cs;
        alt_pt(ALT_CX, ALT_CY, tn, 46, &x0, &y0);
        alt_pt(ALT_CX, ALT_CY, tn, r1, &x1, &y1);
        alt_aaline(x0, y0, x1, y1, alt_gold_line(12));
    }
    /* the head: the card, the sound's name in its ribbon, the arcana of the page shown; the tempo, the engine, the
     * mode */
    {
        static const char *const ARC[QP_N] = {"XIV", "X", "XIII", "XVI", "XVIII", "I", "XVII", "XXI"};
        uint32_t bpm = (uint32_t)song.g[G_BPM], n = 0, m = alt_emode[au.sel] % AM_N;
        int32_t x;
        alt_card(ALT_PRE[au.pre[au.sel] % ALT_NPRE].name, ARC[q % QP_N]);
        if (q)
            cv_text(9, 19, ALT_FS, alt_qp_name(q), ALT_COL[au.sel]);
        if (bpm >= 100u)
            b[n++] = (char)('0' + bpm / 100u);
        b[n++] = (char)('0' + bpm / 10u % 10u);
        b[n++] = (char)('0' + bpm % 10u);
        b[n] = 0;
        x = cv_text_r(231, 3, ALT_FV, b, ALT_INK, ALT_BG) - text_w(ALT_FV, b) - 9;
        if (song.playing)
            alt_tri(x * 16, 7 * 16, x * 16, 15 * 16, (x + 6) * 16, 11 * 16, ALT_GO);
        else
            cv_rect(x, 8, 6, 6, ALT_DIM);
        if (ar_on)                               /* REC: a red dot while it writes, a ring when the chosen circle has moves */
            alt_disc(x - 9, 11, 4, ALT_RED);
        else if (ar_n[au.sel % AS_NCIRC]) {
            alt_disc(x - 9, 11, 4, ALT_RED);
            alt_disc(x - 9, 11, 2, ALT_BG);
        }
        x = cv_text_r(231, 19, ALT_FS, ALT_MODE_NAME[m], m ? ALT_COL[au.sel] : ALT_DIM, ALT_BG);
        cv_text_r(231 - text_w(ALT_FS, ALT_MODE_NAME[m]) - 6, 19, ALT_FS, e->name, ALT_DIM, ALT_BG);
        (void)x;
#if ALTER_CPU_SHOW
        alt_cpu_line();
#endif
    }
    /* the alidade: the chosen circle's step, a gold hand from the centre */
    if (song.playing) {
        const as_circ_t *c = &alt_seq.c[au.sel];
        uint32_t len = c->cfg.len ? c->cfg.len : 1u, now = (c->pos + len - 1u) % len;
        int32_t x, y;
        alt_at((uint32_t)(((uint64_t)now << 32) / len), 90, &x, &y);
        alt_aaline(ALT_CX, ALT_CY, x, y, alt_gold_line(45));
    }
    /* the circles, each a sigil: its ring, its hits joined (a square, a triangle, a star ..), its hits as marks: a
     * diamond (outer), a sun (middle), a dot (inner) */
    for (i = 0; i < AS_NCIRC; i++) {
        const as_circ_t *c = &alt_seq.c[i];
        uint32_t len = c->cfg.len ? c->cfg.len : 1u, now = (c->pos + len - 1u) % len, m = 0, st;
        int muted = au.cfg[i].mute, dim = q == QP_SEQ && i != au.sel, chosen = i == au.sel;
        uint16_t col = muted ? ALT_GREY : ALT_COL[i];
        int32_t r = ALT_R[i], x, y, hx[32], hy[32];
        if (dim)
            col = alt_fade(col, 30);
        alt_aaring16(ALT_CX * 16, ALT_CY * 16, r * 16, chosen ? col : alt_gold_line(muted ? 30 : 55));   /* the ring: gold, */
        if (chosen)                              /* the chosen circle's in its colour, doubled */
            alt_aaring16(ALT_CX * 16, ALT_CY * 16, r * 16 - 48, alt_fade(col, 35));
        for (j = 0; j < len && j < 32u; j++)
            if ((c->pat >> j) & 1u)
                alt_at((uint32_t)(((uint64_t)j << 32) / len), r, &hx[m], &hy[m]), m++;
        if (m >= 2u) {                           /* the sigil: the hits joined, a star once there are five */
            uint32_t k, cur = 0;
            st = 1u;
            if (m >= 5u) {
                st = (m * 2u + 2u) / 5u;
                while (st > 1u) {
                    uint32_t a2 = m, b2 = st;
                    while (b2)
                        a2 %= b2, a2 ^= b2, b2 ^= a2, a2 ^= b2;
                    if (a2 == 1u)
                        break;
                    st--;
                }
            }
            for (k = 0; k < (m == 2u ? 1u : m); k++) {
                uint32_t nx = (cur + st) % m;
                alt_aaline(hx[cur], hy[cur], hx[nx], hy[nx], i ? alt_fade(col, 40) : alt_gold_line(muted ? 20 : 40));
                cur = nx;
            }
        }
        for (j = 0; j < len; j++) {              /* the steps */
            int on = (int)((c->pat >> j) & 1u), here = song.playing && j == now;
            alt_at((uint32_t)(((uint64_t)j << 32) / len), r, &x, &y);
            if (here && on)
                alt_star(x, y, (flash >> i) & 1u ? 9 : 7, ALT_INK);
            if (!on) {
                cv_rect(x - 1, y - 1, 2, 2, here ? ALT_INK : alt_gold_line(muted ? 25 : 45));
                continue;
            }
            if (i == 0u) {
                alt_diamond(x, y, 4, 6, alt_mixc(ALT_BG, ALT_INK, 70));
                alt_diamond(x, y, 3, 5, col);
            } else if (i == 1u) {
                alt_disc(x, y, 4, col);
                alt_disc(x, y, 2, ALT_BG);
                cv_rect(x, y, 1, 1, ALT_INK);
            } else
                alt_disc(x, y, 2, col);
        }
        if (c->die) {                            /* a draw on: a small triangle at the circle's left, pointing in */
            int32_t k2;
            alt_at(0xC0000000u, r, &x, &y);
            for (k2 = 0; k2 < 7; k2++)
                cv_rect(x - 16 + k2, y - (6 - k2), 1, 2 * (6 - k2) + 1, ALT_COL[i]);
        }
    }
    /* the middle: the key held or tapped, else the three circles' triangle in its gold circle */
    if (au.msg && fm1_ms - au.msg_ms < 900u) {
        int32_t w = text_w(ALT_FS, au.msg);
        cv_rect(ALT_CX - w / 2 - 5, ALT_CY - 10, w + 10, 20, ALT_BG);
        cv_frame(ALT_CX - w / 2 - 5, ALT_CY - 10, w + 10, 20, ALT_GOLD);
        cv_text_on(ALT_CX - w / 2, ALT_CY - 7, ALT_FS, au.msg, ALT_INK, ALT_BG);
    } else {
        alt_aaring16(ALT_CX * 16, ALT_CY * 16, 27 * 16, alt_gold_line(45));
        alt_prism();
    }
    alt_foot(q);
}


/* the card's grain (DAY: paper over the cream; NIGHT: ink over the black), the same every frame (a hash of the pixel's place, not a random draw), in
 * three tones near ALT_BG; half h of the screen (rows h*120 ..), straight into the canvas just cleared */
static void alt_paper(uint32_t h)
{
    uint16_t t[4];
    uint32_t x, y, i = 0;
    t[0] = swap16(ALT_BG);
    t[1] = swap16(ALT_DAY ? RGB(234, 220, 188) : RGB(19, 16, 20));
    t[2] = swap16(ALT_DAY ? RGB(225, 209, 175) : RGB(9, 8, 10));
    t[3] = swap16(ALT_DAY ? RGB(249, 239, 213) : RGB(23, 20, 22));
    for (y = 0; y < 120u; y++) {
        uint32_t yy = (y + h * 120u) * 0x9E3779B1u;
        for (x = 0; x < 240u; x++, i++) {
            uint32_t v = (x * 0x85EBCA77u) ^ yy;
            v ^= v >> 15;
            v *= 0x2C1B3C6Du;
            v ^= v >> 13;
            v &= 15u;                            /* 3 in 16 a shade darker, 1 in 16 darker still, 2 in 16 lighter */
            cv_px[i] = t[v < 3u ? 1 : v == 3u ? 2 : v < 6u ? 3 : 0];
        }
    }
}

static void alter_draw(void)
{
    uint32_t sig = 0, i, h, flash = 0, q = au.lockc ? 0u : alt_page();
    for (i = 0; i < AS_NCIRC; i++) {
        if ((h = alt_hits[i]) != au.hits_seen[i]) {
            au.hits_seen[i] = h;
            au.flash_ms[i] = fm1_ms;
        }
        if (fm1_ms - au.flash_ms[i] < 70u)
            flash |= 1u << i;
        sig = sig * 7u + alt_glow(i);
    }
    for (i = 0; i < AS_NCIRC; i++)               /* what the picture depends on */
        sig = sig * 31u + alt_seq.c[i].pos + alt_seq.c[i].pat * 7u + (uint32_t)alt_seq.c[i].cfg.len * 131u +
              au.cfg[i].len + au.cfg[i].hits * 3u + au.cfg[i].rot * 5u + au.cfg[i].prob * 9u + au.cfg[i].mute * 11u +
              alt_seq.c[i].die * 13u + trk[i].eng_req * 17u;
    for (i = 0; i < 4u; i++) {
        const char *nm;
        char tx[12];
        int32_t f;
        sig = sig * 31u + (uint32_t)alt_kv(q, i, &nm, tx, &f);
        if (f >= 0 && !au.mat_open && !au.scene_open && !au.lockc) {
            int32_t lo, hi, pc;
            alt_live_bar(q, i, f, &lo, &hi, &pc);
            sig = sig * 31u + (uint32_t)(lo * 977 + hi * 31 + 7);   /* (the range) */
        }
        sig = sig * 31u + (uint32_t)alt_knob_lfo(q, i);
    }
    sig = sig * 31u + alt_emode[au.sel] * 37u + alt_seq.scale * 41u + au.scene_open * 43u + au.row[au.tab] * 47u + au.tab * 59u +
          au.pre[au.sel] * 53u;
    sig = sig * 31u + (au.mat_open ? alt_mat_sig() * 2u + 1u : 0u) + (au.seq_open ? alt_seq_sig() * 4u + 3u : 0u) +
          (afp.open ? afp_sig() * 8u + 5u : 0u);
    sig = sig * 31u + au.mn * 61u + au.set_row * 67u + (uint32_t)ALT_DAY * 71u + au.cfg[au.sel % AS_NCIRC].chord * 73u;
    sig = sig * 31u + alt_dial_live() * 83u;
    sig = sig * 31u + (uint32_t)ar_on * 89u + (ar_n[au.sel % AS_NCIRC] != 0) * 97u;   /* (REC's mark) */     /* (a number shown while its knob turns, then gone) */
    sig = sig * 31u + q * 19u + au.lockc * 23u + alt_seq.c[au.lockc ? au.lockc - 1u : 0u].locks * 29u;
    sig = sig * 31u + au.sel * 3u + flash * 5u + (uint32_t)song.playing * 7u + trk[au.sel].preset * 11u +
          (uint32_t)song.g[G_BPM] * 13u + (au.msg && fm1_ms - au.msg_ms < 1500u ? (uint32_t)(uintptr_t)au.msg + (fm1_ms - au.msg_ms < 900u) : 1u);
#if ALTER_CPU_SHOW
    alt_cpu_poll();
    sig = sig * 31u + alt_cpu_avg * 101u + alt_cpu_pk * 103u + alt_cpu_cut * 107u;   /* (version test : la charge CPU) */
#endif
    ui.force = 1;                                /* (whatever comes after this view draws itself whole) */
    if (au.drawn && sig == au.sig)
        return;
    au.sig = sig;
    au.drawn = 1;
    for (i = 0; i < 2u; i++) {                   /* two halves: the canvas holds 240 x 124 */
        cv_begin(240, 120, ALT_BG);
        alt_paper(i);                            /* (both cards have a grain: paper, ink) */
        cv_oy = -(int32_t)i * 120;
        alt_scene(q, flash);
        cv_oy = 0;
        cv_blit(0, i * 120u);
    }
}

/* the browser build's hooks (web/emu/felucca_web.c web_alter): op 0 circle a's sound b, 1 scene a, 2 mute circle a
 * (b = 0 / 1), 3 circle a's envelope mode b, 4 tempo a, 5 circle a's mode: its sound's own, 6 Felucca's phrase b on
 * part a (as its own SAVE page would: the test that ALTER keeps Felucca's sequencer quiet), 7 the FX page's preset b
 * of slot a (alter_fx.c AFX_S_*: 0..2 a circle's DIST), 8 MASTER setting a (amx) to b, 9 circle a's
 * DELAY lane at step b off, 10 circle a's envelope mode settings to their defaults, 11 circle a aligns its level,
 * 12 the MASTER FILTER's setting a (0..10 A, 11..21 B, 22 MORPH) to b, 13 its preset b into snapshot a */
/* the browser build's tool: USER scene u's records as bytes (its scene, its three rhythms, its effects, its envelope
 * settings), so tools/alter/alter_user_scene.mjs can turn a USER scene into a factory one (alter_sounds.c) */
#define ALT_DUMP_N (sizeof(alt_scene_rec_t) + sizeof(alt_rhy_t) * AS_NCIRC + 1u + ALT_FXREC + 1u + AS_NCIRC * AE_N)
static uint8_t alt_dump[ALT_DUMP_N];
static uint32_t alt_dump_user(uint32_t u)
{
    uint32_t n = 0, k;
    u %= ALT_NUSER;
    for (k = 0; k < sizeof(alt_scene_rec_t); k++)
        alt_dump[n++] = ((const uint8_t *)&alt_bank_s.s[u])[k];
    for (k = 0; k < sizeof(alt_rhy_t) * AS_NCIRC; k++)
        alt_dump[n++] = ((const uint8_t *)alt_bank_r.r[u])[k];
    alt_dump[n++] = alt_bank_r.fxok[u];
    for (k = 0; k < ALT_FXREC; k++)
        alt_dump[n++] = alt_bank_r.fx[u][k];
    alt_dump[n++] = alt_bank_p.envok[u];
    for (k = 0; k < AS_NCIRC * AE_N; k++)
        alt_dump[n++] = ((const uint8_t *)alt_bank_p.env[u])[k];
    return n;
}

static void alt_web_cmd(uint32_t op, uint32_t a, uint32_t b)
{
    if (!au.ready)
        return;
    switch (op) {
    case 0: alt_load_sound(a % AS_NCIRC, b); break;
    case 1: alt_load_scene(a); break;
    case 2: au.cfg[a % AS_NCIRC].mute = (uint8_t)(b != 0); alt_post_circle(a % AS_NCIRC); break;
    case 3: alt_emode[a % AS_NCIRC] = (uint8_t)(b % AM_N); break;
    case 4: song.g[G_BPM] = (int16_t)clamp((int32_t)a, 40, 240); break;
    case 5: alt_emode[a % AS_NCIRC] = (uint8_t)(ALT_PRE[au.pre[a % AS_NCIRC]].mode % AM_N); break;
    case 6: pat_load(&trk[a % NPART], b); break;   /* (a test: Felucca's own phrase b on part a) */
    case 7:                                      /* the FX page's preset b of slot a (0..2 a circle's DIST, 3 DELAY, 4 REVERB, 5 MASTER) */
        if (a < AS_NCIRC) afp_dist_apply(a, b); else if (a == AFX_S_DLY) afp_dly_apply(b);
        else if (a == AFX_S_REV) afp_rev_apply(b); else afp_mst_apply(b);
        break;
    case 8: amx[a % AMX_N] = (uint8_t)b; break;  /* (a test: MASTER setting a) */
    case 9: alt_seq.c[a % AS_NCIRC].sdly[b % AS_NLANE] = 0; break;   /* (a test: a step's DELAY lane off) */
    case 10: alt_env_default(a % AS_NCIRC); break;   /* (a test: circle a's mode settings back to the defaults) */
    case 11: alt_cal(a % AS_NCIRC); break;       /* (a test: circle a aligns its level) */
    case 12:                                     /* (a test: the MASTER FILTER's setting a: 0..10 A, 11..21 B, 22 MORPH) */
        if (a < 2u * AFL_N) afl[a / AFL_N][a % AFL_N] = (uint8_t)b; else afl_morph = (uint8_t)b;
        break;
    case 13: afl_pre_apply(a & 1u, b); break;    /* (a test: the MASTER FILTER's preset b into snapshot a) */
    case 14:                                     /* (a test: circle a's engine setting b >> 8 (0..7) to (int8) b) */
        fm1_irq_off();
        trk[a % AS_NCIRC].p[P_E0 + ((b >> 8) & 7u)] = (int8_t)(b & 255u);
        fm1_irq_on();
        break;
    default: break;
    }
}
