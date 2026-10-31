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
 * circle turned a step, kept); the blacks in their groups 3-2-3-2-1, on the chosen circle: held ROLL 1/16 1/32
 * triplet | RATCHET x2 x3 | DUB DELAY, DUB REVERB, DESTROY | tapped DENSITY - + (a hit less / more, kept) | held
 * REVERSE. All on the grid (alter_play.c). LEDs: the keys that act faint, bright when engaged (a circle muted, a
 * fill held, a tap just made); the others dark.
 * The screen: three concentric circles (circle 1 outside), each a voice's hits, lit as they play; the chosen one a
 * solid ring, the others dotted; a draw on: a small triangle at the circle's left, its colour; the chosen
 * circle's sound at the top, its knobs (or its rhythm, SEQ held) at the bottom. Drawn only when something changed. */

enum { AK_NONE, AK_MUTE, AK_DIE, AK_SHIFT, AK_DENS, AK_FILL, AK_TLOCK };
typedef struct {
    uint8_t kind, arg;
    const char *name;
} alt_key_t;
static const alt_key_t ALT_KEY[27] = {    /* k: semitones above F3 (whites 0 2 4 6 7 9 11 12 14 16 18 19 ..) */
    [0] = {AK_MUTE, 0, "MUTE 1"}, [2] = {AK_MUTE, 1, "MUTE 2"}, [4] = {AK_MUTE, 2, "MUTE 3"},
    [7] = {AK_DIE, 0, "DICE 1"}, [9] = {AK_DIE, 1, "DICE 2"}, [11] = {AK_DIE, 2, "DICE 3"},
    [14] = {AK_SHIFT, 0, "SHIFT -1"}, [16] = {AK_SHIFT, 1, "SHIFT +1"},   /* (TURING: PREV, NEXT) */
    [18] = {AK_TLOCK, 0, "LOCK"},                                           /* (TURING only: frozen / free) */
    [1] = {AK_FILL, AF_ROLLT8, "ROLL 1/8T"}, [3] = {AK_FILL, AF_ROLL16, "ROLL 1/16"}, [5] = {AK_FILL, AF_BUILD, "BUILD UP"},
    [8] = {AK_FILL, AF_RAT2, "RATCHET X2"}, [10] = {AK_FILL, AF_RAT3, "RATCHET X3"},
    [13] = {AK_FILL, AF_DLY, "DUB DELAY"}, [15] = {AK_FILL, AF_REV, "DUB REVERB"}, [17] = {AK_FILL, AF_DESTROY, "DESTROY"},
    [20] = {AK_DENS, 0, "DENSITY -"}, [22] = {AK_DENS, 1, "DENSITY +"},
    [25] = {AK_FILL, AF_REVERSE, "REVERSE"},
};
#define ALT_DIE_HOLD_MS 600u
#define ALT_TAP_MS 150u                   /* a tapped key's bright LED */
#define ALT_SAVE_HOLD_MS 1000u            /* SAVE held this long on its page: store */
#define ALT_PAGE_HOLD_MS 2000u            /* a page button held this long: it closes on release (shorter: stays open) */

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
                                   B_OCTUP, B_REC, B_ARP};   /* (REC, ARP: nothing yet, and not Felucca's) */
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
static const uint8_t ALT_ENGS[AS_NCIRC] = {ENGI_ALTER, ENGI_PHYS, 11};   /* 11: NOISE (engines.c ENGINES) */
#include "alter_sounds.c"

/* circle c's sound: ALT_PRE[i] on its part (the engine, its defaults, then the sound's values) */
static void alt_load_sound(uint32_t c, uint32_t i)
{
    track_t *t = &trk[c];
    const alt_pre_t *pr = &ALT_PRE[i % ALT_NPRE];
    uint32_t e = ALT_ENGS[pr->eng % AS_NCIRC], j;
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
        c->rot = (uint8_t)(((int32_t)c->rot + s + 4 * AS_MAXLEN) % c->len);
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
        cf->chord = (uint8_t)clamp((int32_t)cf->chord + (s > 0 ? 1 : -1), 0, AS_NCHORD - 1);
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
        c->chord = (uint8_t)clamp((int32_t)c->chord + (s > 0 ? 1 : -1), 0, AS_NCHORD - 1);
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
#define ALT_SEQ_HOLD_MS 500u                     /* SEQ held this long: the menu */
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
            if (b < LN_N) {
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
            } else if (i >= 2u)                  /* KNOB 3 / 4 the bottom / top of their RANGE (KNOB 2: nothing yet) */
                alt_lane_range(i - 2u, s);
        }
    alt_hold = 0;
}

/* the LFO page has this frame (1), or it has just been left (0: the frame goes on as usual) */
static int alt_matrix_input(uint32_t pressed)
{
    uint32_t q, i, lfo = (fm1_in.buttons >> panel.btn[B_LFO]) & 1u;
    int32_t s;
    for (q = 1; q < QP_N; q++)                   /* another page button: that page instead */
        if (q != QP_LFO && ((pressed >> panel.btn[ALT_QP_BTN[q]]) & 1u)) {
            au.mat_open = 0;
            return 0;
        }
    if (!lfo && ((au.qp_was >> QP_LFO) & 1u)) { /* LFO let go: back to the circles */
        au.qp_was &= (uint8_t)~(1u << QP_LFO);
        au.mat_open = 0;
        au.qp = QP_NONE;
        return 1;
    }
    au.qp_was = (uint8_t)((au.qp_was & ~(1u << QP_LFO)) | (lfo << QP_LFO));
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

static uint32_t alt_crng;
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
    for (c = 0; c < AS_NCIRC; c++) {
        track_t *t = &trk[c];
        uint32_t pi = alt_cr(ALT_NPRE), e = ALT_ENGS[ALT_PRE[pi].eng % AS_NCIRC], ei;
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
        alt_cl[c][0].depth = (int8_t)((int32_t)alt_cr(81u) - 40);
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
    }
    alt_say("CHAOS");
}

static void alt_select(uint32_t i)
{
    au.sel = (uint8_t)(i % AS_NCIRC);
    alt_sel = au.sel;
    track_select(au.sel);                        /* Felucca's pages (ENV, FX, EDIT ..) edit this circle's sound */
}

/* 1: the PLAY view had this frame's input (ui_input stops); 0: Felucca's (another page, or a button that opens one) */
static void afp_input(void);                     /* the FX page (alter_fxpage.c, with the screen below) */
#define ALT_FX_HOLD_MS 500u                       /* FX held this long: the FX page */
static int alter_ui_input(uint32_t pressed)
{
    uint32_t keys = fm1_in.notes, down, k, i, ours = alt_ours(), q, octs, both;
    int32_t s;
    both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
    octs = (fm1_in.buttons & both) == both;
    if (octs && alter_view() && !au.handed)      /* OCT- + OCT+ + a page button: Felucca's own page */
        for (q = 1; q <= QP_N; q++)              /* (QP_N: SAVE, Felucca's save pages) */
            if ((pressed >> panel.btn[q < QP_N ? ALT_QP_BTN[q] : B_SAVE]) & 1u) {
                au.handed = 1;
                au.handed_ms = fm1_ms;
                au.qp = QP_NONE;
                au.qp_was = 0;
                au.scene_open = 0;
                au.mat_open = 0;
                au.seq_open = 0;
                afp.open = 0;
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
    if ((pressed >> panel.btn[B_HOME]) & 1u) {   /* HOME: the circles, no page */
        au.qp = QP_NONE;
        au.lockc = 0;
        au.scene_open = 0;
        au.mat_open = 0;
        au.seq_open = 0;
        afp.open = 0;
    }
    if (((pressed >> panel.btn[B_SAVE]) & 1u) && !au.scene_open) {   /* SAVE: the SAVE page (its press opens it) */
        au.scene_open = 1;
        au.save_t0 = 0;
        au.qp = QP_NONE;
        au.lockc = 0;
        au.mat_open = 0;
        au.seq_open = 0;
        afp.open = 0;
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
    if (au.mat_open && alt_matrix_input(pressed)) {
        au.keys = keys;
        return 1;
    }
    for (q = 1; q < QP_N; q++) {                 /* the page buttons: a tap keeps the page, a long hold does not */
        uint32_t dn = (fm1_in.buttons >> panel.btn[ALT_QP_BTN[q]]) & 1u, was = (au.qp_was >> q) & 1u;
        if (dn && !was && q != QP_SEQ && au.seq_open)
            au.seq_open = 0;                     /* (another page: the SEQ menu closes) */
        if (dn && !was && q != QP_FX && afp.open)
            afp.open = 0;                        /* (another page: the FX page closes) */
        if (q == QP_FX && dn && was && !afp.open &&
            (int32_t)(fm1_ms - au.qp_t0[q]) >= (int32_t)ALT_FX_HOLD_MS) {   /* FX held 0.5 s: the FX page */
            afp.open = 1;
            afp.hold = 1;
            au.qp = QP_NONE;
            au.seq_open = 0;
            alt_say("FX");
        }
        if (q == QP_FX && !dn && was && (afp.hold || afp.open)) {   /* its release: the page stays (a hold), or */
            if (!afp.hold)                       /* closes (a tap on it) */
                afp.open = 0;
            afp.hold = 0;
            au.qp_was &= (uint8_t)~(1u << q);
            continue;
        }
        if (q == QP_SEQ && dn && was && !au.seq_open &&
            (int32_t)(fm1_ms - au.qp_t0[q]) >= (int32_t)ALT_SEQ_HOLD_MS) {   /* SEQ held 0.5 s: the menu */
            au.seq_open = 1;
            au.seq_hold = 1;
            au.qp = QP_NONE;
            au.step_down = 0;
            alt_say("SEQ");
        }
        if (q == QP_SEQ && !dn && was && (au.seq_hold || au.seq_open)) {   /* its release: the menu stays (a hold), */
            if (!au.seq_hold)                    /* or closes (a tap on it) */
                au.seq_open = 0;
            au.seq_hold = 0;
            au.qp_was &= (uint8_t)~(1u << q);
            continue;
        }
        if (dn && !was) {
            au.qp_t0[q] = fm1_ms;
            au.lockc = 0;
            alt_say(alt_qp_name(q));
        } else if (!dn && was) {
            if ((int32_t)(fm1_ms - au.qp_t0[q]) < (int32_t)ALT_PAGE_HOLD_MS && q == QP_LFO && au.qp == QP_LFO) {
                au.qp = QP_NONE;                 /* LFO tapped on its page: the MATRIX */
                au.mat_open = 1;
                au.mtab = 0;
                au.dd = 0;
                alt_say("MATRIX");
            } else if ((int32_t)(fm1_ms - au.qp_t0[q]) < (int32_t)ALT_PAGE_HOLD_MS)
                au.qp = (uint8_t)(au.qp == q ? QP_NONE : q);
            else if (au.qp == q)
                au.qp = QP_NONE;
        }
        au.qp_was = (uint8_t)((au.qp_was & ~(1u << q)) | (dn << q));
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
        afp_input();
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
    /* the keys */
    for (k = 0; k < 27u; k++) {
        const alt_key_t *a = &ALT_KEY[k];
        uint32_t i2 = a->arg;
        int tur = au.cfg[au.sel].type == AS_T_TURING;
        if ((down >> k) & 1u && a->kind && (a->kind != AK_TLOCK || tur)) {
            alt_say(a->kind == AK_SHIFT && tur ? (i2 ? "NEXT" : "PREV") : a->name);
            if (a->kind == AK_MUTE) {
                au.cfg[i2].mute ^= 1u;
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
                alt_shift_tot[au.sel] += d;      /* (TURING: the audio side takes it as NEXT / PREV) */
                if (!tur)
                    au.cfg[au.sel].rot = (uint8_t)(((int32_t)au.cfg[au.sel].rot + d + 4 * AS_MAXLEN) % au.cfg[au.sel].len);
                au.tap_ms[k] = fm1_ms | 1u;
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
                alt_say("CLEAN");
            }
        } else if (au.die_t0[i2]) {              /* let go: a tap rolls, at once */
            if (!((au.die_clean_done >> i2) & 1u)) {
                alt_roll_cnt[i2]++;
                au.tap_ms[k] = fm1_ms | 1u;
                alt_say("NEW DRAW");
            }
            au.die_t0[i2] = 0;
        }
    }
    {
        uint32_t h = 0;
        for (k = 0; k < 27u; k++)
            if ((keys >> k) & 1u && ALT_KEY[k].kind == AK_FILL)
                h |= 1u << ALT_KEY[k].arg;
        alt_hold = (uint16_t)h;
        if (h && au.msg)                         /* a fill held: its name stays in the middle */
            au.msg_ms = fm1_ms;
    }
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
        else if (a->kind == AK_DIE || a->kind == AK_SHIFT || a->kind == AK_DENS)
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
}

/* ------------------------------------------------------------------ the screen --- */
#define ALT_CX 120
#define ALT_CY 116
static const uint8_t ALT_R[AS_NCIRC] = {80, 60, 40};
static const uint16_t ALT_COL[AS_NCIRC] = {RGB(240, 153, 123), RGB(93, 202, 165), RGB(250, 199, 117)};
#define ALT_GREY RGB(70, 70, 74)
#define ALT_DARK RGB(40, 40, 44)
#define ALT_INK RGB(235, 235, 235)
#define ALT_DIM RGB(150, 150, 155)

static void alt_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t dx = (int32_t)alt_isqrt((uint32_t)(r * r - dy * dy));
        cv_rect(cx - dx, cy + dy, 2 * dx + 1, 1, c);
    }
}

static void alt_at(uint32_t turn, int32_t r, int32_t *x, int32_t *y)   /* turn: 0 = top, clockwise */
{
    *x = ALT_CX + ((r * sine_i(turn)) >> 15);
    *y = ALT_CY - ((r * sine_i(turn + 0x40000000u)) >> 15);
}

static uint16_t alt_fade(uint16_t c, int32_t pct) { return ux_mix(RGB(0, 0, 0), c, pct); }

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

/* knob i of quick page q as shown: its name, and its value as text (txt[0] set) or as a bar (*fill of 50 px);
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
        return v;
    case QK_BPM:
        *name = "TEMPO";
        v = song.g[G_BPM];
        alt_num(txt, v, 0);
        return v;
    case QK_SWING:
        *name = "SWING";
        v = song.g[G_SWING];
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
        } else
            *fill = span > 0 ? (v - d->min) * 50 / span : 0;
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
    alt_disc(14, 13, 5, ALT_COL[c]);
    cv_text(26, 2, &AF_M, "LOCKS", ALT_INK);
    cv_text(8, 26, &AF_S, "WHITE KEYS F3..E4   HOME: BACK", ALT_DIM);
    for (i = 0; i < 7u; i++) {
        int32_t y = 52 + (int32_t)i * 24, on = (lk & ALT_LK_BIT[i]) != 0;
        cv_rect(12, y + 2, 10, 10, on ? ALT_COL[c] : ALT_DARK);
        cv_text(32, y, &AF_S, ALT_LK_NAME[i], on ? ALT_INK : ALT_DIM);
        cv_text_r(228, y, &AF_S, on ? "LOCKED" : "FREE", on ? ALT_COL[c] : ALT_DIM, cv_bg);
    }
}

static void alt_scenes_scene(void)
{
    static const char *const TAB[2] = {"SCENES", "PATTERNS"};
    uint32_t t = au.tab, r = au.row[t], i, top;
    char nb[12];
    for (i = 0; i < 2u; i++) {                   /* the tabs */
        int32_t x = 8 + (int32_t)i * 100, w = text_w(&AF_M, TAB[i]);
        cv_text(x, 4, &AF_M, TAB[i], i == t ? ALT_INK : ALT_GREY);
        if (i == t)
            cv_rect(x, 26, w, 2, ALT_COL[0]);
    }
    if (au.msg && fm1_ms - au.msg_ms < 1500u)   /* SAVED, PROTECTED, EMPTY .. */
        cv_text(8, 216, &AF_S, au.msg, ALT_COL[1]);
    else
        cv_text(8, 216, &AF_S, "SELECT  SAVE: LOAD  HOLD: STORE", ALT_DIM);
    top = r < 2u ? 0u : r - 2u;                  /* five rows, the chosen one second or third */
    if (top > ALT_NLIST - 5u)
        top = ALT_NLIST - 5u;
    for (i = 0; i < 5u; i++) {
        uint32_t row = top + i;
        int32_t y = 40 + (int32_t)i * 34, on = row == r, user = row > ALT_NFACT;
        const char *nm = alt_row_name(t, row, nb);
        if (on)
            cv_rect(4, y - 3, 232, 32, ALT_DARK);
        if (row && !user)                        /* the factory ones: a lock */
            cv_rect(14, y + 4, 6, 7, on ? ALT_DIM : ALT_GREY);
        cv_text(26, y, &AF_S, nm, on ? ALT_INK : (nm[0] == 'E' ? ALT_GREY : ALT_DIM));
        cv_text(26, y + 14, &AF_S, alt_row_about(t, row), on ? ALT_COL[t + 1u] : ALT_GREY);
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
    cv_rect(lx - 1, ly - 1, 82, 5 * 15 + 2, RGB(0, 0, 0));
    for (k = -2; k <= 2; k++) {
        int32_t v = cur + k, yy = ly + (k + 2) * 15;
        if (v < 0 || v >= n)
            continue;
        if (!k)
            cv_rect(lx, yy, 80, 15, ALT_DARK);
        cv_text(lx + 4, yy + 1, &AF_S, tb == ALT_TAB_MTX ? alt_mtx_name(ki, v, b) : alt_lfo_text(3, v, b),
                k ? ALT_DIM : col);
    }
}

/* an LFO tab: its title, one bar of time with its beats and steps, the wave scrolling through it (now at the right
 * edge: it comes in from there), its four settings (KNOB 1-4) */
static void alt_lfo_tab(uint32_t tb, uint16_t col)
{
    char b[8], t[24] = "CIRCLE 1  LFO 1";
    uint32_t i, w, r, seed, c = au.sel % AS_NCIRC, p = alt_lfo_p ? alt_lfo_p : 1u, now = alt_gl_n, bar = 16u * p;
    int on = alt_lfo_acts(tb);
    int32_t x, py = 0, y0 = 46, h = 84, mid = y0 + h / 2;
    uint16_t wc = on ? col : ALT_DIM;
    alt_lfo_def(tb, &w, &r, &seed);
    if (tb == ALT_TAB_GL)
        cv_text(8, 28, &AF_S, "GLOBAL  THE THREE CIRCLES", col);
    else {
        t[7] = (char)('1' + c);
        t[14] = (char)('1' + tb);
        cv_text(8, 28, &AF_S, t, col);
    }
    /* the grid: a bar, steps faint, beats brighter, the bar's ends; fixed to the beat as time scrolls past */
    for (i = 0; i <= 16u; i++) {
        uint32_t back = (now % p) + (16u - i) * p;     /* samples before now of the step line i */
        int32_t xx = ALT_WAVE_X + ALT_WAVE_W - (int32_t)((uint64_t)back * ALT_WAVE_W / bar), beat = ((now / p) + i) % 4u == 0u;
        if (xx >= ALT_WAVE_X && xx <= ALT_WAVE_X + ALT_WAVE_W)
            cv_rect(xx, y0, 1, h, beat ? ALT_GREY : ALT_DARK);
    }
    cv_rect(ALT_WAVE_X, mid, ALT_WAVE_W, 1, ALT_DARK);
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
            cv_pset(ALT_WAVE_X + x, mid - ((ghost[x] * (h / 2 - 3)) >> 15), ALT_GREY);
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
    for (i = 0; i < 4u; i++) {                   /* the four settings, as the knobs below them */
        int32_t v = alt_lfo_val(tb, i), xx = 8 + (int32_t)i * 58;
        cv_text(xx, 142, &AF_S, ALT_LFO_KNOB[i], ALT_DIM);
        cv_text(xx, 158, &AF_M, alt_lfo_text(i, v, b), i == 3 && !v ? ALT_DIM : ALT_INK);
        if (i == 2) {                            /* DEPTH: a bar from the middle */
            int32_t bw = (v < 0 ? -v : v) * 24 / 64;
            cv_rect(xx, 182, 50, 3, ALT_DARK);
            cv_rect(xx + 25, 180, 1, 7, ALT_GREY);
            cv_rect(v >= 0 ? xx + 25 : xx + 25 - bw, 182, bw, 3, col);
        }
    }
    if (tb < ALT_TAB_GL && alt_lfo_val(tb, 3) && alt_lfo_val(tb, 2) && !on)
        cv_text(8, 196, &AF_S, "THE GLOBAL LEADS ON THIS DEST", ALT_GREY);
    else if (!on)
        cv_text(8, 196, &AF_S, "SET A DEST AND A DEPTH", ALT_GREY);
    cv_text(8, 216, &AF_S, "SELECT: CIRCLE   KNOBS 1-4: SETTINGS", ALT_DIM);
    if (au.dd == 4u && fm1_ms - au.dd_ms < ALT_DD_MS)
        alt_dropdown(150, 62, alt_lfo_val(tb, 3), GD_N, col, tb, 3);
}

/* the MTX tab: the chosen circle's four slots, under its own title */
static void alt_mtx_tab(uint16_t col)
{
    char b0[8], b1[8], b2[8], t[24] = "MATRIX  CIRCLE 1";
    uint32_t c = au.sel % AS_NCIRC, row = au.mrow[0] % NMSLOT, r, take = alt_gl_take();
    t[15] = (char)('1' + c);
    cv_text(8, 30, &AF_S, t, col);
    cv_rect(8, 46, 224, 1, alt_fade(col, 50));
    for (r = 0; r < NMSLOT; r++) {
        int32_t y = 54 + (int32_t)r * 36, on = r == row;
        int32_t v0 = alt_mtx_val(r, 0), v1 = alt_mtx_val(r, 1), v2 = alt_mtx_val(r, 2);
        int led = ((take >> v1) & 1u) && (v0 == MS_LFO || v0 == MS_SH || v0 == MS_SLEW), act = v0 && v1 && v2;
        int32_t bw = (v2 < 0 ? -v2 : v2) * 24 / 64;
        uint16_t rb = on ? ALT_DARK : cv_bg, vc = led ? ALT_GREY : act ? col : ALT_DIM;
        if (on)
            cv_rect(2, y - 2, 236, 34, ALT_DARK);
        b0[0] = (char)('1' + r);
        b0[1] = 0;
        cv_text_on(8, y + 3, &AF_S, b0, ALT_GREY, rb);
        cv_text_on(24, y + 3, &AF_S, alt_mtx_name(0, v0, b1), on ? ALT_INK : ALT_DIM, rb);
        cv_text_on(88, y + 3, &AF_S, ">", ALT_GREY, rb);
        cv_text_on(100, y + 3, &AF_S, alt_mtx_name(1, v1, b2), on ? ALT_INK : ALT_DIM, rb);
        cv_rect(168, y + 9, 1, 7, ALT_GREY);     /* the amount: a bar from the middle, and its number */
        if (v2 > 0)
            cv_rect(169, y + 10, bw, 5, vc);
        else if (v2 < 0)
            cv_rect(168 - bw, y + 10, bw, 5, vc);
        alt_num(b0, v2, 0);
        cv_text_r(234, y + 3, &AF_S, b0, vc, rb);
        if (led)
            cv_text_on(24, y + 17, &AF_S, "GLOBAL LEADS: THIS SLOT RESTS", ALT_GREY, rb);
        else if (on)
            cv_text_on(24, y + 17, &AF_S, ALT_SRC_ABOUT[clamp(v0, 0, MS_N - 1)], ALT_GREY, rb);
    }
    cv_text(8, 216, &AF_S, "SELECT SLOT  K1 SRC  K2 DEST  K3 AMT", ALT_DIM);
    if (au.dd && au.dd <= 2u && fm1_ms - au.dd_ms < ALT_DD_MS) {   /* the list, dropped down over its column */
        uint32_t ki = au.dd - 1u;
        int32_t ly = 54 + (int32_t)row * 36 + 6 - 2 * 15;
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
    int32_t x = 8, w = 224 / 16, y0 = 66, h = 96, mid = y0 + h / 2;
    alt_disc(14, 13, 5, col);                    /* the head: the circle, SEQ, its engine (SEQ held + ALGORITHM) */
    cv_text(26, 2, &AF_M, "SEQ", ALT_INK);
    for (i = 0; i < AS_NTYPE; i++) {
        int32_t tw = text_w(&AF_S, AS_TYPE_NAME[i]);
        cv_text(x + 54 + (int32_t)i * 58, 6, &AF_S, AS_TYPE_NAME[i], i == cf->type ? col : ALT_GREY);
        if (i == cf->type)
            cv_rect(x + 54 + (int32_t)i * 58, 21, tw, 2, col);
    }
    {                                            /* the lanes (ALGORITHM), spread over the width */
        int32_t tot = 0, lx = 8, gap;
        for (i = 0; i < LN_N; i++)
            tot += text_w(&AF_S, ALT_LANE_TAB[i]);
        gap = (224 - tot) / (LN_N - 1);
        for (i = 0; i < LN_N; i++) {
            int32_t tw = text_w(&AF_S, ALT_LANE_TAB[i]);
            cv_text(lx, 32, &AF_S, ALT_LANE_TAB[i], i == ln ? ALT_INK : ALT_GREY);
            if (i == ln)
                cv_rect(lx, 47, tw, 2, col);
            lx += tw + gap;
        }
    }
    if (ln == LN_PITCH)
        cv_rect(x, mid, 224, 1, ALT_DARK);
    for (k = 0; k < 16u; k++) {                  /* the steps: a bar each, their lane's value */
        int32_t bx = x + (int32_t)k * w, v = alt_lane_val(c, ln, k), on = k < n && ((sc->pat >> k) & 1u);
        int held = (au.step_down >> k) & 1u, here = song.playing && k == now % 16u && k < n;
        uint16_t bc = k >= n ? ALT_DARK : on ? col : ALT_GREY;
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
        } else {
            int32_t bh = v * (h - 2) / 127;
            cv_rect(bx + 2, y0 + h - bh, w - 4, bh ? bh : 1, bc);
        }
        if (held)                                /* held: a white frame */
            cv_rect(bx + 1, y0 - 2, w - 2, 2, ALT_INK), cv_rect(bx + 1, y0 + h + 1, w - 2, 2, ALT_INK);
        cv_rect(bx + 2, y0 + h + 6, w - 4, 3, here ? ALT_INK : on ? alt_fade(col, 60) : ALT_DARK);   /* hit, playhead */
    }
    {
        char b[8];
        int32_t v = au.step_down ? alt_lane_val(c, ln, (uint32_t)__builtin_ctz(au.step_down)) : 0;
        if (au.step_down) {                      /* the held step's value, or its RANGE (bottom .. top) */
            uint32_t k0 = (uint32_t)__builtin_ctz(au.step_down), rg = (sc->rmask[ln] >> k0) & 1u, j2;
            int32_t vv[2], x2 = 8;
            vv[0] = rg ? sc->rlo[ln][k0] : v;
            vv[1] = rg ? sc->rhi[ln][k0] : v;
            x2 = cv_text(x2, 178, &AF_S, ALT_LANE_NAME[ln], ALT_DIM) + 6;   /* (cv_text: the x after the text) */
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
                    x2 = cv_text(x2, 178, &AF_S, "..", ALT_DIM) + 2;
                x2 = cv_text(x2, 178, &AF_S, b, ALT_INK) + 2;
            }
            if (rg)
                cv_text(x2 + 6, 178, &AF_S, "RANDOM", ALT_DIM);
        }
    }
    for (i = 0; i < 4u; i++) {                   /* the engine's four knobs */
        int32_t kx = 8 + (int32_t)i * 58, fill;
        const char *name;
        char txt[12];
        alt_kv(QP_SEQ, i, &name, txt, &fill);
        cv_text(kx, 200, &AF_S, name, ALT_DIM);
        cv_text(kx, 216, &AF_S, txt, ALT_INK);
    }
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
    uint32_t tb = au.mtab, i;
    uint16_t sc = ALT_COL[au.sel % AS_NCIRC], col = tb == ALT_TAB_GL ? ALT_INK : sc;
    int32_t x = 4;
    for (i = 0; i < 5u; i++) {                   /* the tabs: the chosen circle's colour, MTX set apart by a bar */
        int32_t w = text_w(&AF_S, TAB[i]);
        uint16_t c = i == ALT_TAB_GL ? ALT_INK : sc;
        if (i == ALT_TAB_MTX) {
            cv_rect(x - 2, 4, 1, 16, ALT_GREY);
            x += 6;
        }
        cv_text(x, 4, &AF_S, TAB[i], i == tb ? c : ALT_GREY);
        if (i < ALT_TAB_GL)                      /* LFO 1 2 3: a line in the circle's colour, full under the one shown */
            cv_rect(x, 20, w, i == tb ? 2 : 1, i == tb ? c : alt_fade(c, 45));
        else if (i == tb)
            cv_rect(x, 20, w, 2, c);
        x += w + 10;
    }
    if (tb == ALT_TAB_MTX)
        alt_mtx_tab(col);
    else
        alt_lfo_tab(tb, col);
}

#include "alter_fxpage.c"

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
    /* the head: a point of the chosen circle's colour, its sound, the transport */
    alt_disc(14, 13, 5, ALT_COL[au.sel]);
    cv_text(26, 2, &AF_M, ALT_PRE[au.pre[au.sel] % ALT_NPRE].name, ALT_INK);
    cv_text(8, 22, &AF_S, e->name, ALT_DIM);
    cv_text(8 + text_w(&AF_S, e->name) + 10, 22, &AF_S, ALT_MODE_NAME[alt_emode[au.sel] % AM_N],
            alt_emode[au.sel] ? ALT_COL[au.sel] : ALT_DIM);
    {
        uint32_t bpm = (uint32_t)song.g[G_BPM], n = 0;
        const char *st = song.playing ? "PLAY " : "STOP ";
        while (*st)
            b[n++] = *st++;
        if (bpm >= 100u)
            b[n++] = (char)('0' + bpm / 100u);
        b[n++] = (char)('0' + bpm / 10u % 10u);
        b[n++] = (char)('0' + bpm % 10u);
        b[n] = 0;
    }
    cv_text_r(234, 4, &AF_S, b, song.playing ? RGB(95, 224, 138) : ALT_DIM, cv_bg);
    /* the circles */
    for (i = 0; i < AS_NCIRC; i++) {
        const as_circ_t *c = &alt_seq.c[i];
        uint32_t len = c->cfg.len ? c->cfg.len : 1u, now = (c->pos + len - 1u) % len;
        int muted = au.cfg[i].mute, dim = q == QP_SEQ && i != au.sel, chosen = i == au.sel;
        uint16_t col = muted ? ALT_GREY : ALT_COL[i];
        int32_t r = ALT_R[i], x, y;
        if (dim)
            col = alt_fade(col, 30);
        if (chosen) {                            /* the ring: solid, full colour for the chosen circle, dotted else */
            for (j = 0; j < 360u; j++) {
                alt_at(j * (0xFFFFFFFFu / 360u), r, &x, &y);
                cv_rect(x, y, 2, 2, col);
            }
        } else
            for (j = 0; j < 96u; j++) {
                alt_at(j * (0xFFFFFFFFu / 96u), r, &x, &y);
                cv_pset(x, y, alt_fade(col, 30));
            }
        for (j = 0; j < len; j++) {              /* the steps */
            int on = (int)((c->pat >> j) & 1u), here = song.playing && j == now;
            alt_at((uint32_t)(((uint64_t)j << 32) / len), r, &x, &y);
            if (here)
                alt_disc(x, y, (flash >> i) & 1u ? 7 : 5, on ? ALT_INK : ALT_DIM);
            alt_disc(x, y, on ? 4 : 1, on ? col : ALT_GREY);
        }
        if (c->die) {                            /* a draw on: a small triangle at the circle's left, pointing in */
            int32_t k2;
            alt_at(0xC0000000u, r, &x, &y);
            for (k2 = 0; k2 < 7; k2++)
                cv_rect(x - 16 + k2, y - (6 - k2), 1, 2 * (6 - k2) + 1, ALT_COL[i]);
        }
    }
    /* the middle: the key held or tapped, else the chosen circle's flash */
    if (au.msg && fm1_ms - au.msg_ms < 900u) {
        int32_t w = text_w(&AF_S, au.msg);
        cv_rect(ALT_CX - w / 2 - 4, ALT_CY - 9, w + 8, 18, RGB(0, 0, 0));
        cv_text(ALT_CX - w / 2, ALT_CY - 8, &AF_S, au.msg, ALT_INK);
    } else
        alt_disc(ALT_CX, ALT_CY, (flash >> au.sel) & 1u ? 9 : 3, (flash >> au.sel) & 1u ? ALT_COL[au.sel] : ALT_GREY);
    /* the foot: the page's name, its four knobs */
    if (q)
        cv_text(8, 194, &AF_S, alt_qp_name(q), ALT_COL[au.sel]);
    for (i = 0; i < 4u; i++) {
        int32_t x = 8 + (int32_t)i * 58, fill;
        const char *name;
        char txt[12];
        int32_t lo = -1, hi = -1, pct = -1;
        alt_kv(q, i, &name, txt, &fill);
        if (fill >= 0)
            alt_live_bar(q, i, fill, &lo, &hi, &pct);
        cv_text(x, 209, &AF_S, name, ALT_DIM);
        if (pct < 0 && alt_knob_lfo(q, i))       /* an LFO on it the bar cannot show: a small wave by its name */
            alt_mini_wave(x + text_w(&AF_S, name) + 4, 213, ALT_COL[au.sel]);
        if (fill >= 0) {
            cv_rect(x, 227, 50, 5, ALT_DARK);
            if (pct >= 0) {                      /* an LFO moves it: the range it reaches, in the circle's colour */
                cv_rect(x + lo, 226, (hi > lo ? hi - lo : 1) + 1, 7, ALT_COL[au.sel]);
                cv_rect(x, 228, fill, 3, ALT_INK);
            } else
                cv_rect(x, 227, fill, 5, ALT_INK);
        } else
            cv_text(x, 222, &AF_S, txt, ALT_INK);
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
    sig = sig * 31u + q * 19u + au.lockc * 23u + alt_seq.c[au.lockc ? au.lockc - 1u : 0u].locks * 29u;
    sig = sig * 31u + au.sel * 3u + flash * 5u + (uint32_t)song.playing * 7u + trk[au.sel].preset * 11u +
          (uint32_t)song.g[G_BPM] * 13u + (au.msg && fm1_ms - au.msg_ms < 1500u ? (uint32_t)(uintptr_t)au.msg + (fm1_ms - au.msg_ms < 900u) : 1u);
    ui.force = 1;                                /* (whatever comes after this view draws itself whole) */
    if (au.drawn && sig == au.sig)
        return;
    au.sig = sig;
    au.drawn = 1;
    for (i = 0; i < 2u; i++) {                   /* two halves: the canvas holds 240 x 124 */
        cv_begin(240, 120, RGB(0, 0, 0));
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
 * DELAY lane at step b off, 10 circle a's envelope mode settings to their defaults */
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
    default: break;
    }
}
