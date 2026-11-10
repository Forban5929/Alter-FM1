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
 * held, on the chosen circle: DUB DELAY, DUB REVERB, DESTROY | ROLL 1/16, ROLL 1/8 | TRIGGER. The whites C4..G5
 * after LOCK: free. All on the grid (alter_play.c). LEDs: the keys that act faint, bright when engaged (a circle muted, a
 * fill held, a tap just made); the others dark.
 * The screen: three concentric circles (circle 1 outside), each a voice's hits, lit as they play; the chosen one a
 * solid ring, the others dotted; a draw on: a small triangle at the circle's left, its colour; the chosen
 * circle's sound at the top, its knobs (or its rhythm, SEQ held) at the bottom. Drawn only when something changed. */

enum { AK_NONE, AK_MUTE, AK_DIE, AK_SHIFT, AK_DENS, AK_FILL, AK_TLOCK, AK_SEL, AK_TRIG, AK_SONG };
typedef struct {
    uint8_t kind, arg;
    const char *name;
} alt_key_t;
static const alt_key_t ALT_KEY[27] = {    /* k: semitones above F3 (whites 0 2 4 6 7 9 11 12 14 16 18 19 ..) */
    [0] = {AK_MUTE, 0, "MUTE 1"}, [2] = {AK_MUTE, 1, "MUTE 2"}, [4] = {AK_MUTE, 2, "MUTE 3"},
    [7] = {AK_DIE, 0, "DICE 1"}, [9] = {AK_DIE, 1, "DICE 2"}, [11] = {AK_DIE, 2, "DICE 3"},
    [14] = {AK_SHIFT, 0, "SHIFT -1"}, [16] = {AK_SHIFT, 1, "SHIFT +1"},   /* (TURING: PREV, NEXT) */
    [18] = {AK_TLOCK, 0, "LOCK"},                                           /* (TURING only: frozen / free) */
    [19] = {AK_SONG, 0, "SONG A"}, [21] = {AK_SONG, 1, "SONG B"}, [23] = {AK_SONG, 2, "SONG C"},   /* (05/11: alt_sg_key) */
    /* the blacks (Romain, 01/11): which circle | its rhythm | the held fills */
    [1] = {AK_SEL, 0, "CIRCLE 1"}, [3] = {AK_SEL, 1, "CIRCLE 2"}, [5] = {AK_SEL, 2, "CIRCLE 3"},
    [8] = {AK_DENS, 0, "DENSITY -"}, [10] = {AK_DENS, 1, "DENSITY +"},
    [13] = {AK_FILL, AF_DLY, "DUB DELAY"}, [15] = {AK_FILL, AF_REV, "DUB REVERB"}, [17] = {AK_FILL, AF_DESTROY, "DESTROY"},
    [20] = {AK_FILL, AF_ROLL16, "ROLL 1/16"}, [22] = {AK_FILL, AF_ROLL32, "ROLL 1/8"},
    [25] = {AK_TRIG, 0, "TRIGGER"},         /* a hit of the chosen circle at once; + REC: added to it (alter_play.c alt_trig_now) */
};
/* MIDI LEARN's actions (alter_learn.c alm_poll): the keys and buttons a controller holds now, added to the FM-1's own;
 * alm_bdown: its buttons pressed this pass */
static uint32_t alm_keys, alm_btns, alm_bdown;
#define ALT_NOTES (fm1_in.notes | alm_keys)
#define ALT_BTNS (fm1_in.buttons | alm_btns)
static void alm_poll(void);
#define ALT_DIE_HOLD_MS 600u
#define ALT_REC_CLEAR_MS 1000u                   /* REC held: the chosen circle's moves cleared */
#define ALT_TAP_MS 150u                   /* a tapped key's bright LED */
#define ALT_SAVE_HOLD_MS 1000u            /* SAVE held this long on its page: store */

/* the quick pages: their buttons, names and four knobs (P_* of the chosen circle's part, or ALT_QK_* below;
 * QK_ENG: the engine's own) */
enum { QP_NONE, QP_SEQ, QP_ENV, QP_FX, QP_LFO, QP_EDIT, QP_SCL, QP_GLO, QP_N };
#define QP_SYS 15u                                /* (no button of its own: HOME held 0.5 s, the SYSTEM menu, 04/11) */
enum { QK_PROG = 198, QK_RUMBLE = 199, QK_SCALE = 200, QK_ROOT, QK_DOSE, QK_BPM, QK_SWING, QK_RHY, QK_ENG, QK_ENGX, QK_CHORD, QK_LWAVE, QK_LRATE,
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
    {QK_SCALE, QK_ROOT, QK_CHORD, QK_PROG},   /* (PROG where DOSE was, Romain 10/11: DOSE in DEEP > MIX) */
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
    uint8_t scene_open, tab, row[4], save_done;   /* the SAVE page open; its tab (0 SCENES 1 PATTERNS), each tab's row;
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
                                   B_OCTUP, B_REC, B_ARP};   /* (REC: alter_rec.c; ARP: SONG, alt_sg_arp) */
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
#define ALT_NENG 7u
static const uint8_t ALT_ENGS[ALT_NENG] = {ENGI_ALTER, ENGI_PHYS, 11, ENGI_RESO, ENGI_PLATE, ENGI_CORDE, ENGI_RING};   /* 11:
                                                  * NOISE (engines.c ENGINES); 4 5 6 PLATE CORDE RING (05/11) */
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
    alt_rmb[c] = pr->rumble;                     /* (its RUMBLE: the KICK's, 04/11) */
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
    uint32_t i = (uint32_t)(((int32_t)au.pre[c] + s % (int32_t)ALT_NPRE + (int32_t)ALT_NPRE) % (int32_t)ALT_NPRE), n;
    for (n = 0; ALT_PRE[i].hide && n < ALT_NPRE; n++)   /* (a hidden one: the next in the turn's way) */
        i = (uint32_t)(((int32_t)i + (s < 0 ? -1 : 1) + (int32_t)ALT_NPRE) % (int32_t)ALT_NPRE);
    alt_load_sound(c, i);
    alt_say(ALT_PRE[i].name);
}

#include "alter_store.c"

/* the SAVE page's four tabs (05/11): SCENES A, SCENES B, PATTERNS A, PATTERNS B. A: NEW, the factory ones, USER A01..A16;
 * B: USER B01..B16. A tab's row r as a row of alt_row_* */
#define ALT_ROWS_A (1u + ALT_NFACT + 16u)
static uint32_t alt_tab_n(uint32_t tab) { return tab & 1u ? 16u : ALT_ROWS_A; }
static uint32_t alt_tab_row(uint32_t tab, uint32_t r) { return tab & 1u ? ALT_ROWS_A + r : r; }
/* the NAME page (Romain 05/11): SAVE held on a USER row opens it; KNOB 1 2 3 the three characters, SAVE stores, HOME
 * leaves (nothing stored) */
static const char ALT_NM_CH[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
#define ALT_NM_N ((int32_t)sizeof ALT_NM_CH - 1)
static struct { uint8_t open, t, r, ch[3]; } anm;
static uint32_t alt_nm_idx(char c)               /* a character's place in ALT_NM_CH (else the space) */
{
    uint32_t k;
    for (k = 0; k < (uint32_t)ALT_NM_N; k++)
        if (ALT_NM_CH[k] == c)
            return k;
    return 0;
}
static void alt_nm_open(uint32_t t, uint32_t r)  /* its NAME, or one to start from: the first sound's 3 letters (a
                                                  * scene), the three circles' HITS (a pattern) */
{
    uint32_t u = (r - 1u - ALT_NFACT) % ALT_NUSER, k, used = t ? alt_used_p[u] : alt_used_s[u];
    const char *nm = t ? alt_nm_p[u] : alt_nm_s[u];
    anm.open = 1;
    anm.t = (uint8_t)t;
    anm.r = (uint8_t)r;
    for (k = 0; k < 3u; k++) {
        char c;
        if (used && nm[0])
            c = nm[k];
        else if (t) {
            uint32_t h = au.cfg[k].hits;
            c = (char)(h > 9u ? 'A' + (h - 10u) : '0' + h);
        } else {
            const char *pn = ALT_PRE[au.pre[0] % ALT_NPRE].name;
            uint32_t j;
            for (j = 0; j < k && pn[j]; j++)
                ;
            c = pn[j] ? pn[j] : ' ';
        }
        anm.ch[k] = (uint8_t)alt_nm_idx(c);
    }
}

/* ---- SONG (Romain 05/11; the audio side: alter_song.c) ---------------------------------------------------------------
 *   (Romain 05/11, made simpler: everything on the three keys, SAVE to store)
 *   C D E high (white)     tapped: that slot plays from the next bar (the chain stops); empty: nothing
 *   SAVE held + C D E      what plays now stored in that slot (and on the flash; the SAVE page it opened closes)
 *   C held + E, D tapped   a chain C > E > D (each link 4 bars), from the next bar, looping; the held key let go: it goes on
 *   ARP tapped             the chain on (from its first link, at the next bar) / off (the slot playing goes on)
 *   ARP held 0.5 s         the SONG page: SELECT a link, KNOB 1 its slot, KNOB 2 its bars (1 2 4 8), KNOB 3 the links, KNOB 4 on/off
 * On the flash in objects 10..13 (storage.c OBJ_ALTSONG0: the slots, then the chain), not in the scenes. At power-on the
 * chain is off. */
#define QP_SONG 16u                               /* (ARP held: the SONG page) */
static void alt_menu_set(uint32_t q);
#define ALT_OBJ_SONG 10u                          /* (storage.c OBJ_ALTSONG0) */
#define SG_MAGIC_C 0x43544C41u                    /* "ALTC": the chain */
#define SG_DEF_BARS 2u                            /* (a new link: 4 bars) */
static struct {
    uint8_t wr, bn, buf[SG_MAXL];                 /* a slot's key held (+ 1): a chain being written from it */
    uint8_t chain[SG_MAXL], n;                    /* the chain (the UI's copy) */
    uint8_t q;                                    /* the slot asked for (+ 1), until it plays */
    uint8_t row, top, dirty;                      /* the SONG page: its link, its first row shown; the chain to store */
    uint32_t done, arp_t0;                        /* sg_done_cnt seen; ARP's press (fm1_ms | 1), 0 = up */
    uint8_t arp_held;                             /* ARP's hold has opened the page */
    char msg[48];
} asg;
static uint8_t asg_save_held;                     /* SAVE still held since its press opened the SAVE page */
static struct { uint32_t magic; uint8_t n, rsv[3]; uint8_t chain[SG_MAXL]; } asg_rec __attribute__((section(".pool")));
_Static_assert(sizeof(sg_slot_t) <= ST_PAYLOAD_MAX_X, "a SONG slot fits an object");

static void alt_sg_push(void)                    /* the chain to the audio side */
{
    uint32_t i;
    fm1_irq_off();
    for (i = 0; i < SG_MAXL; i++)
        sg_chain[i] = asg.chain[i];
    sg_n = asg.n;
    if (!asg.n)
        sg_on = 0;
    fm1_irq_on();
}
static void alt_sg_chain_save(void)
{
    uint32_t i;
    asg_rec.magic = SG_MAGIC_C;
    asg_rec.n = asg.n;
    for (i = 0; i < SG_MAXL; i++)
        asg_rec.chain[i] = asg.chain[i];
    (void)st_save(ALT_OBJ_SONG + SG_NSLOT, &asg_rec, sizeof asg_rec);
    asg.dirty = 0;
}
static void alt_sg_boot(void)                    /* the slots and the chain from the flash (or none) */
{
    uint32_t s, i;
    for (s = 0; s < SG_NSLOT; s++)
        if (st_load(ALT_OBJ_SONG + s, &sg_slot[s], sizeof sg_slot[s]) != (int)sizeof sg_slot[s] ||
            sg_slot[s].magic != SG_MAGIC || !sg_slot[s].used)
            for (i = 0; i < sizeof sg_slot[s]; i++)
                ((uint8_t *)&sg_slot[s])[i] = 0;
    asg.n = 0;
    if (st_load(ALT_OBJ_SONG + SG_NSLOT, &asg_rec, sizeof asg_rec) == (int)sizeof asg_rec && asg_rec.magic == SG_MAGIC_C) {
        asg.n = (uint8_t)(asg_rec.n > SG_MAXL ? SG_MAXL : asg_rec.n);
        for (i = 0; i < SG_MAXL; i++)
            asg.chain[i] = (uint8_t)((asg_rec.chain[i] & 3u) % SG_NSLOT | (asg_rec.chain[i] & 12u));
    }
    asg.done = sg_done_cnt;
    alt_sg_push();
}
static int alt_sg_store(uint32_t s)              /* what plays now into slot s; 0: the flash failed */
{
    static uint8_t rec[AS_NCIRC][AR_PACK] __attribute__((section(".pool")));   /* (RAM is short: the pool) */
    static as_circ_t cc[AS_NCIRC] __attribute__((section(".pool")));
    sg_slot_t *S = &sg_slot[s % SG_NSLOT];
    uint32_t c, k;
    for (c = 0; c < AS_NCIRC; c++)
        ar_export(c, rec[c]);                    /* (irq off on its own) */
    fm1_irq_off();
    for (c = 0; c < AS_NCIRC; c++)
        cc[c] = alt_seq.c[c];
    fm1_irq_on();
    for (c = 0; c < AS_NCIRC; c++)
        cc[c].cfg = cc[c].next = au.cfg[c];      /* (the settings as asked: the mutes too) */
    fm1_irq_off();                               /* (the audio side may take this slot at a bar line: whole) */
    S->magic = SG_MAGIC;
    S->used = 1;
    for (c = 0; c < AS_NCIRC; c++) {
        S->c[c] = cc[c];
        for (k = 0; k < AS_MAXLEN; k++)
            S->fxr[c][k] = alt_fxr[c][k];
        for (k = 0; k < AR_PACK; k++)
            S->rec[c][k] = rec[c][k];
    }
    fm1_irq_on();
    return st_save(ALT_OBJ_SONG + s, S, sizeof *S) == 0;
}
static void alt_sg_ask(uint32_t s)               /* slot s from the next bar (the chain stops) */
{
    fm1_irq_off();
    sg_on = 0;
    sg_ask = (uint8_t)(s + 1u);
    sg_ask_cnt++;
    fm1_irq_on();
    asg.q = (uint8_t)(s + 1u);
}
static void alt_sg_on(int on)
{
    fm1_irq_off();
    sg_on = (uint8_t)(on && asg.n);
    if (sg_on)
        sg_start_cnt++;
    fm1_irq_on();
}
static const char *alt_sg_chain_text(const uint8_t *ch, uint32_t n, const char *head)   /* "CHAIN A C B" */
{
    uint32_t i, j = 0;
    while (*head && j < sizeof asg.msg - 1u)
        asg.msg[j++] = *head++;
    for (i = 0; i < n && j + 2u < sizeof asg.msg; i++) {
        asg.msg[j++] = ' ';
        asg.msg[j++] = (char)('A' + (ch[i] & 3u));
    }
    asg.msg[j] = 0;
    return asg.msg;
}
/* once a frame: a slot taken by the audio side (the UI's settings follow it); OCT- let go after writing a chain; the
 * chain edited on the SONG page stored once the page is left */
static void alt_sg_sync(void)
{
    uint32_t c;
    if (sg_done_cnt != asg.done) {
        uint32_t s = sg_play ? sg_play - 1u : 0u;
        asg.done = sg_done_cnt;
        asg.q = 0;
        for (c = 0; c < AS_NCIRC; c++) {
            as_cfg_t n = sg_slot[s % SG_NSLOT].c[c].next;
            n.dose = au.cfg[c].dose, n.root = au.cfg[c].root, n.chord = au.cfg[c].chord;
            if (!n.len)
                n.len = 16;
            au.cfg[c] = n;
        }
    }
    if (asg.wr && !((ALT_NOTES >> (19u + 2u * (asg.wr - 1u))) & 1u)) {   /* the first key let go: the chain
                                                  * written (two links or more) plays */
        uint32_t i;
        asg.wr = 0;
        if (asg.bn >= 2u) {
            for (i = 0; i < SG_MAXL; i++)
                asg.chain[i] = i < asg.bn ? asg.buf[i] : 0u;
            asg.n = asg.bn;
            asg.row = 0;
            asg.top = 0;
            alt_sg_push();
            alt_sg_on(1);
            alt_sg_chain_save();
            alt_say(alt_sg_chain_text(asg.chain, asg.n, "CHAIN"));
        }
    }
    if (asg.dirty && au.mn != QP_SONG)
        alt_sg_chain_save();
}
/* SAVE held + a slot's key: what plays stored there */
static void alt_sg_save_key(uint32_t s)
{
    static const char *const SAVED[SG_NSLOT] = {"A SAVED", "B SAVED", "C SAVED"};
    alt_say(alt_sg_store(s) ? SAVED[s] : "SAVE FAILED");
}
/* a slot's key went down (s: 0..2): alone, that slot from the next bar; another slot's key held, a link more */
static void alt_sg_key(uint32_t s)
{
    static const char *const NAME[SG_NSLOT] = {"SONG A", "SONG B", "SONG C"};
    if (!sg_slot[s].used) {
        alt_say("EMPTY");
        return;
    }
    if (asg.wr) {                                /* the first key held: the chain */
        if (asg.bn < SG_MAXL)
            asg.buf[asg.bn++] = (uint8_t)(s | SG_DEF_BARS << 2);
        alt_say(alt_sg_chain_text(asg.buf, asg.bn, "CHAIN"));
        return;
    }
    asg.wr = (uint8_t)(s + 1u);                  /* (held: the first of a chain, if another key follows) */
    asg.bn = 1;
    asg.buf[0] = (uint8_t)(s | SG_DEF_BARS << 2);
    alt_sg_ask(s);
    alt_say(NAME[s]);
}
/* ARP: tapped the chain on / off; held 0.5 s the SONG page (pressed: this frame's presses) */
static void alt_sg_arp(uint32_t pressed, uint32_t hold_ms)
{
    uint32_t dn = (ALT_BTNS >> panel.btn[B_ARP]) & 1u;
    if ((pressed >> panel.btn[B_ARP]) & 1u) {
        asg.arp_t0 = fm1_ms | 1u;
        asg.arp_held = 0;
    } else if (dn && asg.arp_t0 && !asg.arp_held && (int32_t)(fm1_ms - asg.arp_t0) >= (int32_t)hold_ms) {
        asg.arp_held = 1;
        au.qp = QP_NONE;
        au.lockc = 0;
        au.scene_open = 0;
        alt_menu_set(QP_SONG);
        asg.row = sg_link < asg.n ? sg_link : 0u;
        alt_say("SONG");
    } else if (!dn && asg.arp_t0) {
        if (!asg.arp_held) {
            if (!asg.n)
                alt_say("NO CHAIN");
            else {
                alt_sg_on(!sg_on);
                alt_say(sg_on ? "CHAIN ON" : "CHAIN OFF");
            }
        }
        asg.arp_t0 = 0;
    }
}
/* the SONG page: SELECT the link, KNOB 1 its slot, KNOB 2 its bars, KNOB 3 how many links, KNOB 4 the chain on / off */
static void alt_sg_turn(uint32_t i, int32_t s)
{
    uint32_t r = asg.row;
    if (i == 3u) {                               /* KNOB 4: the chain on (right) / off (left) */
        if (s > 0 && !sg_on && asg.n)
            alt_sg_on(1);
        else if (s < 0 && sg_on)
            alt_sg_on(0);
        return;
    }
    if (i == 2u) {
        int32_t n = clamp((int32_t)asg.n + (s > 0 ? 1 : -1), 0, (int32_t)SG_MAXL);
        if (n > (int32_t)asg.n)                  /* (a link more: as the last one, or A) */
            asg.chain[n - 1] = asg.n ? asg.chain[asg.n - 1u] : (uint8_t)(SG_DEF_BARS << 2);
        asg.n = (uint8_t)n;
        if (asg.row >= asg.n)
            asg.row = (uint8_t)(asg.n ? asg.n - 1u : 0u);
    } else if (r < asg.n && i < 2u) {
        uint32_t sl = asg.chain[r] & 3u, b = (asg.chain[r] >> 2) & 3u;
        if (i == 0u)
            sl = (uint32_t)clamp((int32_t)sl + (s > 0 ? 1 : -1), 0, (int32_t)SG_NSLOT - 1);
        else
            b = (uint32_t)clamp((int32_t)b + (s > 0 ? 1 : -1), 0, 3);
        asg.chain[r] = (uint8_t)(sl | b << 2);
    } else
        return;
    asg.dirty = 1;
    alt_sg_push();
}
static void alt_sg_select(int32_t s)
{
    asg.row = (uint8_t)clamp((int32_t)asg.row + s, 0, asg.n ? (int32_t)asg.n - 1 : 0);
}

/* SEQ held: knob i of the chosen circle, s detents */
/* the rhythm's four knobs, by the circle's engine (alter_seq.c AS_T_*) */
static const char *const ALT_RHY_NAME[AS_NTYPE][4] = {
    {"LEN", "HITS", "ROT", "PROB"}, {"LEN", "LOCK", "DENS", "RANGE"},
};
/* SEQ: knob i of the chosen circle's rhythm, s detents (LEN HITS ROT PROB / LEN LOCK DENS RANGE) */
static void alt_edit_circle(uint32_t i, int32_t s)
{
    as_cfg_t *c = &au.cfg[au.sel];
    uint32_t t = c->type % AS_NTYPE, mx = AS_PLAYLEN;   /* (16 steps at most: Romain 04/11) */
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
    if (id == QK_ENG)                            /* (a sound with a RUMBLE: KNOB 4 is it, 04/11; the KICK's MODE stays) */
        return i == 3u && ALT_PRE[au.pre[au.sel % AS_NCIRC] % ALT_NPRE].rumble ? QK_RUMBLE : e->knob[i];
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
    case QK_PROG:
        alt_seq.prog[au.sel % AS_NCIRC] = (uint8_t)clamp((int32_t)alt_seq.prog[au.sel % AS_NCIRC] + (s > 0 ? 1 : -1), 0,
                                                         AS_NPROG - 1);
        return;
    case QK_CHORD:
        c->chord = as_ch_step(c->chord, s);
        alt_post_circle(au.sel);
        return;
    case QK_BPM:
        song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + s, GP[G_BPM].min, GP[G_BPM].max);
        return;
    case QK_RUMBLE:
        alt_rmb[au.sel % AS_NCIRC] = (uint8_t)clamp((int32_t)alt_rmb[au.sel % AS_NCIRC] + s * 2, 0, 127);
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
/* circle c's RATCH lane drawn at random (Romain 04/11): no ratchet as a rule; with pct % chance ONE step only, x2
 * mostly, x3 almost never (1 in 16), never x4 */
static void alt_rat_draw(as_circ_t *sc, uint32_t pct)
{
    uint32_t k, len = sc->cfg.len ? sc->cfg.len : 16u;
    for (k = 0; k < AS_NLANE; k++)
        sc->srat[k] = 0;
    if (alt_cr(100u) < pct)
        sc->srat[alt_cr(len < AS_NLANE ? len : AS_NLANE)] = (uint8_t)(alt_cr(16u) ? 1u : 2u);
}
static uint32_t alt_crng;
static uint32_t alt_cr(uint32_t n);
/* a random VELO / CHANCE lane step (Romain 04/11, alter_seq.c as_vpct / as_cpct): mostly the normal 96 (100 %), 72,
 * 48; the chance mostly 127, fewer sure steps the more hits the circle has (25 % at the least) */
/* a TURING drawn at random (Romain 05/11): LEN 5..16, nearer 16 than 5 (the larger of two draws), DENS 40..60 %,
 * LOCK 50..75 %, RANGE 1..4 (the knobs show LOCK and DENS as % of 127) */
static uint8_t alt_len_draw(void)               /* a LEN 5..16, nearer 16 (the larger of two draws; Romain 10/11: CHAOS
                                                  * TOTAL and OCT- + DICE draw one too) */
{
    uint32_t a = 5u + alt_cr(12u), b = 5u + alt_cr(12u);
    return (uint8_t)(a > b ? a : b);
}
static void alt_len_fit(as_cfg_t *cf)            /* (HITS, ROT, FROT kept inside a new LEN) */
{
    cf->hits = (uint8_t)(cf->hits > cf->len ? cf->len : cf->hits);
    cf->rot = (uint8_t)(cf->rot % cf->len);
    cf->frot = (uint8_t)(cf->frot % cf->len);
}
static void alt_tur_draw(as_cfg_t *cf)
{
    cf->len = alt_len_draw();
    cf->tdens = (uint8_t)(51u + alt_cr(26u));
    cf->tlock = (uint8_t)(64u + alt_cr(32u));
    cf->trange = (uint8_t)(1u + alt_cr(4u));
    cf->hits = (uint8_t)(cf->hits > cf->len ? cf->len : cf->hits);
    cf->rot = (uint8_t)(cf->rot % cf->len);
    cf->frot = (uint8_t)(cf->frot % cf->len);
}
static uint8_t alt_rvel(void) { return (uint8_t)(AS_VEL[AS_NORMAL] * as_vpct(alt_cr(1024u)) / 100u); }
static uint8_t alt_rch(uint32_t hits) { uint32_t a = alt_cr(1024u); return (uint8_t)(127u * as_cpct(a, alt_cr(1024u), hits) / 100u); }
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
            sc->svel[k] = alt_rvel();
        else if (ln == LN_CHANCE)
            sc->chance[k] = sc->chance[k + AS_NLANE] = alt_rch(sc->cfg.hits);
        else if (ln == LN_DLY)
            sc->sdly[k] = (uint8_t)v;
        else if (ln == LN_REV)
            sc->srev[k] = (uint8_t)v;
        else if (ln == LN_PITCH)
            sc->spitch[k] = (int8_t)v;
        else
            sc->srat[k] = (uint8_t)v;
    }
    if (ln == LN_RATCH)                          /* (RATCH: one step only, as every draw) */
        alt_rat_draw(sc, 100u);
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
            if (b < LN_N && (ALT_BTNS & both) == both) {   /* OCT- + OCT+ held: that lane back to flat */
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
        if ((ALT_BTNS >> panel.btn[ALT_QP_BTN[q]]) & 1u)
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

/* LE HASARD (Romain 03/11) : un son tiré n'est jamais un son RESO (« pas dans le style », brouillon dans le hasard ;
 * RESO reste choisissable à la main dans PRESETS) */
/* SYSTEM (HOME held, 04/11): what the random may draw, kept in the settings (alt_sys, a byte row no engine uses:
 * favorites.factory[13], engine 13 retired): [0] ALT_SYS_OK once set, [1] the engines it may draw (bit = ALT_ENGS index;
 * default ALTER PHYS NOISE PLATE CORDE RING, RESO off as Romain asked 03/11), [2] the envelope modes CHAOS TOTAL may draw (bit = AM_*:
 * FREE PERC STAB), [3] CLOCK + 1, [4] MIDI IN + 1, [5] the MIXER's MASTER + 1, [6] TUNE's row value + 1, [7] 1: PLATE
 * CORDE RING (05/11) known */
#define alt_sys (favorites.factory[13])
#define ALT_SYS_OK 0xA7u
static void alt_sys_init(void)
{
    if (alt_sys[0] != ALT_SYS_OK) {
        uint32_t k;
        for (k = 0; k < 32u; k++)
            alt_sys[k] = 0;
        alt_sys[0] = ALT_SYS_OK;
        alt_sys[1] = 0x77u;                      /* ALTER PHYS NOISE, PLATE CORDE RING (RESO off) */
        alt_sys[7] = 1u;
        alt_sys[2] = 7u;                         /* FREE PERC STAB */
    }
    if (alt_sys[7] != 1u) {                      /* (stored before PLATE CORDE RING, 05/11: they join the random) */
        alt_sys[1] |= 0x70u;
        alt_sys[7] = 1u;
    }
    if (alt_sys[3])
        song.g[G_CLOCK] = (int16_t)(alt_sys[3] - 1u);
    if (alt_sys[4])
        song.g[G_ROUTE] = (int16_t)(alt_sys[4] - 1u);
    if (alt_sys[5])
        alt_mgain = (uint8_t)(alt_sys[5] - 1u);
    if (alt_sys[6])                              /* TUNE (05/11) */
        menu_put(MI_TUNE, (uint32_t)(alt_sys[6] - 1u) % TUNE_N);
}
static uint8_t alt_die_w[AS_NCIRC];               /* what each DICE key draws, from the OCT held at its press */
static uint32_t alt_rand_pre(void)
{
    uint32_t pi = 0, n, ok = alt_sys[0] == ALT_SYS_OK && (alt_sys[1] & 127u) ? alt_sys[1] & 127u : 0x77u;
    for (n = 0; n < 200u; n++) {
        pi = alt_cr(ALT_NPRE);
        if (((ok >> (ALT_PRE[pi].eng % ALT_NENG)) & 1u) && !ALT_PRE[pi].hide)
            break;
    }
    return pi;
}
static uint32_t alt_rand_mode(void)              /* an envelope mode the SYSTEM menu lets the random draw */
{
    uint32_t ok = alt_sys[0] == ALT_SYS_OK && (alt_sys[2] & 7u) ? alt_sys[2] & 7u : 7u, m, n;
    for (n = 0; n < 32u; n++)
        if ((ok >> (m = alt_cr(3u))) & 1u)
            return m;
    return AM_FREE;
}

/* BUDGET D'EFFETS du hasard (Romain 03/11 : « certaines peuvent être hautes, mais tout ne peut pas être haut ») :
 * un cercle tiré additionne 6 intensités (256 = au plus fort) : distorsions DIST et INSERT (sa dose x son MIX),
 * échos envoi DELAY (pesé par le FEEDBACK partagé) et envoi REVERB, mouvements LFO (profondeurs, 20 = fort) et MTX
 * (|AMT|, 25 = fort). Au-delà de ALT_FXB (1,5 effet au plus fort), la plus forte reste telle quelle et les autres
 * baissent ensemble pour tenir dans le budget (les couloirs DELAY/REVERB des pas suivent leur envoi).
 * Seulement le hasard (CHAOS, CHAOS TOTAL) : les knobs à la main restent libres. Appelé interruptions coupées. */
#define ALT_FXB 384
static int32_t alt_fxb_n(int32_t v, int32_t full)
{
    v = v < 0 ? -v : v;
    return v >= full ? 256 : v * 256 / full;
}
static int32_t alt_fxb_vals(uint32_t c, int32_t *v);
static int32_t alt_fxb_total(uint32_t c)         /* (les tests : web_alter_peek) */
{
    int32_t v[6];
    return alt_fxb_vals(c, v);
}
static int32_t alt_fxb_vals(uint32_t c, int32_t *v)
{
    const track_t *t = &trk[c];
    const as_circ_t *sc = &alt_seq.c[c];
    int32_t tot = 0, fb = song.g[G_DFDBK], ld = 0, lr = 0;
    uint32_t k, i;
    for (k = 0; k < AS_NLANE; k++) {             /* (les couloirs DELAY / REVERB s'ajoutent à l'envoi à chaque coup :
                                                  * l'envoi de base + son pas le plus fort) */
        ld = sc->sdly[k] > ld ? sc->sdly[k] : ld;
        lr = sc->srev[k] > lr ? sc->srev[k] : lr;
    }
    v[0] = alt_fxb_n(t->p[P_DIST], 127);
    v[1] = t->p[P_ITYPE] ? alt_fxb_n(t->p[P_IA], 127) * alt_fxb_n(t->p[P_IMIX], 127) >> 8 : 0;
    v[2] = alt_fxb_n(t->p[P_DLY] - alt_applied[c][12] + ld, 127) * (128 + clamp(fb, 0, 120) * 128 / 120) >> 8;   /* (FEEDBACK 0: x 0,5) */
    v[3] = alt_fxb_n(t->p[P_REV] - alt_applied[c][13] + lr, 127);
    v[4] = 0;
    for (k = 0; k < CL_N; k++)
        v[4] += alt_fxb_n(alt_cl[c][k].depth, 20);
    v[5] = 0;
    for (k = 0; k < NMSLOT; k++)
        if (t->p[P_M1SRC + 3u * k])
            v[5] += alt_fxb_n(t->p[P_M1AMT + 3u * k], 25);
    for (i = 0; i < 6u; i++) {
        v[i] = v[i] > 256 ? 256 : v[i];
        tot += v[i];
    }
    return tot;
}
static int alt_fx_budget1(uint32_t c);
static void alt_fx_budget(uint32_t c)           /* (jusqu'à 4 passes : un envoi + son couloir plafonné à 127 baisse
                                                  * moins que prévu) */
{
    uint32_t it;
    for (it = 0; it < 4u && alt_fx_budget1(c); it++)
        ;
}
static int alt_fx_budget1(uint32_t c)
{
    track_t *t = &trk[c];
    as_circ_t *sc = &alt_seq.c[c];
    int32_t v[6], tot, big = 0, f;
    uint32_t k, i;
    tot = alt_fxb_vals(c, v);
    for (i = 1; i < 6u; i++)
        if (v[i] > v[big])
            big = (int32_t)i;
    if (tot <= ALT_FXB || tot - v[big] <= 0)
        return 0;
    f = (ALT_FXB - v[big]) * 256 / (tot - v[big]);    /* (0..255 : ce qui reste du budget, pour les autres) */
#define ALT_FXB_SC(x) ((x) = (int16_t)((x) * f / 256))
    if (big != 0)
        ALT_FXB_SC(t->p[P_DIST]);
    if (big != 1)
        ALT_FXB_SC(t->p[P_IMIX]);
    if (big != 2) {
        t->p[P_DLY] = (int16_t)((t->p[P_DLY] - alt_applied[c][12]) * f / 256 + alt_applied[c][12]);
        for (k = 0; k < AS_NLANE; k++)
            sc->sdly[k] = (uint8_t)(sc->sdly[k] * f / 256);
    }
    if (big != 3) {
        t->p[P_REV] = (int16_t)((t->p[P_REV] - alt_applied[c][13]) * f / 256 + alt_applied[c][13]);
        for (k = 0; k < AS_NLANE; k++)
            sc->srev[k] = (uint8_t)(sc->srev[k] * f / 256);
    }
    if (big != 4)
        for (k = 0; k < CL_N; k++)
            alt_cl[c][k].depth = (int8_t)(alt_cl[c][k].depth * f / 256);
    if (big != 5)
        for (k = 0; k < NMSLOT; k++)
            ALT_FXB_SC(t->p[P_M1AMT + 3u * k]);
#undef ALT_FXB_SC
    return 1;
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
        uint32_t pi = alt_rand_pre(), e = ALT_ENGS[ALT_PRE[pi].eng % ALT_NENG], ei;
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
        ei = 5u + alt_cr(12u);                   /* the rhythm: LEN 5..16, anything in it, at once */
        cf->len = (uint8_t)ei;
        cf->hits = (uint8_t)(1u + alt_cr(ei));
        cf->rot = (uint8_t)alt_cr(ei);
        cf->frot = (uint8_t)alt_cr(ei);
        cf->prob = (uint8_t)(80u + alt_cr(48u));
        cf->dose = 127;
        cf->chord = AS_CH_OFF;                   /* (le hasard : jamais d'accord, Romain 03/11) */
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
                uint8_t ch = (pick & 2u) ? alt_rch(cf->hits) : 127u;   /* (04/11: the random's VELO and CHANCE) */
                sc->svel[k] = (pick & 1u) ? alt_rvel() : AS_VEL[AS_NORMAL];
                sc->chance[k] = sc->chance[k + AS_NLANE] = ch;
                sc->sdly[k] = (pick & 4u) && alt_cr(2u) ? (uint8_t)alt_cr(52u) : 0u;   /* (0..51: 40 %) */
                sc->srev[k] = (pick & 8u) && alt_cr(2u) ? (uint8_t)alt_cr(52u) : 0u;
                sc->spitch[k] = (int8_t)((pick & 16u) && alt_cr(2u) ? (int32_t)alt_cr(15u) - 7 : 0);
            }
            alt_rat_draw(sc, pick & 32u ? 50u : 0u);   /* (RATCH: one step at most, a fifth of the circles) */
        }
        fm1_irq_off();
        alt_fx_budget(c);                        /* (le budget d'effets du hasard) */
        fm1_irq_on();
        alt_post_now |= (uint8_t)(1u << c);
        alt_post_circle(c);
        alt_roll_w[c] = AS_DW_ALL;
        alt_roll_cnt[c]++;                       /* and the die at full DOSE */
        alt_cal(c);                              /* (its level aligned on its first hits) */
    }
    alt_say("CHAOS");
}

/* « AU HASARD » (the companion page, alter_link.c LA_RANDOM; MUTE 1+2+3 until 05/11): the chosen circle's LEN (4..16) and HITS drawn, at once, and its VELO, CHANCE and
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
                sc->svel[k] = alt_rvel();
            else if (LN[i] == LN_CHANCE)
                sc->chance[k] = sc->chance[k + AS_NLANE] = alt_rch(au.cfg[au.sel % AS_NCIRC].hits);
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
        if (cf->type == AS_T_TURING)
            alt_tur_draw(cf);
        cf->chord = AS_CH_OFF;                   /* (le hasard : jamais d'accord, Romain 03/11) */
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
static const uint8_t ALT_DT[4] = {2, 1, 10, 0};   /* (the delay's times: params.c N_DTIME; 1/16 1/8 1/8D 1/4, Romain 04/11) */
static const uint8_t ALT_TC_G[9] = {G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX, G_RTYPE, G_RSIZE, G_RDAMP, G_DMODE, G_DTAPE};
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
    uint8_t rmb[AS_NCIRC];                       /* (the RUMBLE, 04/11) */
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
        alt_tcu.rmb[c] = alt_rmb[c];
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
        alt_rmb[c] = alt_tcu.rmb[c];
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
    song.g[G_DMODE] = (int16_t)alt_cr(3u);              /* ECHO PING-PONG GHOST, TAPE (04/11) */
    song.g[G_DTAPE] = (int16_t)alt_cr(100u);
    song.g[G_RTYPE] = (int16_t)alt_cr(4u);   /* SHORT GATED HALL CLOUD */
    song.g[G_RSIZE] = (int16_t)alt_cr(128u);
    song.g[G_RDAMP] = (int16_t)alt_cr(128u);
    afx_edit |= 1u << AFX_S_DLY | 1u << AFX_S_REV;
    for (c = 0; c < AS_NCIRC; c++) {
        track_t *t = &trk[c];
        uint32_t pi = alt_rand_pre(), e = ALT_ENGS[ALT_PRE[pi].eng % ALT_NENG], n;
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
        alt_emode[c] = (uint8_t)alt_rand_mode(); /* FREE PERC STAB (those SYSTEM allows), and its settings */
        for (k = 0; k < AE_N; k++)
            alt_envp[c][k] = (uint8_t)alt_cr(128u);
        cf->type = (uint8_t)alt_cr(AS_NTYPE);    /* the rhythm: EUCLID or TURING, anything in it, at once */
        n = alt_len_draw();                      /* (5..16, nearer 16: Romain 10/11) */
        cf->len = (uint8_t)n;
        cf->hits = (uint8_t)(1u + alt_cr(n));
        cf->rot = (uint8_t)alt_cr(n);
        cf->frot = (uint8_t)alt_cr(n);
        cf->prob = (uint8_t)(64u + alt_cr(64u));
        cf->tlock = (uint8_t)alt_cr(128u);
        cf->tdens = (uint8_t)alt_cr(128u);
        cf->trange = (uint8_t)alt_cr(15u);
        if (cf->type == AS_T_TURING)
            alt_tur_draw(cf);
        cf->dose = 127;
        cf->chord = AS_CH_OFF;                   /* (le hasard : jamais d'accord, Romain 03/11) */
        cf->root = (uint8_t)(24u + alt_cr(49u)); /* C1 .. C5 */
        for (k = 0; k < AS_NLR; k++)
            sc->rmask[k] = 0;
        for (k = 0; k < AS_NLANE; k++) {         /* the six lanes, every step (CHANCE: something still plays) */
            sc->svel[k] = (uint8_t)(1u + alt_cr(127u));
            sc->chance[k] = sc->chance[k + AS_NLANE] = (uint8_t)(20u + alt_cr(108u));
            sc->sdly[k] = (uint8_t)alt_cr(128u);
            sc->srev[k] = (uint8_t)alt_cr(128u);
            sc->spitch[k] = (int8_t)((int32_t)alt_cr(15u) - 7);
        }
        alt_rat_draw(sc, 30u);                   /* (RATCH: one step at most, rarely) */
        fm1_irq_off();
        alt_fx_budget(c);                        /* (le budget d'effets du hasard) */
        fm1_irq_on();
        alt_post_now |= (uint8_t)(1u << c);
        alt_post_circle(c);
        alt_roll_w[c] = AS_DW_ALL;
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
 * scales), GLO (the MIXER, 04/11); HOME held: SYSTEM (04/11) */
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
    au.mn = (uint8_t)(q == QP_ENV || q == QP_EDIT || q == QP_SCL || q == QP_GLO || q == QP_SYS || q == QP_SONG ? q : 0u);
    au.qp = (uint8_t)(q == QP_ENV || q == QP_SCL ? q : QP_NONE);   /* (ENV, SCL: their quick page's knobs, drawn big) */
    au.step_down = 0;
    au.dd = 0;
    au.set_row = 0;
}
static const char *alt_menu_name(uint32_t q)
{
    return q == QP_SEQ ? "SEQ" : q == QP_FX ? "FX" : q == QP_LFO ? "LFO" : q == QP_ENV ? "ENVELOPE" :
           q == QP_EDIT ? "DEEP" : q == QP_SCL ? "SCALES" : q == QP_GLO ? "MIXER" : q == QP_SYS ? "SYSTEM" : q == QP_SONG ? "SONG" : "";
}

static void afp_input(uint32_t pressed);                     /* the FX page (alter_fxpage.c, with the screen below) */
static void alt_deep_input(void);                /* DEEP (with the screens below) */
static void alt_mix_turn(uint32_t i, int32_t s);  /* the MIXER */
static void alt_mx_nav(void);                    /* the MIXER's tabs (the CONSOLE) */
static void alt_sys_input(void);                  /* SYSTEM (HOME held) */
static int alter_ui_input(uint32_t pressed)
{
    uint32_t keys, down, k, i, ours = alt_ours(), q, octs, both;
    int32_t s;
    alm_poll();                                  /* MIDI LEARN: the settings moved, the keys and buttons held */
    pressed |= alm_bdown;
    keys = ALT_NOTES;
    both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
    octs = (ALT_BTNS & both) == both;
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
    /* (OCT- + OCT+ + a page button opened Felucca's own pages until 05/11: gone, with them; Romain. OCT- + OCT+ held
     * 5 s is still the update mode: main.c, before any of this) */
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
        alt_sys_init();                          /* (SYSTEM's settings: CLOCK, MIDI IN, MASTER, the random's choices) */
        alt_select(song.sel < AS_NCIRC ? song.sel : 0u);
        alt_banks_load();                        /* the USER slots, from the flash */
        alt_sg_boot();                           /* SONG's slots and chain, from the flash */
        alt_load_scene(0);                       /* power-on: the first scene */
    }
    if (song.rec)                                /* (no recording in ALTER: an armed REC would count in and click) */
        song.rec = 0;
    alt_sg_sync();                               /* SONG: a slot taken, a chain written */
    alt_sg_arp(pressed, ALT_MENU_HOLD_MS);       /* ARP: the chain on / off, the SONG page */
    if (song.sel != au.sel && song.sel < AS_NCIRC)   /* (chosen elsewhere: TRACKS, the editor) */
        au.sel = alt_sel = song.sel;
    if ((pressed >> panel.btn[B_PLAY]) & 1u)
        transport_req = song.playing || seq_counting() ? 2u : 1u;
    {                                            /* REC (alter_rec.c): tap = overdub on / off; held 1 s = the chosen
                                                  * circle cleared; OCT- + OCT+ + REC = every circle cleared */
        uint32_t rd = (ALT_BTNS >> panel.btn[B_REC]) & 1u;
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
    {                                            /* HOME: the circles, no page; held 0.5 s: SYSTEM (04/11) */
        static uint32_t home_t0;
        uint32_t hd = (ALT_BTNS >> panel.btn[B_HOME]) & 1u;
        if ((pressed >> panel.btn[B_HOME]) & 1u) {
            int was = au.mn == QP_SYS;
            anm.open = 0;                        /* (the NAME page: left, nothing stored) */
            au.qp = QP_NONE;
            au.lockc = 0;
            au.scene_open = 0;
            alt_menu_set(0);
            home_t0 = was ? 0u : fm1_ms | 1u;   /* (tapped on SYSTEM: out, no new hold) */
        } else if (hd && home_t0 && !octs && (int32_t)(fm1_ms - home_t0) >= (int32_t)ALT_MENU_HOLD_MS) {
            alt_menu_set(QP_SYS);
            au.set_row = 0;
            alt_say("SYSTEM");
            home_t0 = 0;
        } else if (!hd)
            home_t0 = 0;
    }
    if (!((ALT_BTNS >> panel.btn[B_SAVE]) & 1u))
        asg_save_held = 0;
    if (((pressed >> panel.btn[B_SAVE]) & 1u) && !au.scene_open) {   /* SAVE: the SAVE page (its press opens it) */
        asg_save_held = 1;                       /* (held on: + C D E high stores a SONG slot) */
        au.scene_open = 1;
        au.save_t0 = 0;
        au.qp = QP_NONE;
        au.lockc = 0;
        alt_menu_set(0);
    } else if (au.scene_open && asg_save_held && ((ALT_BTNS >> panel.btn[B_SAVE]) & 1u) &&
               (keys & ~au.keys & ((1u << 19) | (1u << 21) | (1u << 23)))) {   /* SAVE held + C D E high: SONG's slot */
        uint32_t dk = keys & ~au.keys;
        alt_sg_save_key((dk >> 19) & 1u ? 0u : (dk >> 21) & 1u ? 1u : 2u);
        au.scene_open = 0;                       /* (the SAVE page it opened: closed) */
        asg_save_held = 0;
        au.keys = keys;
        return 1;
    } else if (au.scene_open && anm.open) {      /* the NAME page: SAVE stores (its release: nothing more) */
        if ((pressed >> panel.btn[B_SAVE]) & 1u) {
            uint32_t k;
            int rc;
            for (k = 0; k < 3u; k++)
                alt_nm_new[k] = ALT_NM_CH[anm.ch[k] % (uint32_t)ALT_NM_N];
            if (alt_nm_new[0] == ' ' && alt_nm_new[1] == ' ' && alt_nm_new[2] == ' ')
                alt_nm_new[0] = 0;               /* (three spaces: no NAME) */
            alt_nm_set = 1;
            rc = alt_row_save(anm.t, anm.r);
            alt_nm_set = 0;
            anm.open = 0;
            au.save_t0 = fm1_ms | 1u;
            au.save_done = 1;
            alt_say(rc > 0 ? "SAVED" : rc == 0 ? "PROTECTED" : "SAVE FAILED");
        }
    } else if (au.scene_open) {                  /* on it: a tap loads, held 1 s stores (a USER row: its NAME first) */
        uint32_t dn = (ALT_BTNS >> panel.btn[B_SAVE]) & 1u, t = au.tab >> 1, r = alt_tab_row(au.tab, au.row[au.tab]);
        if (((pressed >> panel.btn[B_SAVE]) & 1u)) {
            au.save_t0 = fm1_ms | 1u;
            au.save_done = 0;
        } else if (dn && au.save_t0 && !au.save_done && (int32_t)(fm1_ms - au.save_t0) >= (int32_t)ALT_SAVE_HOLD_MS) {
            au.save_done = 1;
            if (r > ALT_NFACT) {
                alt_nm_open(t, r);
                alt_say("NAME");
            } else
                alt_say("PROTECTED");
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
    if (au.scene_open && anm.open) {             /* the NAME page: KNOB 1 2 3 its characters */
        for (i = 0; i < 4u; i++)
            if ((s = panel_enc(EN_K1 + i)) != 0 && i < 3u)
                anm.ch[i] = (uint8_t)(((int32_t)anm.ch[i] + (s > 0 ? 1 : -1) + ALT_NM_N) % ALT_NM_N);
        (void)panel_enc(EN_SELECT);
        (void)panel_enc(EN_ALGO);
        (void)panel_enc(EN_PRESET);
        alt_hold = 0;
        au.keys = keys;
        return 1;
    }
    if (au.scene_open) {                         /* the SAVE page: SELECT picks the row, ALGORITHM the tab */
        if ((s = panel_enc(EN_SELECT)) != 0)
            au.row[au.tab] = (uint8_t)clamp((int32_t)au.row[au.tab] + s, 0, (int32_t)alt_tab_n(au.tab) - 1);
        if ((s = panel_enc(EN_ALGO)) != 0)
            au.tab = (uint8_t)clamp((int32_t)au.tab + (s > 0 ? 1 : -1), 0, 3);   /* (SCENES A B, PATTERNS A B) */
        for (i = 0; i < 4u; i++)
            (void)panel_enc(EN_K1 + i);
        (void)panel_enc(EN_PRESET);
        alt_hold = 0;
        au.keys = keys;
        return 1;
    }
    for (q = 1; q < QP_N; q++) {                 /* the page buttons: a tap the quick page, held 0.5 s the menu */
        uint32_t dn = (ALT_BTNS >> panel.btn[ALT_QP_BTN[q]]) & 1u, was = (au.qp_was >> q) & 1u;
        if (dn && !was) {
            au.qp_t0[q] = fm1_ms;
            au.lockc = 0;
            au.mhold &= (uint8_t)~(1u << q);
            if (alt_menu_cur() && alt_menu_cur() != q)
                alt_menu_set(0);                 /* (another page: the menu open closes) */
            alt_say(alt_qp_name(q));
        } else if (dn && alt_menu_cur() != q && !((au.mhold >> q) & 1u) &&
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
    if (au.mn == QP_EDIT) {                      /* DEEP (05/11; SETTINGS: a row of SYSTEM) */
        uint32_t dn = keys & ~au.keys, k;
        for (k = 0; k < 27u; k++)                /* (the black CIRCLE keys still choose the circle) */
            if ((dn >> k) & 1u && ALT_KEY[k].kind == AK_SEL)
                alt_select(ALT_KEY[k].arg);
        alt_deep_input();
        au.keys = keys;
        return 1;
    }
    if (au.mn == QP_SYS) {                       /* SYSTEM */
        alt_sys_input();
        au.keys = keys;
        return 1;
    }
    if (((ALT_BTNS >> panel.btn[B_SEQ]) & 1u) && (s = panel_enc(EN_ALGO)) != 0)
        alt_seq_type(s);                         /* SEQ held + ALGORITHM: the chosen circle's engine */
    if (au.seq_open) {                           /* the SEQ menu */
        alt_seq_input(keys);
        au.keys = keys;
        return 1;
    }
    q = alt_page();
    if (afp.open)                                /* the FX page: its encoders (the keys play on: mutes, dice, fills) */
        afp_input(pressed);
    if (au.mn == QP_GLO && !au.lockc)
        alt_mx_nav();                            /* the MIXER's tabs and rows (the CONSOLE) */
    for (i = 0; i < 4u; i++)
        if ((s = panel_enc(EN_K1 + i)) != 0 && !au.lockc)
        {
            if (au.mn == QP_GLO)
                alt_mix_turn(i, s);              /* the MIXER's faders */
            else if (au.mn == QP_SONG)
                alt_sg_turn(i, s);               /* the SONG page's links */
            else
                alt_turn(q, i, s);
        }
    if (au.mn == QP_SONG) {                      /* (the SONG page: SELECT its link; ALGORITHM, PRESET nothing) */
        if ((s = panel_enc(EN_SELECT)) != 0)
            alt_sg_select(s);
        (void)panel_enc(EN_ALGO);
        (void)panel_enc(EN_PRESET);
    }
    if ((s = panel_enc(EN_SELECT)) != 0 && !au.lockc)
        alt_select((uint32_t)((int32_t)au.sel + s + 3 * 16));
    if ((s = panel_enc(EN_ALGO)) != 0 && !au.lockc) {   /* the chosen circle's envelope mode */
        alt_emode[au.sel] = (uint8_t)clamp((int32_t)alt_emode[au.sel] + (s > 0 ? 1 : -1), 0, (int32_t)AM_NSEL - 1);
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
    /* the combo: CIRCLE 1+2+3 = TOTAL CHAOS (the chosen circle put back). (MUTE 1+2+3 = SHUFFLE removed 05/11,
     * Romain: a double of the dice; the three MUTE keys are three mutes. The companion's « Au hasard » keeps the draw) */
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
            if (!combo && a->kind != AK_MUTE && a->kind != AK_SHIFT && a->kind != AK_SEL && a->kind != AK_SONG)
                alt_say(a->name);
            if (combo) {
                ;                                /* (part of a combo: nothing else) */
            } else if (a->kind == AK_SONG) {     /* SONG: a slot (alt_sg_key) */
                alt_sg_key(i2);
                au.tap_ms[k] = fm1_ms | 1u;
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
                uint32_t dn2 = (ALT_BTNS >> panel.btn[B_OCTDN]) & 1u, up2 = (ALT_BTNS >> panel.btn[B_OCTUP]) & 1u;
                alt_die_w[i2] = (uint8_t)(dn2 ? AS_DW_VC : up2 ? AS_DW_PDR : AS_DW_RHY);   /* (Romain 04/11: three dice) */
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
                uint32_t c2 = au.sel % AS_NCIRC, rd = (ALT_BTNS >> panel.btn[B_REC]) & 1u;
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
                alt_roll_w[i2] = alt_die_w[i2];      /* DICE: the pattern · OCT- + DICE: velocity and chance (and a
                                                  * LEN, Romain 10/11) · OCT+ + DICE: pitch, delay and reverb throws */
                if (alt_die_w[i2] == AS_DW_VC && !(alt_seq.c[i2].locks & AS_LK_RHYTHM)) {
                    au.cfg[i2].len = alt_len_draw();
                    alt_len_fit(&au.cfg[i2]);
                    alt_post_now |= (uint8_t)(1u << i2);
                    alt_post_circle(i2);
                }
                alt_roll_cnt[i2]++;
                if (au.cfg[i2].chord != AS_CH_OFF) {   /* (le hasard : jamais d'accord, Romain 03/11) */
                    au.cfg[i2].chord = AS_CH_OFF;
                    alt_post_circle(i2);
                }
                alt_cal(i2);
                au.tap_ms[k] = fm1_ms | 1u;
                alt_say(alt_die_w[i2] == AS_DW_VC ? "VELO+CHANCE+LEN" : alt_die_w[i2] == AS_DW_PDR ? "PITCH + FX" : "NEW PATTERN");
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
                lit = ALT_SEQ_BK[k] == au.lane || (int)((ALT_NOTES >> k) & 1u);
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
            lit = (int)((ALT_NOTES >> k) & 1u) || (au.tap_ms[k] && (int32_t)(fm1_ms - au.tap_ms[k]) < (int32_t)ALT_TAP_MS);
        else if (a->kind == AK_FILL)
            lit = (int)((ALT_NOTES >> k) & 1u);
        else if (a->kind == AK_TLOCK)            /* (dark unless the chosen circle is TURING) */
            lit = au.cfg[au.sel].type == AS_T_TURING && alt_seq.c[au.sel].tfreeze;
        else if (a->kind == AK_SONG)             /* SONG: the slot playing lit, the one asked for blinking, a full one
                                                  * faint, an empty one dark */
            lit = (sg_play == a->arg + 1u && sg_slot[a->arg].used) || (asg.q == a->arg + 1u && ((fm1_ms >> 7) & 1u)) ||
                  (int)((ALT_NOTES >> k) & 1u);
        faint = a->kind && !lit && (a->kind != AK_TLOCK || au.cfg[au.sel].type == AS_T_TURING) &&
                (a->kind != AK_SONG || sg_slot[a->arg].used);
        led_clear(nl, 14u + k);
        led_clear(nd, 14u + k);
        led_clear(nb, 14u + k);
        led_clear(nm, 14u + k);
        {                                        /* (Romain 05/11) every key with a function at full light; a state on
                                                  * (a mute, the chosen circle, the slot playing, TURING frozen, a slot
                                                  * asked for) beats: a short dip to the mid level each second (the LEDs
                                                  * have no level between ~60 % and full); a momentary key pressed
                                                  * (a die, a fill...) dips while held; a key with nothing to do dark */
            int moment = a->kind == AK_DIE || a->kind == AK_SHIFT || a->kind == AK_DENS || a->kind == AK_TRIG ||
                         a->kind == AK_FILL;
            int dip = lit && (moment || (fm1_ms % 1000u) < 170u);
            led_put(nl, 14u + k, (lit || faint) && !dip);
            led_put(nm, 14u + k, dip);
        }
    }
    led_clear(nl, panel.btn[B_REC]);             /* REC: lit while it writes, faint when the chosen circle has moves */
    led_clear(nd, panel.btn[B_REC]);
    led_put(nl, panel.btn[B_REC], ar_on != 0);
    led_put(nd, panel.btn[B_REC], !ar_on && ar_n[au.sel % AS_NCIRC]);
    led_clear(nl, panel.btn[B_ARP]);             /* ARP: lit while the chain plays, faint when there is one */
    led_clear(nd, panel.btn[B_ARP]);
    led_put(nl, panel.btn[B_ARP], sg_on != 0);
    led_put(nd, panel.btn[B_ARP], !sg_on && asg.n);
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

/* (08/11, SMOOTH) the box x0..x1, y0..y1 (screen pixels) misses the canvas: what would be drawn there is skipped
 * whole (a small rectangle of the screen drawn alone does not walk every line of the scene) */
static int alt_miss(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    return x1 < -cv_ox || x0 >= (int32_t)cv_w - cv_ox || y1 < -cv_oy || y0 >= (int32_t)cv_h - cv_oy;
}

static void alt_plot(int32_t x, int32_t y, uint16_t c, int32_t a);   /* (alter_look.c) */
/* a disc, its edge anti-aliased (each edge pixel as much of c as the disc covers of it) */
static void alt_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy, dx, r16 = r * 16 + 8;
    if (alt_miss(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1))
        return;
    if (r >= 0 && r <= 10) {                     /* (09/11: the edge's coverage worked out once a radius, not a square
                                                  * root a pixel; the same values) */
        static uint8_t cov[2299] __attribute__((section(".pool")));   /* (radius r's square at sum of (2k+3)^2, k < r) */
        static uint16_t ok __attribute__((section(".pool")));
        int32_t w = 2 * r + 3, k, o = 0;
        uint8_t *t;
        for (k = 0; k < r; k++)
            o += (2 * k + 3) * (2 * k + 3);
        t = cov + o;
        if (!(ok >> r & 1u)) {
            for (dy = -r - 1; dy <= r + 1; dy++)
                for (dx = -r - 1; dx <= r + 1; dx++) {
                    int32_t d = (int32_t)alt_isqrt((uint32_t)((dx * dx + dy * dy) * 256)), a = (r16 - d) * 16 + 128;
                    t[(dy + r + 1) * w + dx + r + 1] = (uint8_t)(a >= 256 ? 255 : a > 0 ? a : 0);
                }
            ok |= (uint16_t)(1u << r);
        }
        for (dy = -r - 1; dy <= r + 1; dy++)
            for (dx = -r - 1; dx <= r + 1; dx++) {
                uint32_t a = t[(dy + r + 1) * w + dx + r + 1];
                if (a == 255u)
                    cv_rect(cx + dx, cy + dy, 1, 1, c);
                else if (a)
                    alt_plot(cx + dx, cy + dy, c, (int32_t)a);
            }
        return;
    }
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
    case QK_PROG:
        *name = "PROG";
        v = alt_seq.prog[au.sel % AS_NCIRC] % AS_NPROG;
        { const char *n = AS_PG_NAME[v]; int32_t k = 0; while (n[k]) { txt[k] = n[k]; k++; } txt[k] = 0; }
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
    case QK_RUMBLE:
        *name = "RUMBLE";
        v = alt_rmb[au.sel % AS_NCIRC];
        *fill = v * 50 / 127;
        alt_num(txt, v, 0);
        return v + 1000;
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
    static const char *const TAB[4] = {"SCENES A", "SCENES B", "PATT. A", "PATT. B"};
    uint32_t t = au.tab >> 1, r = au.row[au.tab], i, top, n = alt_tab_n(au.tab);
    char nb[12];
    alt_card("SAVE", "XX");
    if (anm.open) {                              /* the NAME page */
        uint32_t k, u = (anm.r - 1u - ALT_NFACT) % ALT_NUSER;
        char hd[24], one[2] = {0, 0};
        uint32_t j = 0;
        const char *w = anm.t ? "PATTERN " : "SCENE ";
        while (*w)
            hd[j++] = *w++;
        hd[j++] = (char)('A' + u / 16u);
        hd[j++] = (char)('0' + (u % 16u + 1u) / 10u);
        hd[j++] = (char)('0' + (u % 16u + 1u) % 10u);
        hd[j] = 0;
        cv_text(120 - text_w(ALT_FM, hd) / 2, 40, ALT_FM, hd, ALT_INK);
        for (k = 0; k < 3u; k++) {
            int32_t x = 36 + (int32_t)k * 60;
            char kn[8] = "KNOB 1";
            one[0] = ALT_NM_CH[anm.ch[k] % (uint32_t)ALT_NM_N];
            cv_rect(x, 76, 48, 64, alt_gold_line(16));
            cv_frame(x, 76, 48, 64, ALT_COL[k]);
            if (one[0] == ' ')
                cv_rect(x + 14, 124, 20, 2, ALT_GREY);   /* (a space: a line) */
            else
                cv_text(x + 24 - text_w(ALT_FL, one) / 2, 94, ALT_FL, one, ALT_INK);
            kn[5] = (char)('1' + k);
            cv_text(x + 24 - text_w(ALT_FS, kn) / 2, 146, ALT_FS, kn, ALT_COL[k]);
        }
        cv_text(8, 196, ALT_FS, "A-Z 0-9 -  OR A SPACE", ALT_DIM);
        cv_text(8, 214, ALT_FS, "SAVE: STORE   HOME: CANCEL", ALT_INK);
        return;
    }
    alt_tabs(23, TAB, 4, au.tab, ALT_COL[0]);
    if (au.msg && fm1_ms - au.msg_ms < 1500u)   /* SAVED, PROTECTED, EMPTY .. */
        cv_text(8, 214, ALT_FS, au.msg, ALT_COL[1]);
    else
        cv_text(8, 214, ALT_FS, "SELECT  SAVE: LOAD  HOLD: STORE", ALT_DIM);
    top = r < 2u ? 0u : r - 2u;                  /* five rows, the chosen one second or third */
    if (top > n - 5u)
        top = n - 5u;
    for (i = 0; i < 5u; i++) {
        uint32_t row = alt_tab_row(au.tab, top + i);
        int32_t y = 48 + (int32_t)i * 30, on = top + i == r, user = row > ALT_NFACT;
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
        for (i = 0; i < 4u; i++)
            sg = sg * 31u + (uint32_t)alt_lfo_val(tb, i);
        sg = sg * 31u + (uint32_t)alt_lfo_acts(tb) * 5u;
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

/* an LFO tab (Romain 10/11): the wave of the bar ahead scrolls in from the right to a bar at the left, "now"; where they
 * meet, a dot that rises and falls with the LFO. No beats, no faint shape: only the line through the middle (+ / -).
 * Each frame only what the wave or the dot left or took is sent again (alt_lw_frame), not the band */
/* l'horloge des LFO lue UNE fois par image (Romain 03/11) : l'écran se dessine en deux moitiés et la vague du bas était
 * décalée de celle du haut (le son avance entre les deux) : une ligne au milieu */
static uint32_t alt_draw_gl_n, alt_draw_lfo_p;
#define ALT_LW_BX 12                             /* the "now" bar: 2 px at x 12-13, the dot's centre at 13 */
#define ALT_LW_X0 14                             /* the wave: x 14 .. 231, the bar ahead */
#define ALT_LW_W 218
#define ALT_LW_Y0 50
#define ALT_LW_H 112
#define ALT_LW_R 4                               /* the dot's radius */
static int16_t alt_lw_y[ALT_LW_W], alt_lw_dot;   /* this frame's wave (its row at each column) and dot */
static int16_t alt_lw_py[ALT_LW_W], alt_lw_pdot; /* what is on the screen */
static uint8_t alt_lw_ok, alt_lw_tb;             /* (alt_lw_py holds tab alt_lw_tb's) */
static void alt_lw_calc(uint32_t tb)
{
    uint32_t w, r, seed, p = alt_draw_lfo_p ? alt_draw_lfo_p : 1u, now = alt_draw_gl_n, bar = 16u * p;
    int32_t x, dep = alt_lfo_val(tb, 2), mid = ALT_LW_Y0 + ALT_LW_H / 2;
    alt_lfo_def(tb, &w, &r, &seed);
    for (x = -1; x < ALT_LW_W; x++) {            /* (x -1: now, the dot) */
        uint32_t ahead = x < 0 ? 0u : (uint32_t)((uint64_t)(uint32_t)x * bar / ALT_LW_W);
        int32_t v = alt_lfo_at(w, r, seed, now + ahead, p, 0), y;
        if (!alt_wave_uni(w))
            v = clamp(v, -32767, 32767);
        y = mid - (((v * dep / 64) * (ALT_LW_H / 2 - 3)) >> 15);
        if (x < 0)
            alt_lw_dot = (int16_t)y;
        else
            alt_lw_y[x] = (int16_t)y;
    }
}
/* the wave's band: the middle line, the wave, the bar, the dot (from alt_lw_y; clipped by the canvas) */
static void alt_lw_draw(uint32_t tb, uint16_t col)
{
    int32_t x, py = alt_lw_dot, mid = ALT_LW_Y0 + ALT_LW_H / 2;
    uint16_t wc = alt_lfo_acts(tb) ? col : ALT_DIM;
    for (x = 0; x < ALT_WAVE_W; x += 3)
        cv_pset(ALT_WAVE_X + x, mid, alt_gold_line(35));
    for (x = 0; x < ALT_LW_W; x++) {
        int32_t y = alt_lw_y[x];
        if (y - py > 1 || py - y > 1)
            cv_rect(ALT_LW_X0 + x, y < py ? y : py, 2, (y < py ? py - y : y - py) + 1, wc);
        else
            cv_rect(ALT_LW_X0 + x, y - 1, 1, 3, wc);
        py = y;
    }
    cv_rect(ALT_LW_BX, ALT_LW_Y0, 2, ALT_LW_H, wc);   /* now */
    alt_disc(ALT_LW_BX + 1, alt_lw_dot, ALT_LW_R + 1, ALT_BG);
    alt_disc(ALT_LW_BX + 1, alt_lw_dot, ALT_LW_R, wc);
}
/* the rows column x of the wave can touch (a step drawn from the column before, 2 px wide; 1 px above and below) */
static void alt_lw_span(const int16_t *yv, int16_t dot, int32_t x, int32_t *lo, int32_t *hi)
{
    int32_t a = yv[x], k;
    *lo = *hi = a;
    for (k = x - 2; k < x; k++) {               /* (column x: its step from x-1, and x-1's, 2 px wide, from x-2) */
        int32_t v = k < 0 ? dot : yv[k];
        if (v < *lo) *lo = v;
        if (v > *hi) *hi = v;
    }
    *lo -= 1;
    *hi += 1;
}
/* what a whole picture of the LFO tab tb left on the screen (alter_draw, after it) */
static void alt_lw_seen(uint32_t tb)
{
    uint32_t i;
    for (i = 0; i < ALT_LW_W; i++)
        alt_lw_py[i] = alt_lw_y[i];
    alt_lw_pdot = alt_lw_dot;
    alt_lw_ok = 1;
    alt_lw_tb = (uint8_t)tb;
}
static void alt_lfo_tab(uint32_t tb, uint16_t col)
{
    char b[8];
    uint32_t i;
    int on = alt_lfo_acts(tb);
    alt_lw_calc(tb);
    alt_lw_draw(tb, col);
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
    alt_tabs(23, ALT_MODE_NAME, AM_NSEL, m, col);
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

/* MIXER (XIV, Temperance; GLO held, Romain 04/11): KNOB 1 2 3 = the three circles' LEVEL (as VOLUME on GLO's quick
 * page, through the queue: REC and the scene keep it), KNOB 4 = the MASTER gain (fx.c alt_mgain, 100 = 0 dB; not
 * stored: 0 dB at power-on). The meter: the loudest sample into the output (fx.c alt_mpk) against the FM-1's own
 * limits: under the limiter (white), the limiter at work (gold, from -5 dB), the soft clip saturating (red, 0 dB and
 * over); its peak held 1 s. The keys play on */
static int32_t alt_mix_pk, alt_mix_hold;
static uint32_t alt_mix_ms;
/* the CONSOLE (étape 4, Romain 05/11): the MIXER's tabs MIX, I, II, III (ALGORITHM); a circle's tab is its strip, two
 * rows of four (SELECT the row, KNOB 1-4 its values): DRIVE LOW MID HIGH / COMP DELAY REVERB LEVEL. DRIVE (P_DIST),
 * the sends and LEVEL go through the queue (REC, the scene: as before); LOW MID HIGH COMP are alter_console.c's acs_v
 * (the scene keeps them, not REC). A circle's tab follows the chosen circle (the black keys) */
static uint8_t alt_mx_tab, alt_mx_row, alt_mx_sel;
static const char *const ALT_MX_TAB[4] = {"MIX", "I", "II", "III"};
static const char *const ALT_MX_NM[2][4] = {{"DRIVE", "LOW", "MID", "HIGH"}, {"COMP", "DELAY", "REVERB", "LEVEL"}};
static const uint8_t ALT_MX_P[2][4] = {{P_DIST, 255, 255, 255}, {255, P_DLY, P_REV, P_LEVEL}};   /* (255: acs_v) */
static int32_t alt_mx_val(uint32_t c, uint32_t r, uint32_t i)   /* a strip's value 0..127 */
{
    uint32_t p = ALT_MX_P[r & 1u][i & 3u];
    if (p != 255u)
        return clamp(trk[c % AS_NCIRC].p[p], 0, 127);
    return acs_v[c % AS_NCIRC][r ? ACS_COMP : i - 1u];
}
static void alt_mx_nav(void)                     /* ALGORITHM: the tab; SELECT: the row (MIX: the chosen circle) */
{
    int32_t s;
    if (alt_mx_tab && alt_mx_sel != au.sel)      /* (a black key chose another circle: its strip) */
        alt_mx_tab = (uint8_t)(au.sel % AS_NCIRC + 1u);
    if ((s = panel_enc(EN_ALGO)) != 0) {
        alt_mx_tab = (uint8_t)clamp((int32_t)alt_mx_tab + (s > 0 ? 1 : -1), 0, 3);
        if (alt_mx_tab && au.sel != alt_mx_tab - 1u)
            alt_select(alt_mx_tab - 1u);
    }
    alt_mx_sel = au.sel;
    if (alt_mx_tab && (s = panel_enc(EN_SELECT)) != 0)
        alt_mx_row = (uint8_t)(s > 0 ? 1u : 0u);
}
static void alt_mix_turn(uint32_t i, int32_t s)
{
    if (alt_mx_tab) {                            /* a strip */
        uint32_t c = (alt_mx_tab - 1u) % AS_NCIRC, p = ALT_MX_P[alt_mx_row & 1u][i & 3u];
        if (p != 255u)
            alt_qpush(c, p, s * 2);
        else {
            uint32_t k = alt_mx_row ? ACS_COMP : i - 1u;
            acs_v[c][k] = (uint8_t)clamp((int32_t)acs_v[c][k] + s * 2, 0, 127);
        }
        return;
    }
    if (i < AS_NCIRC)
        alt_qpush(i, P_LEVEL, s * 2);
    else
    {
        alt_mgain = (uint8_t)clamp((int32_t)alt_mgain + s * 2, 0, 127);
        alt_sys[5] = (uint8_t)(alt_mgain + 1u);   /* (kept: SYSTEM's settings) */
        settings_save();
    }
}
static void alt_mix_poll(void)                   /* once a frame: the meter falls ~20 dB/s, its peak held 1 s */
{
    int32_t a = alt_mpk;
    alt_mpk = 0;
    alt_mix_pk -= alt_mix_pk >> 3;
    if (a > alt_mix_pk)
        alt_mix_pk = a;
    if (a >= alt_mix_hold || fm1_ms - alt_mix_ms > 1000u) {
        alt_mix_hold = a;
        alt_mix_ms = fm1_ms;
    }
}
static int32_t alt_mix_y(int32_t a, int32_t top, int32_t h)   /* a level (32767 = 0 dB) to a height: -40 .. +6 dB */
{
    int32_t db = 0, x = a > 0 ? a : 1;
    while (x < 32767 && db > -40)                /* (in 1/2 dB: x up by 6 % a step) */
        x += x / 16 + 1, db--;
    while (x > 32767 * 2 && db < 12)
        x -= x / 17, db++;
    db = clamp(db / 2, -40, 6);
    return top + h - (db + 40) * h / 46;
}
static int32_t alt_db_q12(int32_t g)            /* a gain, Q12, in whole dB (-40 .. +12) */
{
    int32_t db = 0, x = g > 0 ? g : 1;
    while (x < 4096 && db > -80)                 /* (in 1/2 dB, as alt_mix_y) */
        x += x / 16 + 1, db--;
    while (x > 4096 * 109 / 100 && db < 24)
        x -= x / 17, db++;
    return db / 2;
}
static void alt_mx_strip(uint32_t c)             /* circle c's CONSOLE strip */
{
    uint32_t r, i;
    uint16_t col = ALT_COL[c], hl = alt_gold_line(16);
    char b[12];
    cv_text_r(232, 24, ALT_FS, ALT_PRE[au.pre[c] % ALT_NPRE].name, col, ALT_BG);   /* (whose sound) */
    for (r = 0; r < 2u; r++) {
        int32_t y = 66 + (int32_t)r * 64, on = r == alt_mx_row;
        uint16_t bg = on ? hl : ALT_BG;
        if (on) {
            cv_rect(4, y - 4, 232, 58, hl);
            alt_diamond(8, y + 24, 3, 3, col);
        }
        for (i = 0; i < 4u; i++) {
            int32_t x = 16 + (int32_t)i * 56, v = alt_mx_val(c, r, i), bx = x, bw = 48;
            int eq = r == 0 && i > 0;
            const char *nm = ALT_MX_NM[r][i];
            if (eq)
                alt_num(b, v ? alt_db_q12(acs_gain(v)) : 0, 0);
            else if (r == 1 && i == 3u) {
                int32_t d = LEVEL_DB_X10[v];
                alt_num(b, (d + (d < 0 ? -5 : 5)) / 10, 0);
            } else
                alt_num(b, v, 0);
            if (eq && v == 0)
                alt_scopy(b, "KILL", sizeof b);
            else if (eq && v > 64) {             /* (+ before a boost) */
                char t[12];
                alt_scopy(t, b, sizeof t);
                b[0] = '+';
                alt_scopy(b + 1, t, sizeof b - 1u);
            } else if (v == 0 && (i == 0 || (r == 1 && i < 3u)))
                alt_scopy(b, "OFF", sizeof b);
            else if (r == 1 && i == 3u && v == 0)
                alt_scopy(b, "OFF", sizeof b);
            cv_text_on(x, y, ALT_FS, nm, on ? ALT_INK : ALT_DIM, bg);
            cv_text_on(x, y + 14, ALT_FM, b, on ? col : ALT_DIM, bg);
            cv_rect(bx, y + 38, bw, 2, alt_gold_line(30));
            if (eq) {                            /* (from the middle: 0 dB) */
                int32_t m = bx + bw / 2, f = (v - 64) * (bw / 2) / 63;
                cv_rect(f < 0 ? m + f : m, y + 37, f < 0 ? -f : f + 1, 4, on ? col : ALT_DIM);
                cv_rect(m, y + 35, 1, 8, ALT_INK);
            } else
                cv_rect(bx, y + 37, v * bw / 127, 4, on ? col : ALT_DIM);
            if (r == 1 && i == 0 && v) {         /* COMP: its reduction now (gold), down to -12 dB */
                int32_t red = -alt_db_q12(acs_gr[c]);
                cv_rect(bx, y + 45, clamp(red, 0, 12) * bw / 12, 3, ALT_GOLD);
            }
        }
    }
    cv_rect(8, 196, 224, 1, alt_gold_line(30));
    cv_text(8, 202, ALT_FS, "SELECT: ROW   KNOB 1-4: CHANGE", ALT_DIM);
    cv_text(8, 216, ALT_FS, "ALGORITHM: TAB   BLACK KEYS: CIRCLE", ALT_DIM);
}
static void alt_mix_scene(void)
{
    uint32_t c;
    int32_t top = 78, h = 96, y0 = top + h, x, y, yl = alt_mix_y(LIM_T, top, h), y0db = alt_mix_y(32767, top, h);
    char b[12];
    alt_card("MIXER", "XIV");
    alt_tabs(40, ALT_MX_TAB, 4, alt_mx_tab, alt_mx_tab ? ALT_COL[(alt_mx_tab - 1u) % AS_NCIRC] : ALT_GOLD);
    if (alt_mx_tab) {
        alt_mx_strip((alt_mx_tab - 1u) % AS_NCIRC);
        return;
    }
    for (c = 0; c <= AS_NCIRC; c++) {            /* the faders: three circles, then the master */
        int32_t v = c < AS_NCIRC ? clamp(trk[c].p[P_LEVEL], 0, 127) : alt_mgain, fy;
        uint16_t col = c < AS_NCIRC ? ALT_COL[c] : ALT_GOLD;
        x = 18 + (int32_t)c * 46;
        cv_rect(x + 6, top, 2, h, ALT_DARK);
        fy = y0 - v * h / 127;
        cv_rect(x + 6, fy, 2, y0 - fy, c < AS_NCIRC && alt_seq.c[c].cfg.mute ? ALT_GREY : col);
        cv_rect(x, fy - 2, 14, 5, col);
        if (c < AS_NCIRC) {
            if (v <= 0)
                alt_scopy(b, "OFF", sizeof b);
            else {
                int32_t d = LEVEL_DB_X10[v];
                alt_num(b, (d + (d < 0 ? -5 : 5)) / 10, 0);   /* (whole dB) */
            }
        } else
            alt_num(b, v, 0);
        cv_text(x + 7 - text_w(ALT_FS, b) / 2, y0 + 6, ALT_FS, b, ALT_INK);
        {
            const char *nm = c == 0 ? "I" : c == 1 ? "II" : c == 2 ? "III" : "MST";
            cv_text(x + 7 - text_w(ALT_FS, nm) / 2, top - 16, ALT_FS, nm, col);
        }
    }
    x = 204;                                     /* the output meter, its zones marked */
    y = alt_mix_y(alt_mix_pk, top, h);
    cv_rect(x, top, 12, h, ALT_DARK);
    if (y < y0)
        cv_rect(x, y, 12, y0 - y, ALT_INK);
    if (y < yl)
        cv_rect(x, y, 12, yl - y, ALT_GOLD);
    if (y < y0db)
        cv_rect(x, y, 12, y0db - y, RGB(220, 40, 40));
    y = alt_mix_y(alt_mix_hold, top, h);
    cv_rect(x - 2, y, 16, 2, alt_mix_hold > 32767 ? RGB(220, 40, 40) : alt_mix_hold > LIM_T ? ALT_GOLD : ALT_INK);
    cv_rect(x + 14, y0db, 6, 1, RGB(220, 40, 40));
    cv_rect(x + 14, yl, 6, 1, ALT_GOLD);
    cv_text(x + 6 - text_w(ALT_FS, "OUT") / 2, top - 16, ALT_FS, "OUT", ALT_DIM);
    {
        const char *st = alt_mix_hold > 32767 ? "CLIP" : alt_mix_hold > LIM_T ? "LIMIT" : "OK";
        cv_text(x + 6 - text_w(ALT_FS, st) / 2, y0 + 6, ALT_FS, st,
            alt_mix_hold > 32767 ? RGB(220, 40, 40) : alt_mix_hold > LIM_T ? ALT_GOLD : ALT_DIM);
    }
    cv_rect(8, 196, 224, 1, alt_gold_line(30));
    cv_text(8, 202, ALT_FS, "KNOB 1 2 3 CIRCLES (dB)  4 MASTER", ALT_INK);
    cv_text(8, 216, ALT_FS, "ALGORITHM: I II III = CONSOLE", ALT_DIM);
}

/* SONG (XX, Judgement; ARP held, Romain 05/11): the three slots (lit: playing; blinking: asked for; EMPTY), the chain as
 * a list (SELECT a link: KNOB 1 its slot, KNOB 2 its bars, KNOB 3 the links), the link playing marked with its bar */
static void alt_sg_scene(void)
{
    static const char *const BARS[4] = {"1 BAR", "2 BARS", "4 BARS", "8 BARS"};
    static const char *const BARN[4] = {"1", "2", "4", "8"};
    uint32_t r;
    uint16_t col = ALT_COL[au.sel % AS_NCIRC], hl = alt_gold_line(16);
    char b[24], kv[4][8];
    alt_card("SONG", "XX");
    /* (Romain 05/11: no A B C boxes here; the list of links, the four knobs named at the foot as on every page) */
    if (!asg.n) {
        cv_text(120 - text_w(ALT_FM, "NO CHAIN") / 2, 84, ALT_FM, "NO CHAIN", ALT_DIM);
        cv_text(120 - text_w(ALT_FS, "HOLD A, TAP C THEN B: A > C > B") / 2, 110, ALT_FS, "HOLD A, TAP C THEN B: A > C > B", ALT_DIM);
        cv_text(120 - text_w(ALT_FS, "OR KNOB 3: A LINK MORE") / 2, 126, ALT_FS, "OR KNOB 3: A LINK MORE", ALT_DIM);
    } else {
        cv_text(12, 30, ALT_FS, sg_on ? "CHAIN ON" : "CHAIN OFF", sg_on ? ALT_GOLD : ALT_DIM);
        if (sg_on && sg_link < asg.n) {
            uint32_t tot = sg_bars(asg.chain[sg_link] >> 2), at = tot - (sg_left ? sg_left : 1u) + 1u, j = 0;
            const char *w = "BAR ";
            while (*w)
                b[j++] = *w++;
            alt_num(b + j, (int32_t)at, 0);
            while (b[j])
                j++;
            b[j++] = '/';
            alt_num(b + j, (int32_t)tot, 0);
            cv_text_r(228, 30, ALT_FS, b, ALT_INK, ALT_BG);
        }
        if (asg.row < asg.top)
            asg.top = asg.row;
        if (asg.row >= asg.top + 7u)
            asg.top = (uint8_t)(asg.row - 6u);
        for (r = asg.top; r < asg.n && r < asg.top + 7u; r++) {
            int32_t y = 48 + (int32_t)(r - asg.top) * 20;
            uint32_t sl = asg.chain[r] & 3u;
            int playing = sg_on && r == sg_link;
            char nm[2] = {(char)('A' + sl), 0};
            uint16_t bg = r == asg.row ? hl : ALT_BG;
            if (r == asg.row) {
                cv_rect(8, y - 3, 224, 19, hl);
                alt_diamond(15, y + 6, 3, 3, col);
            }
            alt_num(b, (int32_t)r + 1, 0);
            cv_text_on(26, y, ALT_FM, b, playing ? ALT_GOLD : ALT_DIM, bg);
            cv_text_on(70, y, ALT_FM, nm, sg_slot[sl].used ? ALT_COL[sl] : ALT_GREY, bg);
            if (!sg_slot[sl].used)
                cv_text_on(90, y + 2, ALT_FS, "EMPTY", ALT_GREY, bg);
            else if (playing)
                cv_text_on(90, y + 2, ALT_FS, "PLAYING", ALT_GOLD, bg);
            cv_text_r(226, y, ALT_FM, BARS[(asg.chain[r] >> 2) & 3u], playing ? ALT_GOLD : ALT_INK, bg);
        }
    }
    {                                            /* the four knobs (alt_sg_turn) */
        static const char *const NAME[4] = {"SLOT", "BARS", "LINKS", "CHAIN"};
        uint32_t i, cur = asg.row < asg.n ? asg.chain[asg.row] : 0u;
        kv[0][0] = asg.n ? (char)('A' + (cur & 3u)) : '-', kv[0][1] = 0;
        alt_scopy(kv[1], asg.n ? BARN[(cur >> 2) & 3u] : "-", sizeof kv[1]);
        alt_num(kv[2], asg.n, 0);
        alt_scopy(kv[3], sg_on ? "ON" : "OFF", sizeof kv[3]);
        cv_rect(8, 196, 224, 1, alt_gold_line(30));
        alt_diamond(120, 196, 2, 2, ALT_GOLD);
        for (i = 0; i < 4u; i++)
            alt_dialk(8 + (int32_t)i * 57, 200, NAME[i], kv[i], -1, -1, -1, 1, col, 0);
    }
}

/* SYSTEM (XXI, the World; HOME held 0.5 s, Romain 04/11, 05/11): four tabs (ALGORITHM), rows (SELECT), KNOB 1 changes
 * the row; the battery in the corner. Kept in the settings (alt_sys above, or Felucca's own record), from the next frame:
 *   SETUP   THEME (NIGHT / DAY: EDIT held's SETTINGS until 05/11), CLOCK (INT, USB, TRS: the Analog Rytm's clock
 *           through the TRS jack), MIDI IN (Felucca's G_ROUTE: CH1-4 = channels 1 2 3 play / move circles 1 2 3)
 *   AUDIO   Felucca's own rows (menu_items.c, as its MENU sets them): CLICK (OFF / ON: its REC is Felucca's), CLICK LEVEL, SPEAKER EQ,
 *           USB LEVEL, TUNE (a project value at Felucca's: kept here in alt_sys[6]), 2ND CORE (from the next power-on)
 *   RANDOM  which engines (ALTER PHYS NOISE RESO) the random's sounds may come from, which modes (FREE PERC STAB)
 *           CHAOS TOTAL may give: ON / OFF each (at least one stays on)
 *   ABOUT   Felucca's ABOUT / CREDITS document (ui_menu.c menu_document: the GPL-3.0 notices), SELECT scrolls it */
enum { SYS_THEME, SYS_SMOOTH, SYS_CLOCK, SYS_MIDIIN, SYS_TRSSEEN, SYS_USBSEEN, SYS_NMIDI };   /* (SEEN: read only, 05/11;
                                                  * SMOOTH 08/11: alt_sys[8], 0 = ON) */
static const char *alt_set_text(uint32_t r);    /* (THEME: below) */
static void alt_set_turn(uint32_t r, int32_t s);
enum { SYS_TMIDI, SYS_TLEARN, SYS_TAUD, SYS_TRAND, SYS_TABOUT, SYS_NT };
static const char *const SYS_TAB[SYS_NT] = {"SETUP", "LEARN", "AUDIO", "RANDOM", "ABOUT"};
static const char *const SYS_MROW[SYS_NMIDI] = {"THEME", "SMOOTH", "CLOCK", "MIDI IN", "TRS SEEN", "USB SEEN"};
static const uint8_t SYS_AROW[] = {MI_CLICK, MI_CLKLVL, MI_LOWCUT, MI_USB, MI_TUNE,   /* (COUNT-IN left 05/11: only
                                                  * Felucca's REC counted in) */
#if FELUCCA_CPU2
                                   MI_CPU2,
#endif
};
#define SYS_NAUD ((uint32_t)NELEM(SYS_AROW))
#define SYS_NRE ALT_NENG                         /* (RANDOM: the engines' rows, then the three modes') */
static const char *const SYS_RROW[ALT_NENG + 3u] = {"ALTER", "PHYS", "NOISE", "RESO", "PLATE", "CORDE", "RING",
                                                    "FREE", "PERC", "STAB"};
static uint8_t alt_sys_top;                      /* (the first row shown: RANDOM's ten scroll)  */
static uint8_t alt_sys_tab;
static int16_t alt_sys_scroll;                   /* (ABOUT: pixels scrolled) */
#include "alter_learn.c"                         /* LEARN: MIDI LEARN (07/11) */
static uint32_t alt_sys_rows(void)
{
    return alt_sys_tab == SYS_TLEARN ? alm_rows() : alt_sys_tab == SYS_TMIDI ? SYS_NMIDI : alt_sys_tab == SYS_TAUD ? SYS_NAUD : alt_sys_tab == SYS_TRAND ? SYS_NRE + 3u : 0u;
}
static const char *alt_sys_name(uint32_t r)
{
    return alt_sys_tab == SYS_TLEARN ? alm_name(r) : alt_sys_tab == SYS_TMIDI ? SYS_MROW[r] : alt_sys_tab == SYS_TAUD ? MI_NAME[SYS_AROW[r]] : SYS_RROW[r];
}
static const char *alt_sys_text(uint32_t r)
{
    static char tb[12];
    if (alt_sys_tab == SYS_TLEARN)
        return alm_text(r);
    if (alt_sys_tab == SYS_TRAND) {
        uint32_t m = r < SYS_NRE ? alt_sys[1] >> r : alt_sys[2] >> (r - SYS_NRE);
        return (m & 1u) ? "ON" : "OFF";
    }
    if (alt_sys_tab == SYS_TAUD) {
        uint32_t row = SYS_AROW[r];
        const char *v = menu_vname(row, menu_get(row));
        if (row == MI_TUNE) {                    /* (the cents: "+12 CT") */
            uint32_t n = 0;
            while (v[n] && n < 7u)
                tb[n] = v[n], n++;
            tb[n++] = ' ', tb[n++] = 'C', tb[n++] = 'T', tb[n] = 0;
            return tb;
        }
        return v;
    }
    if (r == SYS_THEME)
        return alt_set_text(0);
    if (r == SYS_SMOOTH)
        return alt_sys[8] ? "OFF" : "ON";
    if (r == SYS_CLOCK)
        return N_CLOCK[clamp(song.g[G_CLOCK], 0, (int32_t)NELEM(N_CLOCK) - 1)];
    if (r == SYS_TRSSEEN || r == SYS_USBSEEN) {  /* (Romain 05/11: "the TRS sees no MIDI"): what came in through each
                                                  * jack since power-on, its messages counted, "CLOCK" when F8s come */
        uint32_t k = r == SYS_TRSSEEN ? 2u : 1u, n = alt_midi_n[k], j = 0;
#if FELUCCA_UART
        if (!n && k == 2u && um.bytes) {         /* (bytes on the wire but no message: a garbled signal, "RAW 12") */
            tb[0] = 'R', tb[1] = 'A', tb[2] = 'W', tb[3] = ' ';
            alt_num(tb + 4, (int32_t)(um.bytes % 100000u), 0);
            return tb;
        }
#endif
        if (!n)
            return "NOTHING";
        alt_num(tb, (int32_t)(n % 100000u), 0);
        while (tb[j])
            j++;
        if (fm1_ms - alt_midi_clk_ms[k] < 500u && j < 6u)
            tb[j++] = ' ', tb[j++] = 'C', tb[j++] = 'L', tb[j++] = 'K';
        tb[j] = 0;
        return tb;
    }
    return N_ROUTE[clamp(song.g[G_ROUTE], 0, (int32_t)NELEM(N_ROUTE) - 1)];
}
static void alt_sys_turn(uint32_t r, int32_t s)
{
    int32_t d = s > 0 ? 1 : -1;
    if (alt_sys_tab == SYS_TLEARN) {
        alm_turn(r, s);
        return;                                  /* (it saves itself) */
    }
    if (alt_sys_tab == SYS_TRAND) {
        uint8_t *b = &alt_sys[r < SYS_NRE ? 1 : 2];
        uint32_t bit = 1u << (r < SYS_NRE ? r : r - SYS_NRE), all = r < SYS_NRE ? 127u : 7u, v = d > 0 ? *b | bit : *b & ~bit;
        if (v & all)                             /* (one stays on) */
            *b = (uint8_t)v;
    } else if (alt_sys_tab == SYS_TAUD) {
        uint32_t row = SYS_AROW[r];
        if (row == MI_CLICK)                     /* (OFF / ON: its REC mode clicks only with Felucca's REC) */
            menu_put(row, d > 0 ? 2u : 0u);
        else
            menu_put(row, menu_step(row, row == MI_TUNE ? s : d));   /* (TUNE: a cent a click, faster turned) */
        if (row == MI_TUNE)
            alt_sys[6] = (uint8_t)(menu_get(MI_TUNE) + 1u);
    } else if (r == SYS_THEME) {
        alt_set_turn(0, s);
        return;                                  /* (it saves itself) */
    } else if (r == SYS_SMOOTH) {
        alt_sys[8] = (uint8_t)(d > 0 ? 0u : 1u);
    } else if (r == SYS_TRSSEEN || r == SYS_USBSEEN) {
        return;                                  /* (read only) */
    } else if (r == SYS_CLOCK) {
        song.g[G_CLOCK] = (int16_t)clamp(song.g[G_CLOCK] + d, 0, (int32_t)NELEM(N_CLOCK) - 1);
        alt_sys[3] = (uint8_t)(song.g[G_CLOCK] + 1);
    } else {
        song.g[G_ROUTE] = (int16_t)clamp(song.g[G_ROUTE] + d, 0, (int32_t)NELEM(N_ROUTE) - 1);
        alt_sys[4] = (uint8_t)(song.g[G_ROUTE] + 1);
    }
    settings_save();
}
#define SYS_DOC_TOP 62                           /* (ABOUT: the document between the tabs and the line at 196) */
static int32_t alt_sys_scroll_max(void)
{
    int32_t h = menu_document(0, 0) - (192 - SYS_DOC_TOP);
    return h > 0 ? h : 0;
}
static void alt_sys_input(void)
{
    int32_t s;
    uint32_t i;
    if ((s = panel_enc(EN_ALGO)) != 0) {
        alt_sys_tab = (uint8_t)clamp((int32_t)alt_sys_tab + (s > 0 ? 1 : -1), 0, SYS_NT - 1);
        au.set_row = 0;
        alt_sys_top = 0;
        alt_sys_scroll = 0;
    }
    if ((s = panel_enc(EN_SELECT)) != 0) {
        if (alt_sys_tab == SYS_TABOUT)
            alt_sys_scroll = (int16_t)clamp((int32_t)alt_sys_scroll + s * 18, 0, alt_sys_scroll_max());
        else
        {
            au.set_row = (uint8_t)clamp((int32_t)au.set_row + s, 0, (int32_t)alt_sys_rows() - 1);
            if (au.set_row < alt_sys_top)        /* (the list follows the row) */
                alt_sys_top = au.set_row;
            if (au.set_row >= alt_sys_top + 7u)
                alt_sys_top = (uint8_t)(au.set_row - 6u);
        }
    }
    if (alt_sys_tab == SYS_TLEARN)
        alm_input();
    for (i = 0; i < 4u; i++)
        if ((s = panel_enc(EN_K1 + i)) != 0 && i == 0u) {
            if (alt_sys_tab == SYS_TABOUT)       /* (KNOB 1 scrolls ABOUT too) */
                alt_sys_scroll = (int16_t)clamp((int32_t)alt_sys_scroll + s * 18, 0, alt_sys_scroll_max());
            else
                alt_sys_turn(au.set_row, s);
        }
    (void)panel_enc(EN_PRESET);
    alt_hold = 0;
}
static void alt_sys_battery(int32_t x, int32_t y)   /* the battery: 3 bars (Felucca's thresholds) or USB power */
{
    int32_t l = batt_shown(), k;
    uint16_t c = l == 4 ? ALT_GOLD : l <= 1 ? RGB(220, 40, 40) : ALT_INK;
    cv_frame(x, y, 26, 12, c);
    cv_rect(x + 26, y + 3, 2, 6, c);
    for (k = 0; k < 3; k++)
        if (l == 4 || k < l)
            cv_rect(x + 2 + k * 8, y + 2, 6, 8, c);
    cv_text_r(x - 4, y, ALT_FS, l == 4 ? "USB" : l == 0 ? "LOW" : l == 1 ? "1/3" : l == 2 ? "2/3" : "FULL", c, ALT_BG);
}
static void alt_sys_scene(void)
{
    uint32_t r, n = alt_sys_rows();
    uint16_t col = ALT_COL[au.sel % AS_NCIRC], hl = alt_gold_line(16);
    static const char *const FOOT[SYS_NT][2] = {
        {"CLOCK TRS = THE RYTM'S CLOCK", "MIDI IN: CHANNELS 1 2 3 = CIRCLES"},   /* (SETUP) */
        {"KNOB 1 >: LEARN   <: CLEAR", "GROUP: CIRCLE I II III, GLOBAL, ACTIONS"},
        {"2ND CORE: AFTER A RESTART", "SPEAKER EQ: THE FM-1'S SPEAKER"},
        {"ENGINES: CHAOS, CHAOS TOTAL", "MODES: CHAOS TOTAL (SELECT: MORE)"},
        {"SELECT / KNOB 1: SCROLL", "GPL-3.0: THE SOURCE IS FREE"},
    };
    if (alt_sys_tab == SYS_TABOUT) {             /* the document first, its top hidden under the card */
        (void)menu_document(SYS_DOC_TOP - alt_sys_scroll, 1);
        cv_rect(0, 0, 240, SYS_DOC_TOP, ALT_BG);  /* (no half lines: hidden above the top and under 192) */
        cv_rect(0, 192, 240, 48, ALT_BG);
    }
    alt_card("SYSTEM", "XXI");
    alt_sys_battery(200, 24);
    alt_tabs(40, SYS_TAB, SYS_NT, alt_sys_tab, ALT_GOLD);
    if (alt_sys_top + 7u > n)
        alt_sys_top = (uint8_t)(n > 7u ? n - 7u : 0u);
    for (r = alt_sys_top; r < n && r < alt_sys_top + 7u; r++) {
        int32_t y = 60 + (int32_t)(r - alt_sys_top) * 19;
        if (r == au.set_row) {
            cv_rect(8, y - 3, 224, 18, hl);
            alt_diamond(15, y + 6, 3, 3, col);
        }
        cv_text_on(24, y, ALT_FM, alt_sys_name(r), ALT_INK, r == au.set_row ? hl : ALT_BG);
        if (alt_sys_tab == SYS_TLEARN && r && alt_sys_text(r)[0] != '-')   /* (LEARN: a link in the small letters,
                                                  * the names are long; none: "-") */
            cv_text_r(226, y + 3, ALT_FS, alt_sys_text(r), r == au.set_row || alm_wait == r ? col : ALT_DIM,
                      r == au.set_row ? hl : ALT_BG);
        else
            cv_text_r(226, y, ALT_FM, alt_sys_text(r), r == au.set_row ? col : ALT_DIM, r == au.set_row ? hl : ALT_BG);
    }
    cv_rect(8, 196, 224, 1, alt_gold_line(30));
    cv_text(8, 202, ALT_FS, alt_sys_tab == SYS_TLEARN && alm_msg && fm1_ms - alm_msg_ms < 2000u ? alm_msg :
                            FOOT[alt_sys_tab % SYS_NT][0], alt_sys_tab == SYS_TLEARN && alm_msg && fm1_ms - alm_msg_ms < 2000u ?
                            ALT_GOLD : ALT_DIM);
    cv_text(8, 216, ALT_FS, FOOT[alt_sys_tab % SYS_NT][1], ALT_DIM);
}

/* DEEP (XVII, the Star; EDIT held, Romain 05/11): every setting of the chosen circle's sound, as a list. Four tabs
 * (ALGORITHM): SOUND (the engine's eight), ENV (the ADSR, whatever the mode), FX (DIST, the INSERT: TYPE, its A B C and
 * MIX, the CHORUS DELAY REVERB sends; a sound with one: its RUMBLE), MIX (LEVEL, PAN, GLIDE). SELECT picks a row (the
 * list scrolls), KNOB 1 changes it, through the queue as a quick page's knob (REC writes it, the die's offset kept on
 * top). SELECT's circle is the one on the PLAY view: changed there (the black keys) the list follows */
#define DP_RUMBLE 0xFEu
#define DP_DOSE 0xFDu                            /* (the dice's DOSE: the SCL page's knob 4 until 10/11) */
enum { DP_SND, DP_ENV, DP_FX, DP_MIX, DP_NT };
static const char *const DP_TAB[DP_NT] = {"SOUND", "ENV", "FX", "MIX"};
static const uint8_t DP_ENVR[] = {P_ATK, P_DEC, P_SUS, P_REL};
static const uint8_t DP_FXR[] = {P_DIST, P_ITYPE, P_IA, P_IB, P_IC, P_IMIX, P_CHOR, P_DLY, P_REV, DP_RUMBLE};
static const uint8_t DP_MIXR[] = {P_LEVEL, P_PAN, P_GLIDE, DP_DOSE};
static const char *const DP_ENVN[] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
static const char *const DP_FXN[] = {"DIST", "INSERT", "", "", "", "INSERT MIX", "CHORUS", "DELAY", "REVERB", "RUMBLE"};
static const char *const DP_MIXN[] = {"LEVEL", "PAN", "GLIDE", "DICE DOSE"};
static uint8_t alt_dp_tab, alt_dp_top;
static int alt_dp_rumble(void) { return ALT_PRE[au.pre[au.sel % AS_NCIRC] % ALT_NPRE].rumble != 0; }
static uint32_t alt_dp_rows(void)
{
    return alt_dp_tab == DP_SND ? 8u : alt_dp_tab == DP_ENV ? 4u : alt_dp_tab == DP_MIX ? 4u :
           (uint32_t)NELEM(DP_FXR) - (alt_dp_rumble() ? 0u : 1u);
}
static uint32_t alt_dp_id(uint32_t r)
{
    return alt_dp_tab == DP_SND ? P_E0 + r : alt_dp_tab == DP_ENV ? DP_ENVR[r % 4u] :
           alt_dp_tab == DP_MIX ? DP_MIXR[r % 4u] : DP_FXR[r % NELEM(DP_FXR)];
}
/* row r: its name, its value's text (txt), its fill 0..50 (-1: a name, no bar) */
static const char *alt_dp_row(uint32_t r, char *txt, int32_t *fill)
{
    const track_t *t = &trk[au.sel % AS_NCIRC];
    uint32_t id = alt_dp_id(r);
    const param_desc_t *d;
    int32_t v;
    *fill = -1;
    if (id == DP_RUMBLE) {
        v = alt_rmb[au.sel % AS_NCIRC];
        *fill = v * 50 / 127;
        alt_num(txt, v, 0);
        return "RUMBLE";
    }
    if (id == DP_DOSE) {
        v = au.cfg[au.sel % AS_NCIRC].dose;
        *fill = v * 50 / 127;
        alt_num(txt, v, 0);
        return "DICE DOSE";
    }
    d = track_desc(t, id);
    v = t->p[id];
    if (alt_slot(id) >= 0)
        v -= alt_applied[au.sel % AS_NCIRC][alt_slot(id)];   /* (the base, without the die's offset) */
    v = clamp(v, d->min, d->max);
    if (d->fmt == F_ENUM && d->names) {
        alt_scopy(txt, d->names[v - d->min], 12);
    } else {
        *fill = d->max > d->min ? (v - d->min) * 50 / (d->max - d->min) : 0;
        if (id == P_PAN)
            alt_num(txt, v, 1);                  /* (-64..63, the middle 0: it showed -64 there until 10/11) */
        else
            alt_num(txt, v, 0);
    }
    if (alt_dp_tab == DP_ENV)
        return DP_ENVN[r % 4u];
    if (alt_dp_tab == DP_MIX)
        return DP_MIXN[r % 4u];
    if (alt_dp_tab == DP_FX && DP_FXN[r % NELEM(DP_FXN)][0])
        return DP_FXN[r % NELEM(DP_FXN)];
    return d->label;                             /* (the engine's own names; the INSERT's A B C by its TYPE) */
}
static void alt_dp_turn(uint32_t r, int32_t s)
{
    uint32_t id = alt_dp_id(r), c = au.sel % AS_NCIRC;
    if (id == DP_RUMBLE) {
        alt_rmb[c] = (uint8_t)clamp((int32_t)alt_rmb[c] + s * 2, 0, 127);
        return;
    }
    if (id == DP_DOSE) {
        au.cfg[c].dose = (uint8_t)clamp((int32_t)au.cfg[c].dose + s * 4, 0, 127);
        alt_post_circle(c);
        return;
    }
    alt_qpush(c, id, track_desc(&trk[c], id)->fmt == F_ENUM ? (s > 0 ? 1 : -1) : s * 2);
}
#define DP_VIS 7u                                /* (rows on the screen: 60 .. 193) */
static void alt_deep_input(void)
{
    int32_t s;
    uint32_t i, n;
    if ((s = panel_enc(EN_ALGO)) != 0) {
        alt_dp_tab = (uint8_t)clamp((int32_t)alt_dp_tab + (s > 0 ? 1 : -1), 0, DP_NT - 1);
        au.set_row = 0;
        alt_dp_top = 0;
    }
    n = alt_dp_rows();
    if ((s = panel_enc(EN_SELECT)) != 0)
        au.set_row = (uint8_t)clamp((int32_t)au.set_row + s, 0, (int32_t)n - 1);
    if (au.set_row >= n)
        au.set_row = (uint8_t)(n - 1u);
    if (au.set_row < alt_dp_top)                 /* (the list follows the row) */
        alt_dp_top = au.set_row;
    if (au.set_row >= alt_dp_top + DP_VIS)
        alt_dp_top = (uint8_t)(au.set_row - DP_VIS + 1u);
    for (i = 0; i < 4u; i++)
        if ((s = panel_enc(EN_K1 + i)) != 0 && i == 0u)
            alt_dp_turn(au.set_row, s);
    (void)panel_enc(EN_PRESET);
    alt_hold = 0;
}
static void alt_deep_scene(void)
{
    uint32_t r, n = alt_dp_rows(), c = au.sel % AS_NCIRC;
    uint16_t col = ALT_COL[c], hl = alt_gold_line(16);
    char txt[16];
    alt_card("DEEP", "XVII");
    cv_text_r(232, 24, ALT_FS, ALT_PRE[au.pre[c] % ALT_NPRE].name, col, ALT_BG);   /* (whose sound) */
    alt_tabs(40, DP_TAB, DP_NT, alt_dp_tab, col);
    for (r = alt_dp_top; r < n && r < alt_dp_top + DP_VIS; r++) {
        int32_t y = 60 + (int32_t)(r - alt_dp_top) * 19, fill;
        const char *nm = alt_dp_row(r, txt, &fill);
        uint16_t bg = r == au.set_row ? hl : ALT_BG;
        if (r == au.set_row) {
            cv_rect(8, y - 3, 224, 18, hl);
            alt_diamond(15, y + 6, 3, 3, col);
        }
        cv_text_on(24, y, ALT_FM, nm, ALT_INK, bg);
        if (fill >= 0) {                         /* a bar under the value */
            cv_rect(176, y + 15, 50, 1, alt_gold_line(30));
            cv_rect(176, y + 15, fill, 1, r == au.set_row ? col : ALT_DIM);
        }
        cv_text_r(226, y, ALT_FM, txt, r == au.set_row ? col : ALT_DIM, bg);
    }
    if (alt_dp_top > 0)                          /* (more above / below) */
        alt_diamond(120, 56, 2, 2, ALT_DIM);
    if (alt_dp_top + DP_VIS < n)
        alt_diamond(120, 192, 2, 2, ALT_DIM);
    cv_rect(8, 196, 224, 1, alt_gold_line(30));
    cv_text(8, 202, ALT_FS, "SELECT: ROW   KNOB 1: CHANGE", ALT_DIM);
    cv_text(8, 216, ALT_FS, "ALGORITHM: TAB   BLACK KEYS: CIRCLE", ALT_DIM);
}

/* THEME: NIGHT / DAY (SETTINGS until 05/11; now SYSTEM's SETUP tab) */
enum { SET_THEME, SET_N };
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
#include "alter_fxpage.c"


static uint32_t alt_now_ms;                      /* (the frame's ms: every rectangle of a frame draws the same) */
/* a filled triangle (screen coordinates, 1/16 px), each row from edge to edge */
static void alt_tri(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t c)
{
    int32_t t, y;
#define ALT_SWAP(a, b) (t = a, a = b, b = t)
    if (y1 < y0) { ALT_SWAP(x0, x1); ALT_SWAP(y0, y1); }
    if (y2 < y0) { ALT_SWAP(x0, x2); ALT_SWAP(y0, y2); }
    if (y2 < y1) { ALT_SWAP(x1, x2); ALT_SWAP(y1, y2); }
#undef ALT_SWAP
    if (y2 == y0 || alt_miss(((x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2)) >> 4) - 1, (y0 >> 4) - 1,
                            ((x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2)) >> 4) + 1, (y2 >> 4) + 1))
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
/* the middle (Romain, 01/11): a triangle cut from its centre into three, one piece per circle in its colour, faint.
 * Still since 09/11 (Romain, after trying a sextant and a watch wheel): no glow on a hit, the triggers do that. Muted: grey. Each piece set in gold (the tarot look); the chosen
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
        int32_t len = mx * mx + my * my, push = 0, ox, oy, k;   /* (still, 09/11: no glow, no push) */
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
        alt_tri(px[0], py[0], px[1], py[1], px[2], py[2], alt_fade(col, au.cfg[i].mute ? 45 : 32));
        for (k = 0; k < 3; k++) {                /* set in gold */
            int32_t k2 = (k + 1) % 3;
            alt_aaline((px[k] + 8) / 16, (py[k] + 8) / 16, (px[k2] + 8) / 16, (py[k2] + 8) / 16, alt_gold_line(75));
        }
        if (i == au.sel)                         /* the chosen circle: a dot in its piece */
            alt_disc((cx + gx + ox) / 16, (cy + gy + oy) / 16, 2, col);
    }
}

/* SMOOTH (08/11, Romain: "des animations plus fluides"; SYSTEM > SETUP > SMOOTH, ON by default): the alidade (the
 * chosen circle's gold hand) turns smoothly, from its POS (alter_lfo.c alt_msrc: its step and the time into it, from
 * the audio's own clock: the tempo, the swing, an external clock as they are), and a trigger that plays grows 2 px,
 * lighter, and shrinks back in 130 ms (Romain: no stars, "they take too much room"). Taken once a frame
 * (alt_sm_take). Drawn as small rectangles round what moves (alt_sm_frame: 6 FPS at first, whole bands of the scene).
 * OFF: the steps as before. */
#define ALT_SMOOTH (alt_sys[8] == 0u)
static void alt_rows(uint32_t *d, int32_t lo, int32_t hi)   /* rows lo .. hi to draw again (a bit each, 240) */
{
    lo = lo < 0 ? 0 : lo;
    hi = hi > 239 ? 239 : hi;
    for (; lo <= hi; lo++)
        d[lo >> 5] |= 1u << (lo & 31);
}
/* VERSION TEST (08/11): the frames sent to the screen in the last second (any change: a band or the whole), shown on
 * the CPU line as "nnF" */
static uint32_t alt_fps, alt_fps_n, alt_fps_ms, alt_full_n;   /* (alt_full_n: whole frames, tests) */
/* (09/11) and how long a frame takes to draw, ms (the time from alter_draw's start to its last blit, the audio's
 * interrupts in it): "28F 9MS". Many ms: the drawing is slow; few ms but few F: something else holds the loop */
static uint32_t alt_draw_t0, alt_draw_us, alt_draw_ms;
static void alt_fps_tick(void)
{
    alt_fps_n++;
    alt_draw_us += (fm1_ticks() - alt_draw_t0) / FM1_TICKS_PER_US;
}
static void alt_fps_poll(void)                   /* (once a second: a still screen counts 0) */
{
    if (fm1_ms - alt_fps_ms >= 1000u) {
        alt_fps = alt_fps_n;
        alt_draw_ms = alt_fps_n ? (alt_draw_us / alt_fps_n + 500u) / 1000u : 0u;
        alt_fps_n = 0;
        alt_draw_us = 0;
        alt_fps_ms = fm1_ms;
    }
}
static uint32_t alt_sm_ang[AS_NCIRC];            /* each circle's turn this frame */
static uint8_t alt_hit_step[AS_NCIRC];           /* the step of each circle's last hit (alter_draw) */
/* SMOOTH (Romain 09/11 night): one hand per circle, in its colour, from the centre to just past its own circle (each
 * circle's playing step sits on its own hand); the chosen circle's 2 px, the others 1 px */
static int32_t alt_hand_r(uint32_t c) { return (int32_t)ALT_R[c % AS_NCIRC] + 6; }
static void alt_alid16c(uint32_t c, uint32_t turn, int32_t *x, int32_t *y)   /* circle c's hand's tip, in 1/16 px */
{
    int32_t r = alt_hand_r(c) * 16;
    *x = ALT_CX * 16 + ((r * sine_i(turn)) >> 15);
    *y = ALT_CY * 16 - ((r * sine_i(turn + 0x40000000u)) >> 15);
}
static uint8_t alt_sm_g[AS_NCIRC];               /* how much it grows now: 2, 1, 0 */
static void alt_sm_take(void)
{
    uint32_t i, t;
    for (i = 0; i < AS_NCIRC; i++) {
        alt_sm_ang[i] = (uint32_t)alt_disp[i] << 17;   /* (Q15 of the circle -> a turn: alter_play.c, even speed) */
        t = alt_now_ms - au.flash_ms[i];
        alt_sm_g[i] = (uint8_t)(!song.playing ? 0u : t < 60u ? 2u : t < 130u ? 1u : 0u);
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
static uint32_t alt_cpu_ms, alt_cpu_avg, alt_cpu_pk, alt_cpu_cut, alt_cpu_c1, alt_cpu_c1b;
static void alt_cpu_poll(void)
{
    if (fm1_ms - alt_cpu_ms < 500u)
        return;
    alt_cpu_ms = fm1_ms;
    alt_cpu_avg = song.cpu_q8 * 100u / 256u;
    alt_cpu_pk = alt_cpu_pk_q8 * 100u / 256u;
    alt_cpu_cut = shed_count + alt_cpu_late;
#if FELUCCA_CPU2
    alt_cpu_c1 = c1_blocks != alt_cpu_c1b;        /* des blocs partagés pendant la dernière demi-seconde */
    alt_cpu_c1b = c1_blocks;
#endif
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
    char b[32];
    uint32_t n = 0;
    uint16_t col = alt_cpu_cut ? ALT_RED : alt_cpu_pk >= 75u ? ALT_GOLD : ALT_DIM;
    b[n++] = 'C', b[n++] = 'P', b[n++] = 'U', b[n++] = ' ';
    alt_cpu_num(b, &n, alt_cpu_avg);
    b[n++] = '/';
    alt_cpu_num(b, &n, alt_cpu_pk);
    b[n++] = '%';
    b[n++] = ' ';                                 /* (08/11: the screen's frames a second) */
    alt_cpu_num(b, &n, alt_fps);
    b[n++] = 'F';
    b[n++] = ' ';
    alt_cpu_num(b, &n, alt_draw_ms);
    b[n++] = 'M', b[n++] = 'S';
#if FELUCCA_CPU2
    if (alt_cpu_c1)                               /* le 2e cœur prend des cercles (MENU > SYSTEM > 2ND CORE) */
        b[n++] = ' ', b[n++] = '2', b[n++] = 'C';
#endif
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

/* circle i's step j of len on its ring (09/11: worked out once a LEN, not for every small rectangle) */
static void alt_step_xy(uint32_t i, uint32_t len, uint32_t j, int32_t *x, int32_t *y)
{
    static int16_t sx[AS_NCIRC][32], sy[AS_NCIRC][32];
    static uint8_t sl[AS_NCIRC];
    i %= AS_NCIRC;
    if (len > 32u || j >= 32u) {
        alt_at((uint32_t)(((uint64_t)j << 32) / (len ? len : 1u)), ALT_R[i], x, y);
        return;
    }
    if (sl[i] != len) {
        uint32_t k;
        for (k = 0; k < len; k++) {
            int32_t a, b;
            alt_at((uint32_t)(((uint64_t)k << 32) / len), ALT_R[i], &a, &b);
            sx[i][k] = (int16_t)a, sy[i][k] = (int16_t)b;
        }
        sl[i] = (uint8_t)len;
    }
    *x = sx[i][j];
    *y = sy[i][j];
}

/* (09/11) the canvas's nearest and farthest pixel from the circles' centre: what lies at a known distance from it
 * (the rays, a ring, its steps, the triangle) is skipped whole when the canvas misses that band */
static int32_t alt_wd0, alt_wd1;
static void alt_band_set(void)
{
    int32_t a0 = -cv_ox - ALT_CX, a1 = (int32_t)cv_w - cv_ox - 1 - ALT_CX, b0 = -cv_oy - ALT_CY,
            b1 = (int32_t)cv_h - cv_oy - 1 - ALT_CY, nx, ny, fx, fy;
    nx = a0 > 0 ? a0 : a1 < 0 ? a1 : 0;
    ny = b0 > 0 ? b0 : b1 < 0 ? b1 : 0;
    fx = -a0 > a1 ? a0 : a1;
    fy = -b0 > b1 ? b0 : b1;
    alt_wd0 = (int32_t)alt_isqrt((uint32_t)(nx * nx + ny * ny));
    alt_wd1 = (int32_t)alt_isqrt((uint32_t)(fx * fx + fy * fy)) + 1;
}
static int alt_band(int32_t lo, int32_t hi) { return hi >= alt_wd0 - 1 && lo <= alt_wd1 + 1; }

#define alt_sm_simple_on (ALT_SMOOTH && alt_sm_simple_v)
static uint32_t alt_sm_simple_v = 1u;              /* (SMOOTH: the halo lighter, 09/11 Romain's choice; 0: as before, tests) */

static void alt_scene(uint32_t q, uint32_t flash)
{
    const track_t *t = &trk[au.sel];
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    char b[24];
    uint32_t i, j, pass;
    alt_band_set();
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
        alt_deep_scene();
        return;
    }
    if (au.mn == QP_GLO) {
        alt_mix_scene();
        return;
    }
    if (au.mn == QP_SYS) {
        alt_sys_scene();
        return;
    }
    if (au.mn == QP_SONG) {
        alt_sg_scene();
        return;
    }
    /* the card's engraving: rays from the centre (under everything, kept off the knobs) */
    {
        static int16_t ray[48][4];               /* (09/11: their ends worked out once) */
        static uint8_t ray_ok;
        uint16_t rc = alt_gold_line(12);
        if (!ray_ok) {
            for (i = 0; i < 48u; i++) {
                uint32_t tn = i * (0xFFFFFFFFu / 48u);
                int32_t cs = sine_i(tn + 0x40000000u), r1 = 98, x0, y0, x1, y1;
                if (cs < 0 && r1 * -cs > (76 << 15))
                    r1 = (76 << 15) / -cs;
                alt_pt(ALT_CX, ALT_CY, tn, 46, &x0, &y0);
                alt_pt(ALT_CX, ALT_CY, tn, r1, &x1, &y1);
                ray[i][0] = (int16_t)x0, ray[i][1] = (int16_t)y0, ray[i][2] = (int16_t)x1, ray[i][3] = (int16_t)y1;
            }
            ray_ok = 1;
        }
        if (alt_band(44, 100))                   /* (they lie from 46 px out to 98) */
            for (i = 0; i < 48u; i++)
                alt_aaline(ray[i][0], ray[i][1], ray[i][2], ray[i][3], rc);
    }
    /* the head: the card, the sound's name in its ribbon, the arcana of the page shown; the tempo, the engine, the
     * mode (SMOOTH 08/11: a piece of the screen away from the card's edges and the head: not walked) */
    if (cv_oy > -40 || cv_ox > -8 || (int32_t)cv_w - cv_ox > 232 || (int32_t)cv_h - cv_oy > 228) {
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
    /* the circles, each a sigil: its ring, its hits joined (a square, a triangle, a star ..), its hits as marks: a
     * diamond (outer), a sun (middle), a dot (inner) */
    for (pass = 0; pass < 3u; pass++) {          /* 0 the alidade (OFF), the rings and their lines; 1 the alidade (SMOOTH);
                                                  * 2 the steps */
    /* the alidade: the chosen circle's step, a gold hand from the centre */
    {
        if (song.playing && (ALT_SMOOTH ? pass == 1u : pass == 0u)) {   /* (SMOOTH: over the rings and lines, under the triggers: drawn
                                                      * under them it was cut where they cross, Romain 08/11) */
            const as_circ_t *c = &alt_seq.c[au.sel];
            uint32_t len = c->cfg.len ? c->cfg.len : 1u, now = (c->pos + len - 1u) % len;
            int32_t x, y;
            if (ALT_SMOOTH) {                        /* (SMOOTH: in 1/16 px, 2 px wide and bright: Romain 08/11 "je perds la
                                                      * ligne des yeux") */
                uint32_t k;
                for (k = 0; k < AS_NCIRC; k++) { /* (the others first, the chosen one over them) */
                    uint32_t ci = (au.sel + 1u + k) % AS_NCIRC;
                    uint16_t col = au.cfg[ci].mute ? ALT_GREY : ALT_COL[ci];
                    alt_alid16c(ci, alt_sm_ang[ci], &x, &y);
                    alt_aaline16(ALT_CX * 16, ALT_CY * 16, x, y, col);
                    if (ci != au.sel)
                        continue;
                    if ((x > ALT_CX * 16 ? x - ALT_CX * 16 : ALT_CX * 16 - x) > (y > ALT_CY * 16 ? y - ALT_CY * 16 : ALT_CY * 16 - y))
                        alt_aaline16(ALT_CX * 16, ALT_CY * 16 + 16, x, y + 16, col);
                    else
                        alt_aaline16(ALT_CX * 16 + 16, ALT_CY * 16, x + 16, y, col);
                }
            } else {
                alt_at((uint32_t)(((uint64_t)now << 32) / len), 90, &x, &y);
                alt_aaline(ALT_CX, ALT_CY, x, y, alt_gold_line(45));
            }
        }
        if (pass == 1u)
            continue;
    }
    for (i = 0; i < AS_NCIRC; i++) {
        const as_circ_t *c = &alt_seq.c[i];
        uint32_t len = c->cfg.len ? c->cfg.len : 1u, now = (c->pos + len - 1u) % len, m = 0, st;
        int muted = au.cfg[i].mute, dim = q == QP_SEQ && i != au.sel, chosen = i == au.sel;
        uint16_t col = muted ? ALT_GREY : ALT_COL[i];
        int32_t r = ALT_R[i], x, y, hx[32], hy[32];
        if (dim)
            col = alt_fade(col, 30);
        if (pass == 0u) {
        if (!alt_band(r - 10, r + 10))           /* (09/11: the ring and its halo away from the canvas: skipped) */
            ;
        else if (chosen) {                       /* the chosen circle's in its colour: thick, a halo round it (Romain
                                                  * 05/11: the gold third one stood out too little from the gold lines) */
            int32_t d;
            for (d = 6; d >= 2; d -= alt_sm_simple_on ? 2 : 1) {   /* (SMOOTH 09/11: every other one, a little
                                                  * stronger: the halo in 6 rings, not 10) */
                uint16_t hc = alt_fade(col, (52 - d * 7) + (alt_sm_simple_on ? 6 : 0));
                alt_aaring16(ALT_CX * 16, ALT_CY * 16, r * 16 + d * 12, hc);
                alt_aaring16(ALT_CX * 16, ALT_CY * 16, r * 16 - d * 12, hc);
            }
            for (d = -1; d <= 1; d++)
                alt_aaring16(ALT_CX * 16, ALT_CY * 16, r * 16 + d * 12, col);
        } else
            alt_aaring16(ALT_CX * 16, ALT_CY * 16, r * 16, alt_gold_line(muted ? 30 : 55));   /* the ring: gold */
        for (j = 0; j < len && j < 32u; j++)
            if ((c->pat >> j) & 1u)
                alt_step_xy(i, len, j, &hx[m], &hy[m]), m++;
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
            for (k = 0; k < (m == 2u ? 1u : m); k++) {   /* (first a contour in the background's colour: each
                                                  * circle's lines stand apart from what is under them, Romain 05/11) */
                uint32_t nx = (cur + st) % m;
                alt_aaline(hx[cur] - 1, hy[cur], hx[nx] - 1, hy[nx], ALT_BG);
                alt_aaline(hx[cur] + 1, hy[cur], hx[nx] + 1, hy[nx], ALT_BG);
                alt_aaline(hx[cur], hy[cur] - 1, hx[nx], hy[nx] - 1, ALT_BG);
                alt_aaline(hx[cur], hy[cur] + 1, hx[nx], hy[nx] + 1, ALT_BG);
                cur = nx;
            }
            cur = 0;
            for (k = 0; k < (m == 2u ? 1u : m); k++) {
                uint32_t nx = (cur + st) % m;
                alt_aaline(hx[cur], hy[cur], hx[nx], hy[nx], i ? alt_fade(col, 40) : alt_gold_line(muted ? 20 : 40));
                cur = nx;
            }
        }
        }
        if (pass != 2u || !alt_band(r - 12, r + 22))   /* (the steps and the die's triangle: round the ring) */
            continue;
        for (j = 0; j < len; j++) {              /* the steps */
            int on = (int)((c->pat >> j) & 1u), here = song.playing && j == now && !ALT_SMOOTH;
            int32_t g = ALT_SMOOTH && on && j == alt_hit_step[i] ? alt_sm_g[i] : 0;   /* (SMOOTH: it plays: grows) */
            uint16_t cg = g ? alt_mixc(col, ALT_INK, g * 30) : col;
            alt_step_xy(i, len, j, &x, &y);
            if (on)                              /* (a contour: the hit apart from its ring, Romain 05/11) */
                alt_disc(x, y, (i == 2u ? 4 : 6) + g, ALT_BG);
            if (here && on)
                alt_star(x, y, (flash >> i) & 1u ? 9 : 7, ALT_INK);
            if (!on) {
                cv_rect(x - 1, y - 1, 2, 2, here ? ALT_INK : alt_gold_line(muted ? 25 : 45));
                continue;
            }
            if (i == 0u) {
                alt_diamond(x, y, 4 + g, 6 + g, alt_mixc(ALT_BG, ALT_INK, 70));
                alt_diamond(x, y, 3 + g, 5 + g, cg);
            } else if (i == 1u) {
                alt_disc(x, y, 4 + g, cg);
                alt_disc(x, y, 2, ALT_BG);
                cv_rect(x, y, 1, 1, ALT_INK);
            } else
                alt_disc(x, y, 2 + g, cg);
        }
        if (c->die) {                            /* a draw on: a small triangle at the circle's left, pointing in */
            int32_t k2;
            alt_at(0xC0000000u, r, &x, &y);
            for (k2 = 0; k2 < 7; k2++)
                cv_rect(x - 16 + k2, y - (6 - k2), 1, 2 * (6 - k2) + 1, ALT_COL[i]);
        }
    }
    }
    /* the middle: the key held or tapped, else the three circles' triangle in its gold circle */
    if (au.msg && fm1_ms - au.msg_ms < 900u) {
        int32_t w = text_w(ALT_FS, au.msg);
        cv_rect(ALT_CX - w / 2 - 5, ALT_CY - 10, w + 10, 20, ALT_BG);
        cv_frame(ALT_CX - w / 2 - 5, ALT_CY - 10, w + 10, 20, ALT_GOLD);
        cv_text_on(ALT_CX - w / 2, ALT_CY - 7, ALT_FS, au.msg, ALT_INK, ALT_BG);
    } else if (alt_band(0, 32)) {                /* (the triangle and its ring: 27 px out at most) */
        alt_aaring16(ALT_CX * 16, ALT_CY * 16, 27 * 16, alt_gold_line(45));
        alt_prism();
    }
    if (sg_on && asg.n >= 2u) {                  /* SONG (Romain 05/11): only while a chain loops, on the right, top to
                                                  * bottom: its links, the one playing filled in its slot's colour */
        uint32_t k, n = asg.n < 8u ? asg.n : 8u;
        for (k = 0; k < n; k++) {
            int32_t x = 222, y = 48 + (int32_t)k * 17;
            uint32_t sl = asg.chain[k] & 3u;
            int on = k == sg_link;
            char nm[2] = {(char)('A' + sl), 0};
            uint16_t c = ALT_COL[sl];
            if (on)
                cv_rect(x, y, 13, 14, c);
            else
                cv_frame(x, y, 13, 14, alt_fade(c, 60));
            cv_text_on(x + 7 - text_w(ALT_FS, nm) / 2, y + 1, ALT_FS, nm, on ? ALT_BG : alt_fade(c, 70), on ? c : ALT_BG);
        }
        if (asg.n > 8u)
            cv_text(222, 48 + 8 * 17, ALT_FS, "..", ALT_DIM);
    }
    if ((int32_t)cv_h - cv_oy > 192)             /* (the knobs: from row 194 down) */
        alt_foot(q);
}


/* the card's grain (DAY: paper over the cream; NIGHT: ink over the black), the same every frame (a hash of the pixel's place, not a random draw), in
 * three tones near ALT_BG; half h of the screen (rows h*120 ..), straight into the canvas just cleared */
/* (05/11, Romain: "laggy") the grain's shades precomputed once: a tile of 128 x 64 pixels, 2 bits each (the hash it
 * was drawn with every frame, a third of a frame's cost), then only looked up; rows y0 .. y0 + n of the screen */
#define ALT_GR_W 128u
#define ALT_GR_H 64u
static uint8_t alt_grain[ALT_GR_W * ALT_GR_H / 4u] __attribute__((section(".pool")));
static uint8_t alt_grain_ok;
static void alt_paper_rect(uint32_t x0, uint32_t y0, uint32_t w, uint32_t n);
static void alt_paper_rows(uint32_t y0, uint32_t n) { alt_paper_rect(0, y0, 240u, n); }
static void alt_paper_rect(uint32_t x0, uint32_t y0, uint32_t w, uint32_t n)   /* (x0, y0, w x n: the canvas) */
{
    uint16_t t[4];
    uint32_t x, y, i = 0;
    if (!alt_grain_ok) {
        for (x = 0; x < sizeof alt_grain; x++)   /* (the pool is not cleared at power-on) */
            alt_grain[x] = 0;
        for (y = 0; y < ALT_GR_H; y++)
            for (x = 0; x < ALT_GR_W; x++) {
                uint32_t v = (x * 0x85EBCA77u) ^ (y * 0x9E3779B1u), k;
                v ^= v >> 15;
                v *= 0x2C1B3C6Du;
                v ^= v >> 13;
                v &= 15u;                        /* 3 in 16 a shade darker, 1 in 16 darker still, 2 in 16 lighter */
                k = v < 3u ? 1u : v == 3u ? 2u : v < 6u ? 3u : 0u;
                alt_grain[(y * ALT_GR_W + x) >> 2] |= (uint8_t)(k << ((x & 3u) * 2u));
            }
        alt_grain_ok = 1;
    }
    t[0] = swap16(ALT_BG);
    t[1] = swap16(ALT_DAY ? RGB(234, 220, 188) : RGB(19, 16, 20));
    t[2] = swap16(ALT_DAY ? RGB(225, 209, 175) : RGB(9, 8, 10));
    t[3] = swap16(ALT_DAY ? RGB(249, 239, 213) : RGB(23, 20, 22));
    for (y = 0; y < n; y++) {
        const uint8_t *row = &alt_grain[((y0 + y) % ALT_GR_H) * (ALT_GR_W / 4u)];
        for (x = x0; x < x0 + w; x++, i++)
            cv_px[i] = t[(row[(x % ALT_GR_W) >> 2] >> ((x & 3u) * 2u)) & 3u];
    }
}
static void alt_paper(uint32_t h) { alt_paper_rows(h * 120u, 120u); }


/* SMOOTH's frame (08/11, "6F": a band of the whole width each star, the scene walked whole each band; Romain: draw
 * only what moves): rectangles round the alidade's old and new place (in six pieces along it), round a trigger that
 * grows or shrinks, round the centre's pieces when they glow; each drawn alone (cv_ox, cv_oy), the scene skipping
 * what misses it (alt_miss). What was drawn: alt_sm_seen. Returns 1 if anything was sent. */
typedef struct {
    int16_t x0, y0, x1, y1;                      /* (inclusive) */
} alt_rc_t;
static alt_rc_t alt_rc[12];
static uint32_t alt_nrc;
static uint32_t alt_sm_dang[AS_NCIRC];           /* the hands drawn: their turns, on or not */
static uint8_t alt_sm_don, alt_sm_dhs[AS_NCIRC], alt_sm_dg[AS_NCIRC];
static uint32_t alt_sm_foot, alt_sm_head, alt_sm_dfoot, alt_sm_dhead, alt_sm_footk[4], alt_sm_dfootk[4];   /* the knobs', the CPU line's sig: now, drawn */
static void alt_rc_add(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    uint32_t k;
    x0 = clamp(x0, 0, 239) & ~1;
    y0 = clamp(y0, 0, 239) & ~1;
    x1 = clamp(x1, 0, 239) | 1;
    y1 = clamp(y1, 0, 239) | 1;
    for (k = 0; k < alt_nrc; k++) {              /* (two as one only when that adds little: the boxes along a slanted
                                                  * alidade touch, their union would be its whole square) */
        alt_rc_t *r = &alt_rc[k];
        int32_t ux0 = x0 < r->x0 ? x0 : r->x0, uy0 = y0 < r->y0 ? y0 : r->y0, ux1 = x1 > r->x1 ? x1 : r->x1,
                uy1 = y1 > r->y1 ? y1 : r->y1;
        if ((ux1 - ux0 + 1) * (uy1 - uy0 + 1) <=
            (x1 - x0 + 1) * (y1 - y0 + 1) + (r->x1 - r->x0 + 1) * (r->y1 - r->y0 + 1) + 64) {
            x0 = x0 < r->x0 ? x0 : r->x0;
            y0 = y0 < r->y0 ? y0 : r->y0;
            x1 = x1 > r->x1 ? x1 : r->x1;
            y1 = y1 > r->y1 ? y1 : r->y1;
            *r = alt_rc[--alt_nrc];
            k = (uint32_t)-1;
        }
    }
    if (alt_nrc == NELEM(alt_rc)) {              /* (full: into the last) */
        alt_rc_t *r = &alt_rc[alt_nrc - 1u];
        x0 = x0 < r->x0 ? x0 : r->x0;
        y0 = y0 < r->y0 ? y0 : r->y0;
        x1 = x1 > r->x1 ? x1 : r->x1;
        y1 = y1 > r->y1 ? y1 : r->y1;
        alt_nrc--;
    }
    alt_rc[alt_nrc].x0 = (int16_t)x0, alt_rc[alt_nrc].y0 = (int16_t)y0;
    alt_rc[alt_nrc].x1 = (int16_t)x1, alt_rc[alt_nrc].y1 = (int16_t)y1;
    alt_nrc++;
}
static void alt_sm_seen(void)
{
    uint32_t i;
    alt_sm_don = (uint8_t)song.playing;
    for (i = 0; i < AS_NCIRC; i++)
        alt_sm_dang[i] = alt_sm_ang[i];
    for (i = 0; i < AS_NCIRC; i++)
        alt_sm_dhs[i] = alt_hit_step[i], alt_sm_dg[i] = alt_sm_g[i];
    alt_sm_dfoot = alt_sm_foot;
    alt_sm_dhead = alt_sm_head;
    for (i = 0; i < 4u; i++)
        alt_sm_dfootk[i] = alt_sm_footk[i];
}
static uint32_t alt_sm_seg = 2u, alt_sm_nrc, alt_sm_area;   /* (the alidade's boxes; rectangles, pixels drawn: tests) */
static void alt_rc_alid(uint32_t c, uint32_t turn)   /* circle c's hand at turn: boxes along it (2 for the
                                                  * long ones: fewest walks of the scene for the pixels, measured 09/11) */
{
    int32_t k, xa, ya, xb, yb, R = alt_hand_r(c), n = R > 50 ? (int32_t)alt_sm_seg : 1, st = R / n;
    for (k = 0; k < n; k++) {
        alt_at(turn, k * st, &xa, &ya);
        alt_at(turn, k == n - 1 ? R : k * st + st, &xb, &yb);
        alt_rc_add((xa < xb ? xa : xb) - 3, (ya < yb ? ya : yb) - 3, (xa > xb ? xa : xb) + 3, (ya > yb ? ya : yb) + 3);
    }
}
static void alt_rc_step(uint32_t i, uint32_t j)  /* circle i's step j, its mark grown */
{
    uint32_t len = alt_seq.c[i].cfg.len ? alt_seq.c[i].cfg.len : 1u;
    int32_t x, y;
    if (j >= len)
        return;
    alt_at((uint32_t)(((uint64_t)j << 32) / len), ALT_R[i], &x, &y);
    alt_rc_add(x - 11, y - 11, x + 11, y + 11);
}
static uint32_t alt_sm_cap = 28u;                /* (ms between two frames at least: ~30 a second, even; 0: none, tests) */
static int alt_sm_frame(uint32_t q, uint32_t flash)
{
    static uint32_t last_ms;
    uint32_t i, k, ang[AS_NCIRC], keep[AS_NCIRC];
    if (alt_now_ms - last_ms < alt_sm_cap)       /* (the main loop comes every 15 ms: a frame every other one) */
        return 0;
    last_ms = alt_now_ms;
    alt_nrc = 0;
    for (i = 0; i < AS_NCIRC; i++) {
        ang[i] = alt_sm_ang[i];
        if (song.playing && ang[i] != alt_sm_dang[i]) {   /* (a pixel's worth of turn or less: the same line) */
            int32_t xa, ya, xb, yb;
            alt_alid16c(i, ang[i], &xa, &ya);
            alt_alid16c(i, alt_sm_dang[i], &xb, &yb);
            if (xa != xb || ya != yb || !alt_sm_don) {
                if (alt_sm_don)
                    alt_rc_alid(i, alt_sm_dang[i]);
                alt_rc_alid(i, ang[i]);
            } else
                ang[i] = alt_sm_dang[i];
        }
    }
    for (i = 0; i < AS_NCIRC; i++)
        if (alt_hit_step[i] != alt_sm_dhs[i] || alt_sm_g[i] != alt_sm_dg[i]) {
            alt_rc_step(i, alt_sm_dhs[i]);
            alt_rc_step(i, alt_hit_step[i]);
        }
    for (i = 0; i < 4u; i++)                     /* (a knob: its value, an LFO on it; itself alone, 09/11) */
        if (alt_sm_footk[i] != alt_sm_dfootk[i])
            alt_rc_add(4 + (int32_t)i * 57, 192, 64 + (int32_t)i * 57, 239);
    if (alt_sm_head != alt_sm_dhead)             /* (the CPU line, top left) */
        alt_rc_add(6, 16, 196, 48);
    for (i = 0; i < AS_NCIRC; i++)               /* (unmoved: drawn where it was) */
        keep[i] = alt_sm_ang[i], alt_sm_ang[i] = ang[i];
    alt_sm_seen();
    for (i = 0; i < AS_NCIRC; i++)
        alt_sm_ang[i] = keep[i];
    if (!alt_nrc)
        return 0;
    alt_sm_nrc += alt_nrc;
    for (k = 0; k < alt_nrc; k++)
        alt_sm_area += (uint32_t)(alt_rc[k].x1 - alt_rc[k].x0 + 1) * (uint32_t)(alt_rc[k].y1 - alt_rc[k].y0 + 1);
    alt_draw_gl_n = alt_gl_n;
    alt_draw_lfo_p = alt_lfo_p;
    for (k = 0; k < alt_nrc; k++) {
        const alt_rc_t *r = &alt_rc[k];
        uint32_t w = (uint32_t)(r->x1 - r->x0 + 1), y = (uint32_t)r->y0, h = (uint32_t)(r->y1 + 1);
        for (i = 0; i < AS_NCIRC; i++)
            keep[i] = alt_sm_ang[i], alt_sm_ang[i] = alt_sm_dang[i];
        while (y < h) {
            uint32_t n = h - y, cap = (CV_MAX / w) & ~1u;
            n = n > cap ? cap : n;
            cv_begin(w, n, ALT_BG);
            alt_paper_rect((uint32_t)r->x0, y, w, n);
            cv_ox = -r->x0;
            cv_oy = -(int32_t)y;
            alt_scene(q, flash);
            cv_ox = 0;
            cv_oy = 0;
            cv_blit((uint32_t)r->x0, y);
            y += n;
        }
        for (i = 0; i < AS_NCIRC; i++)
            alt_sm_ang[i] = keep[i];
    }
    return 1;
}

static uint32_t alt_mat_ms;                       /* (the LFO tabs' wave: drawn when) */
/* the LFO tab's wave shown (the LFO page open, not MTX, nothing over it) */
static int alt_lw_vis(void)
{
    return !au.lockc && !au.scene_open && au.mat_open && au.mtab != ALT_TAB_MTX;
}
/* a frame of the LFO tab's wave (Romain 10/11): only the columns whose wave moved, each from the lowest to the highest
 * row it took or leaves (4 columns a piece: 8 = +25 % pixels, 2 = -15 % for twice the pieces), and the dot; the same pixels as a whole picture. A list dropped down over
 * it: the band whole, as before. 1 if something was sent */
static int alt_lw_frame(uint32_t q, uint32_t flash)
{
    uint32_t tb = au.mtab, n = 0;
    int32_t x0, x, lo, hi, a, b;
    alt_lw_calc(tb);
    if (!alt_lw_ok || alt_lw_tb != tb || (au.dd && fm1_ms - au.dd_ms < ALT_DD_MS)) {
        uint32_t y = 42u;                        /* (the band whole: the rows of the wave and the dot) */
        while (y < 166u) {
            uint32_t h = 166u - y > 120u ? 120u : 166u - y;
            cv_begin(240, h, ALT_BG);
            alt_paper_rows(y, h);
            cv_oy = -(int32_t)y;
            alt_scene(q, flash);
            cv_oy = 0;
            cv_blit(0, y);
            y += h;
        }
        alt_lw_seen(tb);
        return 1;
    }
    for (x0 = -4; x0 < ALT_LW_W; x0 += 4) {      /* (x0 -4: the dot's piece, round the dot where it was and is) */
        int32_t rx0, rx1, moved = 0;
        lo = 999, hi = -999;
        if (x0 < 0) {
            rx0 = ALT_LW_BX + 1 - ALT_LW_R - 2, rx1 = ALT_LW_BX + 1 + ALT_LW_R + 2;
            moved = alt_lw_dot != alt_lw_pdot;
            lo = (alt_lw_dot < alt_lw_pdot ? alt_lw_dot : alt_lw_pdot) - ALT_LW_R - 2;
            hi = (alt_lw_dot > alt_lw_pdot ? alt_lw_dot : alt_lw_pdot) + ALT_LW_R + 2;
        } else {
            rx0 = ALT_LW_X0 + x0;
            rx1 = rx0 + 3;
            if (rx1 > ALT_LW_X0 + ALT_LW_W - 1)
                rx1 = ALT_LW_X0 + ALT_LW_W - 1;
        }
        for (x = rx0 - ALT_LW_X0; x <= rx1 - ALT_LW_X0; x++) {   /* (the wave's columns in the piece) */
            int32_t k;
            if (x < 0)
                continue;
            for (k = x - 2; k <= x; k++)         /* (a column's rows hang on its own and the two before) */
                if (k < 0 ? alt_lw_dot != alt_lw_pdot : alt_lw_y[k] != alt_lw_py[k])
                    moved = 1;
            alt_lw_span(alt_lw_y, alt_lw_dot, x, &a, &b);
            lo = a < lo ? a : lo, hi = b > hi ? b : hi;
            alt_lw_span(alt_lw_py, alt_lw_pdot, x, &a, &b);
            lo = a < lo ? a : lo, hi = b > hi ? b : hi;
        }
        if (!moved)
            continue;
        if (x0 < 0 && alt_lw_dot != alt_lw_pdot) {   /* (the dot's piece: the dot where it was and is) */
            int32_t d0 = (alt_lw_dot < alt_lw_pdot ? alt_lw_dot : alt_lw_pdot) - ALT_LW_R - 2;
            int32_t d1 = (alt_lw_dot > alt_lw_pdot ? alt_lw_dot : alt_lw_pdot) + ALT_LW_R + 2;
            lo = d0 < lo ? d0 : lo, hi = d1 > hi ? d1 : hi;
        }
        if (lo < ALT_LW_Y0 - ALT_LW_R - 2)
            lo = ALT_LW_Y0 - ALT_LW_R - 2;
        if (hi > ALT_LW_Y0 + ALT_LW_H + ALT_LW_R + 1)
            hi = ALT_LW_Y0 + ALT_LW_H + ALT_LW_R + 1;
        {
            uint32_t w = (uint32_t)(rx1 - rx0 + 1), y = (uint32_t)lo, h = (uint32_t)(hi - lo + 1);
            cv_begin(w, h, ALT_BG);
            alt_paper_rect((uint32_t)rx0, y, w, h);
            cv_ox = -rx0;
            cv_oy = -lo;
            alt_lw_draw(tb, au.mtab == ALT_TAB_GL ? ALT_INK : ALT_COL[au.sel % AS_NCIRC]);
            cv_ox = 0;
            cv_oy = 0;
            cv_blit((uint32_t)rx0, y);
            n++;
        }
    }
    alt_lw_seen(tb);
    return n != 0;
}

static void alter_draw(void)
{
    uint32_t sig = 0, sfoot = 0, shead = 0, i, h, flash = 0, q = au.lockc ? 0u : alt_page();
    alt_now_ms = fm1_ms;
    alt_draw_t0 = fm1_ticks();
    for (i = 0; i < AS_NCIRC; i++) {
        if ((h = alt_hits[i]) != au.hits_seen[i]) {
            uint32_t len = alt_seq.c[i].cfg.len ? alt_seq.c[i].cfg.len : 1u;
            au.hits_seen[i] = h;
            au.flash_ms[i] = alt_now_ms;
            alt_hit_step[i] = (uint8_t)((alt_seq.c[i].pos + len - 1u) % len);   /* (SMOOTH: the trigger that grows) */
        }
        if (alt_now_ms - au.flash_ms[i] < 70u)
            flash |= 1u << i;
    }
    alt_sm_take();
    for (i = 0; i < AS_NCIRC; i++)               /* what the picture depends on (the steps playing: apart, below) */
        sig = sig * 31u + alt_seq.c[i].pat * 7u + (uint32_t)alt_seq.c[i].cfg.len * 131u +
              au.cfg[i].len + au.cfg[i].hits * 3u + au.cfg[i].rot * 5u + au.cfg[i].prob * 9u + au.cfg[i].mute * 11u +
              alt_seq.c[i].die * 13u + trk[i].eng_req * 17u;
    for (i = 0; i < 4u; i++) {
        const char *nm;
        char tx[12];
        int32_t f;
        uint32_t sk = (uint32_t)alt_kv(q, i, &nm, tx, &f);   /* (the knobs: apart, SMOOTH draws each alone) */
        if (f >= 0 && !au.mat_open && !au.scene_open && !au.lockc) {
            int32_t lo, hi, pc;
            alt_live_bar(q, i, f, &lo, &hi, &pc);
            sk = sk * 31u + (uint32_t)(lo * 977 + hi * 31 + 7);   /* (the range) */
        }
        sk = sk * 31u + (uint32_t)alt_knob_lfo(q, i);
        sk = sk * 31u + ((alt_dial_live() >> i) & 1u) * 83u;
        alt_sm_footk[i] = sk;
        sfoot = sfoot * 31u + sk;
    }
    sig = sig * 31u + anm.open * 3u + anm.ch[0] * 5u + anm.ch[1] * 7u + anm.ch[2] * 11u;   /* (the NAME page) */
    sig = sig * 31u + alt_emode[au.sel] * 37u + alt_seq.scale * 41u + au.scene_open * 43u + au.row[au.tab] * 47u + au.tab * 59u +
          au.pre[au.sel] * 53u;
    sig = sig * 31u + (au.mat_open ? alt_mat_sig() * 2u + 1u : 0u) + (au.seq_open ? alt_seq_sig() * 4u + 3u : 0u) +
          (afp.open ? afp_sig() * 8u + 5u : 0u);
    if (au.mn == QP_GLO) {                       /* (the MIXER: its faders and meter) */
        alt_mix_poll();
        sig = sig * 31u + (uint32_t)alt_mix_pk / 600u * 7u + (uint32_t)alt_mix_hold / 600u * 11u + alt_mgain * 13u;
        for (i = 0; i < AS_NCIRC; i++)
            sig = sig * 31u + (uint32_t)trk[i].p[P_LEVEL] + alt_seq.c[i].cfg.mute * 128u;
        sig = sig * 31u + alt_mx_tab * 3u + alt_mx_row * 5u;   /* (the CONSOLE: its tab, row, values, the COMP's meter) */
        if (alt_mx_tab) {
            uint32_t cc = (alt_mx_tab - 1u) % AS_NCIRC, k;
            for (k = 0; k < 4u; k++)
                sig = sig * 31u + (uint32_t)alt_mx_val(cc, 0, k) * 7u + (uint32_t)alt_mx_val(cc, 1, k) * 11u;
            sig = sig * 31u + (uint32_t)(-alt_db_q12(acs_gr[cc]));
        }
    }
    if (au.mn == QP_EDIT) {                      /* (DEEP: its tab, its list, the chosen circle's values) */
        uint32_t k, c = au.sel % AS_NCIRC;
        sig = sig * 31u + alt_dp_tab * 7u + alt_dp_top * 11u + alt_rmb[c] + au.pre[c] * 13u;
        for (k = 0; k < P_COUNT; k++)
            sig = sig * 31u + (uint32_t)(uint16_t)trk[c].p[k];
    }
    if (au.mn == QP_SYS)                         /* (SYSTEM: its tab, values, the battery) */
    {
        uint32_t k;
        for (k = 0; k < SYS_NAUD; k++)
            sig = sig * 31u + menu_get(SYS_AROW[k]);
        sig = sig * 31u + alt_midi_n[1] * 3u + alt_midi_n[2] * 5u
#if FELUCCA_UART
              + um.bytes * 13u
#endif
               + (fm1_ms - alt_midi_clk_ms[1] < 500u) * 7u +
              (fm1_ms - alt_midi_clk_ms[2] < 500u) * 11u;   /* (TRS / USB SEEN) */
        sig = sig * 31u + (uint32_t)alt_sys_scroll * 3u;
        if (alt_sys_tab == SYS_TLEARN)
            sig = sig * 31u + alm_sig();
    }
    if (au.mn == QP_SYS)
        sig = sig * 31u + alt_sys_top * 5u + alt_sys_tab * 7u + alt_sys[1] * 11u + alt_sys[2] * 13u + alt_sys[8] * 3u + (uint32_t)song.g[G_CLOCK] * 17u +
              (uint32_t)song.g[G_ROUTE] * 19u + (uint32_t)batt_shown() * 23u
#if FELUCCA_CPU2
              + menu_get(MI_CPU2) * 29u
#endif
              ;
    sig = sig * 31u + au.mn * 61u + au.set_row * 67u + (uint32_t)ALT_DAY * 71u + au.cfg[au.sel % AS_NCIRC].chord * 73u;

    sig = sig * 31u + (uint32_t)ar_on * 89u + (ar_n[au.sel % AS_NCIRC] != 0) * 97u;
    sig = sig * 31u + sg_play * 3u + sg_link * 5u + sg_on * 7u + (asg.q ? asg.q * 11u + ((fm1_ms >> 7) & 1u) : 0u) +
          sg_slot[0].used * 13u + sg_slot[1].used * 17u + sg_slot[2].used * 19u + asg.n * 23u;   /* (SONG's corner) */
    if (au.mn == QP_SONG) {                      /* (SONG: the slots, the chain, where it is) */
        uint32_t k;
        sig = sig * 31u + sg_play * 3u + sg_link * 5u + sg_left * 7u + sg_on * 11u + asg.q * 13u + asg.row * 17u + asg.n * 19u +
              sg_slot[0].used * 23u + sg_slot[1].used * 29u + sg_slot[2].used * 31u + (uint32_t)song.playing * 37u;
        for (k = 0; k < asg.n; k++)
            sig = sig * 31u + asg.chain[k];
    }   /* (REC's mark) */     /* (a number shown while its knob turns, then gone) */
    sig = sig * 31u + q * 19u + au.lockc * 23u + alt_seq.c[au.lockc ? au.lockc - 1u : 0u].locks * 29u;
    sig = sig * 31u + au.sel * 3u + (uint32_t)song.playing * 7u + trk[au.sel].preset * 11u +
          (uint32_t)song.g[G_BPM] * 13u + (au.msg && fm1_ms - au.msg_ms < 1500u ? (uint32_t)(uintptr_t)au.msg + (fm1_ms - au.msg_ms < 900u) : 1u);
    alt_fps_poll();
#if ALTER_CPU_SHOW
    alt_cpu_poll();
    shead = shead * 31u + alt_fps * 113u + alt_draw_ms * 127u + alt_cpu_avg * 101u + alt_cpu_pk * 103u + alt_cpu_cut * 107u + alt_cpu_c1 * 109u;   /* (version test : la charge CPU) */
#endif
    ui.force = 1;                                /* (whatever comes after this view draws itself whole) */
    {                                            /* the steps playing and the hits' flash: a page that shows them has
                                                  * them in its sig; the circles' view redraws only the rows they move
                                                  * on (Romain 05/11: "laggy": a whole frame each step) */
        static uint8_t now_seen[AS_NCIRC], fl_seen;
        uint8_t now[AS_NCIRC];
        int circles = !au.lockc && !au.scene_open && !au.mat_open && !au.seq_open && !afp.open && !au.mn;
        for (i = 0; i < AS_NCIRC; i++) {
            uint32_t len = alt_seq.c[i].cfg.len ? alt_seq.c[i].cfg.len : 1u;
            now[i] = (uint8_t)((alt_seq.c[i].pos + len - 1u) % len);
        }
        alt_sm_foot = sfoot;
        alt_sm_head = shead;
        if (!(ALT_SMOOTH && circles) && !au.mat_open)   /* (SMOOTH's circles: the knobs and the CPU line drawn alone
                                                  * when they change; else a whole frame, 2 a second: the alidade stopped.
                                                  * The LFO page has neither: its own dials, in alt_mat_sig; 10/11: the CPU
                                                  * line's 2 a second were whole screens there, the wave stopped) */
            sig = (sig * 31u + sfoot) * 31u + shead;
        if (!circles && !au.mat_open && au.mn != QP_EDIT && au.mn != QP_SYS) {   /* (another page: they are part of the
                                                  * picture; not the LFO / MTX menu, DEEP, SYSTEM: a whole screen each step
                                                  * for nothing there, Romain 10/11 "LFO: low frame rate") */
            for (i = 0; i < AS_NCIRC; i++)
                sig = sig * 31u + alt_seq.c[i].pos;
            sig = sig * 31u + flash * 5u;
        }
        if (circles && au.drawn && sig == au.sig) {
            if (ALT_SMOOTH) {
                if (alt_sm_frame(q, flash))
                    alt_fps_tick();
                return;
            }
            {
            uint32_t dirty[8] = {0, 0, 0, 0, 0, 0, 0, 0};
            int32_t y = 0;
            int any = 0;
                for (i = 0; i < AS_NCIRC; i++)   /* the rows the old and new marks of each circle that moved cover */
                    if (now[i] != now_seen[i] || ((flash ^ fl_seen) >> i & 1u)) {
                        uint32_t len = alt_seq.c[i].cfg.len ? alt_seq.c[i].cfg.len : 1u, k;
                        for (k = 0; k < 2u; k++) {
                            int32_t x, yy, rr = ALT_R[i];
                            alt_at((uint32_t)(((uint64_t)(k ? now[i] : now_seen[i]) << 32) / len), rr, &x, &yy);
                            alt_rows(dirty, yy - 11, yy + 11);
                            if (i == au.sel) {   /* (the alidade: from the centre out, 90 px) */
                                alt_at((uint32_t)(((uint64_t)(k ? now[i] : now_seen[i]) << 32) / len), 90, &x, &yy);
                                alt_rows(dirty, (yy < ALT_CY ? yy : ALT_CY) - 2, (yy > ALT_CY ? yy : ALT_CY) + 2);
                            }
                        }
                    }
            for (i = 0; i < AS_NCIRC; i++)
                now_seen[i] = now[i];
            fl_seen = (uint8_t)flash;
            alt_draw_gl_n = alt_gl_n;
            alt_draw_lfo_p = alt_lfo_p;
            while (y < 240) {                    /* each band of rows to draw again (rows apart by 16 or less: one
                                                  * band), in pieces the canvas holds */
                int32_t lo = y, hi = y, k;
                if (!((dirty[y >> 5] >> (y & 31)) & 1u)) {
                    y++;
                    continue;
                }
                for (k = y + 1; k < 240 && k - hi <= 16; k++)
                    if ((dirty[k >> 5] >> (k & 31)) & 1u)
                        hi = k;
                y = hi + 1;
                lo &= ~1;
                hi = (hi + 2) & ~1;
                if (hi > 240)
                    hi = 240;
                any = 1;
                while (lo < hi) {
                    uint32_t n = (uint32_t)(hi - lo) > 120u ? 120u : (uint32_t)(hi - lo);
                    cv_begin(240, n, ALT_BG);
                    alt_paper_rows((uint32_t)lo, n);
                    cv_oy = -lo;
                    alt_scene(q, flash);
                    cv_oy = 0;
                    cv_blit(0, (uint32_t)lo);
                    lo += (int32_t)n;
                }
            }
            if (any)
                alt_fps_tick();
            return;
            }
        }
        for (i = 0; i < AS_NCIRC; i++)
            now_seen[i] = now[i];
        fl_seen = (uint8_t)flash;
    }
    if (au.drawn && sig == au.sig) {
        if (alt_lw_vis() && alt_now_ms - alt_mat_ms >= alt_sm_cap) {
            alt_mat_ms = alt_now_ms;
            alt_draw_gl_n = alt_gl_n;
            alt_draw_lfo_p = alt_lfo_p;
            if (alt_lw_frame(q, flash))
                alt_fps_tick();
        }
        return;
    }
    au.sig = sig;
    au.drawn = 1;
    alt_mat_ms = alt_now_ms;
    alt_draw_gl_n = alt_gl_n;                    /* (une seule heure pour les deux moitiés) */
    alt_draw_lfo_p = alt_lfo_p;
    for (i = 0; i < 2u; i++) {                   /* two halves: the canvas holds 240 x 124 */
        cv_begin(240, 120, ALT_BG);
        alt_paper(i);                            /* (both cards have a grain: paper, ink) */
        cv_oy = -(int32_t)i * 120;
        alt_scene(q, flash);
        cv_oy = 0;
        cv_blit(0, i * 120u);
    }
    alt_sm_seen();
    alt_lw_ok = 0;
    if (alt_lw_vis())                            /* (the LFO's wave now on the screen: the next frames send only its
                                                  * changes) */
        alt_lw_seen(au.mtab);
    alt_fps_tick();
    alt_full_n++;
}

/* the browser build's hooks (web/emu/felucca_web.c web_alter): op 0 circle a's sound b, 1 scene a, 2 mute circle a
 * (b = 0 / 1), 3 circle a's envelope mode b, 4 tempo a, 5 circle a's mode: its sound's own, 6 Felucca's phrase b on
 * part a (as its own SAVE page would: the test that ALTER keeps Felucca's sequencer quiet), 7 the FX page's preset b
 * of slot a (alter_fx.c AFX_S_*: 0..2 a circle's DIST), 8 MASTER setting a (amx) to b, 9 circle a's
 * DELAY lane at step b off, 10 circle a's envelope mode settings to their defaults, 11 circle a aligns its level,
 * 12 the MASTER FILTER's setting a (0..10 A, 11..21 B, 22 MORPH) to b, 13 its preset b into snapshot a, 14..16 below,
 * 17 circle a & 3's part setting a >> 8 to b (255: its RUMBLE), 18 every RUMBLE muted (a = 1) or not, 19 a RANGE */
/* the browser build's tool: USER scene u's records as bytes (its scene, its three rhythms, its effects, its envelope
 * settings), so tools/alter/alter_user_scene.mjs can turn a USER scene into a factory one (alter_sounds.c) */
#define ALT_DUMP_N (sizeof(alt_scene_rec_t) + sizeof(alt_rhy_t) * AS_NCIRC + 1u + ALT_FXREC + 1u + AS_NCIRC * AE_N)
static uint8_t alt_dump[ALT_DUMP_N];
static uint32_t alt_dump_user(uint32_t u)
{
    uint32_t n = 0, k;
    const alt_us_t *f = &alt_ub.s.r[(u % ALT_NUSER) % ALT_US_PER];
    u %= ALT_NUSER;
    (void)alt_ub_load_s(u);
    for (k = 0; k < sizeof(alt_scene_rec_t); k++)
        alt_dump[n++] = ((const uint8_t *)&f->s)[k];
    for (k = 0; k < sizeof(alt_rhy_t) * AS_NCIRC; k++)
        alt_dump[n++] = ((const uint8_t *)f->r)[k];
    alt_dump[n++] = f->fxok;
    for (k = 0; k < ALT_FXREC; k++)
        alt_dump[n++] = f->fx[k];
    alt_dump[n++] = f->envok;
    for (k = 0; k < AS_NCIRC * AE_N; k++)
        alt_dump[n++] = ((const uint8_t *)f->env)[k];
    return n;
}

/* (les tests, 03/11) : w 0 = moteur du cercle c, 1 = son accord, 2 = son budget d'effets (256 = un effet au plus fort),
 * 3..8 = ses six parts (DIST INSERT DELAY REVERB LFO MTX), 9 TYPE LEN DENS LOCK, 10 RANGE (05/11), 11 CONSOLE */
static int alt_web_rc;
static int32_t alt_peek(uint32_t c, uint32_t w)
{
    c %= AS_NCIRC;
    if (w >= 3u && w < 9u) {
        int32_t v[6];
        alt_fxb_vals(c, v);
        return v[w - 3u];
    }
    if (w == 9u)                                 /* (the rhythm: TYPE, LEN, DENS, LOCK; 10: RANGE) */
        return (int32_t)(au.cfg[c].type | au.cfg[c].len << 8 | au.cfg[c].tdens << 16 | (uint32_t)au.cfg[c].tlock << 24);
    if (w == 10u)
        return au.cfg[c].trange;
    if (w == 11u)                                /* (the CONSOLE: LOW MID HIGH COMP, a byte each) */
        return (int32_t)(acs_v[c][0] | acs_v[c][1] << 8 | acs_v[c][2] << 16 | (uint32_t)acs_v[c][3] << 24);
    if (w == 15u)                                /* (the PROGRESSION's tests: the last hit's note) */
        return alt_note[c][0];
    if (w == 13u || w == 14u) {                  /* (the draws' tests, 10/11: 13 a hash of circle c's draw and pattern;
                                                  * 14 its TURING: treg | frozen << 16 | die << 17 | PROGRESSION << 24) */
        const as_circ_t *x = &alt_seq.c[c];
        uint32_t h = 2166136261u, k, j;
        if (w == 14u)
            return (int32_t)(x->treg | (uint32_t)x->tfreeze << 16 | (uint32_t)x->die << 17 | (uint32_t)alt_seq.prog[c] << 24);
        for (k = 0; k < 16u; k++) {
            uint32_t v[9] = {(uint32_t)x->dforce[k], (uint32_t)x->pitch[k], (uint32_t)x->vel[k], x->ddly[k], x->drev[k],
                             x->glide[k] | x->flam[k] << 1 | x->rat[k] << 2 | x->gate[k] << 4, x->dchance[k],
                             (uint32_t)x->env[k], 0};
            for (j = 0; j < AS_NTIMB; j++)
                v[8] = v[8] * 257u + (uint8_t)x->timb[k][j];
            for (j = 0; j < 9u; j++)
                h = (h ^ (v[j] & 0xFFFFFFFFu)) * 16777619u;
        }
        h = (h ^ x->pat) * 16777619u;
        return (int32_t)(h ^ x->die ^ (uint32_t)(uint8_t)x->dhits << 8 ^ (uint32_t)x->drot << 16);
    }
    if (w == 12u)                                /* (MIDI LEARN's tests: MUTE, HITS, the chosen circle, playing, REC) */
        return (int32_t)(au.cfg[c].mute | au.cfg[c].hits << 8 | au.sel << 16 | (uint32_t)song.playing << 24 |
                         (uint32_t)ar_on << 25);
    if (w >= 100u && w < 100u + P_COUNT)         /* (a part's parameter as it is) */
        return trk[c].p[w - 100u];
    if (w >= 400u && w < 400u + ALM_N)           /* (MIDI LEARN's entry w - 400: its three bytes) */
        return (int32_t)(alm_e(w - 400u)[0] | alm_e(w - 400u)[1] << 8 | (uint32_t)alm_e(w - 400u)[2] << 16);
    return w == 0 ? (int32_t)trk[c].eng_req : w == 1 ? (int32_t)au.cfg[c].chord : alt_fxb_total(c);
}
static void alt_web_cmd(uint32_t op, uint32_t a, uint32_t b)
{
    if (op == 18u)                               /* (before the view is ready too: the tests' first call) */
        rmb_mute = (uint8_t)(a != 0);
    if (!au.ready)
        return;
    switch (op) {
    case 0: alt_load_sound(a % AS_NCIRC, b); break;
    case 1: alt_load_scene(a); break;
    case 2: au.cfg[a % AS_NCIRC].mute = (uint8_t)(b != 0); alt_post_circle(a % AS_NCIRC); break;
    case 26: alt_seq.prog[a % AS_NCIRC] = (uint8_t)(b % AS_NPROG); break;   /* (a test: circle a's PROGRESSION) */
    case 27:                                     /* (a test: circle a & 3's LFO a >> 8: WAVE b & 7, RATE b >> 3 & 31, DEST
                                                  * b >> 8 & 31, DEPTH (int8) (b >> 16)) */
        alt_cl[a & 3u][(a >> 8) % CL_N].wave = (uint8_t)((b & 7u) % GW_N);
        alt_cl[a & 3u][(a >> 8) % CL_N].rate = (uint8_t)((b >> 3 & 31u) % GL_NRATE);
        alt_cl[a & 3u][(a >> 8) % CL_N].dst = (uint8_t)((b >> 8 & 31u) % GD_N);
        alt_cl[a & 3u][(a >> 8) % CL_N].depth = (int8_t)clamp((int8_t)(b >> 16), -64, 63);
        break;
    case 22: alt_total_chaos(); break;          /* (a test: CHAOS TOTAL, 10/11) */
    case 23: alt_web_rc = alt_row_save(0, ALT_NFACT + 1u + a % ALT_NUSER); break;   /* (a test: SAVE into USER a) */
    case 25:                                     /* (a test: USER a stored as before the draws, 10/11: the object cut) */
        alt_web_rc = alt_row_save(0, ALT_NFACT + 1u + a % ALT_NUSER);
        alt_ub.s.dr[a % ALT_US_PER][0] = 0;
        (void)st_save(ALT_OBJ_US0 + (a % ALT_NUSER) / ALT_US_PER, &alt_ub.s, sizeof alt_ub.s - sizeof alt_ub.s.dr);
        break;
    case 24: alt_web_rc = alt_row_load(b & 1u, b & 2u ? 0u : ALT_NFACT + 1u + a % ALT_NUSER); break;   /* (a test: LOAD
                                                  * USER a, b & 1 a pattern; b & 2: NEW) */
    case 21: au.cfg[a % AS_NCIRC].len = (uint8_t)clamp((int32_t)b, 1, 32); alt_post_circle(a % AS_NCIRC); break;   /* (a test: circle a's LEN) */
    case 3: alt_emode[a % AS_NCIRC] = am_ok(b); break;
    case 4: song.g[G_BPM] = (int16_t)clamp((int32_t)a, 40, 240); break;
    case 5: alt_emode[a % AS_NCIRC] = am_ok(ALT_PRE[au.pre[a % AS_NCIRC]].mode); break;
    case 6: pat_load(&trk[a % NPART], b); break;   /* (a test: Felucca's own phrase b on part a) */
    case 7:                                      /* the FX page's preset b of slot a (0..2 a circle's DIST, 3 DELAY, 4 REVERB, 5 MASTER) */
        if (a < AS_NCIRC) afp_dist_apply(a, b); else if (a == AFX_S_DLY) afp_dly_apply(b);
        else if (a == AFX_S_REV) afp_rev_apply(b); else afp_mst_apply(b);
        break;
    case 8: amx[a % AMX_N] = (uint8_t)b; break;  /* (a test: MASTER setting a) */
    case 20: acs_v[(a & 3u) % AS_NCIRC][(a >> 8) % ACS_N] = (uint8_t)(b & 127u); break;   /* (a test: circle a & 3's
                                                  * CONSOLE value a >> 8: 0 LOW 1 MID 2 HIGH 3 COMP) */
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
    case 19:                                     /* (a test: circle a & 3's lane a >> 8, steps mask b & 0xFFFF: a RANGE
                                                  * from (b >> 16) & 255 to b >> 24) */
        {
            as_circ_t *sc = &alt_seq.c[(a & 3u) % AS_NCIRC];
            uint32_t ln = (a >> 8) % AS_NLR, k;
            for (k = 0; k < AS_NLANE; k++)
                if ((b >> k) & 1u) {
                    sc->rlo[ln][k] = (int8_t)((b >> 16) & 255u);
                    sc->rhi[ln][k] = (int8_t)(b >> 24);
                }
            sc->rmask[ln] |= (uint16_t)b;
        }
        break;
    case 17:                                     /* (a test: circle a & 3's part setting a >> 8 to (int8) b; a >> 8 = 255:
                                                  * its RUMBLE to b) */
        fm1_irq_off();
        if ((a >> 8) == 255u)
            alt_rmb[a & 3u] = (uint8_t)b;
        else if ((a >> 8) < P_COUNT)
            trk[(a & 3u) % AS_NCIRC].p[a >> 8] = (int8_t)(b & 255u);
        fm1_irq_on();
        break;
    case 16:                                     /* (a test: the reverb: TYPE a >> 8, SIZE a & 255, DAMP b) */
        song.g[G_RTYPE] = (int16_t)(a >> 8);
        song.g[G_RSIZE] = (int16_t)clamp((int32_t)(a & 255u), 0, 127);
        song.g[G_RDAMP] = (int16_t)clamp((int32_t)b, 0, 127);
        break;
    case 15:                                     /* (a test: circle a's rhythm: LEN b & 255, HITS b >> 8, ROT 0) */
        au.cfg[a % AS_NCIRC].len = (uint8_t)(b & 255u);
        au.cfg[a % AS_NCIRC].hits = (uint8_t)(b >> 8);
        au.cfg[a % AS_NCIRC].rot = 0;
        alt_post_circle(a % AS_NCIRC);
        break;
    default: break;
    }
}
