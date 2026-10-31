/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's PLAY view: the HOME page (Felucca's pages stay behind the other buttons). Three circles, three voices:
 * circle i plays part i (alter_play.c). Included in ui_input.c (after led_put); ui_input calls alter_ui_input first,
 * ui_draw_page alter_draw, ui_leds alter_leds. Every button does what its label says (Romain's rule):
 *   KNOB 1-4     the four main parameters of the chosen circle's engine (engine_t.knob)
 *   SEQ ENV FX LFO EDIT SCL GLO   a quick page: the view stays, KNOB 1-4 become the page's four (ALT_QP: SEQ = LEN
 *                HITS ROT PROB from the next cycle, the other circles greyed; ENV = ATK DEC SUS REL; FX = DIST INSERT
 *                DELAY REVERB; LFO = RATE >PITCH >TIMBRE >LEVEL; EDIT = the engine's four others; SCL = SCALE ROOT
 *                CHORD DOSE; GLO = TEMPO LEVEL PAN SWING). A tap keeps it open (the same button again, or HOME, closes
 *                it; another page button switches); held 2 s or more it closes on release
 *   OCT- + OCT+ held, then a page button: Felucca's own page (HOME comes back); then DICE n: circle n's LOCKS page
 *                (the white keys F3..E4 = RHYTHM PITCH GLIDE ENV TIMBRE FORCE VELOCITY, a tap locks / frees; HOME)
 *   SELECT       the chosen circle (and Felucca's selected part: its pages edit its sound)
 *   PRESETS      the chosen circle's sound, through ALTER's own sounds of the three engines (alter_sounds.c ALT_PRE)
 *   SAVE         a tap: the SCENES page (PRESETS picks, SAVE loads, HOME leaves): the whole instrument from a starting
 *                point (alter_sounds.c ALT_SCENE). The first scene is loaded at power-on
 *   ALGORITHM    the chosen circle's envelope mode: FREE PERC STAB DRONE (alter_play.c AM_*)
 *   DICE 1 + 2 + 3 together: CHAOS, everything drawn to the extreme on the three circles: engine, preset, the
 *                engine's eight, the effects and LFO, the rhythm (at once), the die (DOSE 127), the scale and roots;
 *                not the tempo, not the modes, not the mutes. For good: no undo
 *   PLAY  start / stop      SAVE ARP REC: Felucca's
 * The keys (no notes here: alt_kb_own): the whites in lit blocks with a dark key between: MUTE 1 2 3 (a tap),
 * DICE 1 2 3 (a tap: a new draw at once; held 0.6 s: the clean pattern again), SHIFT -1 +1 (a tap: the chosen
 * circle turned a step, kept); the blacks in their groups 3-2-3-2-1, on the chosen circle: held ROLL 1/16 1/32
 * triplet | RATCHET x2 x3 | DUB DELAY, DUB REVERB, DESTROY | tapped DENSITY - + (a hit less / more, kept) | held
 * REVERSE. All on the grid (alter_play.c). LEDs: the keys that act faint, bright when engaged (a circle muted, a
 * fill held, a tap just made); the others dark.
 * The screen: three concentric circles (circle 1 outside), each a voice's hits, lit as they play; the chosen one a
 * solid ring, the others dotted; a draw on: a small triangle at the circle's left, its colour; the chosen
 * circle's sound at the top, its knobs (or its rhythm, SEQ held) at the bottom. Drawn only when something changed. */

enum { AK_NONE, AK_MUTE, AK_DIE, AK_SHIFT, AK_DENS, AK_FILL };
typedef struct {
    uint8_t kind, arg;
    const char *name;
} alt_key_t;
static const alt_key_t ALT_KEY[27] = {    /* k: semitones above F3 (whites 0 2 4 6 7 9 11 12 14 16 18 19 ..) */
    [0] = {AK_MUTE, 0, "MUTE 1"}, [2] = {AK_MUTE, 1, "MUTE 2"}, [4] = {AK_MUTE, 2, "MUTE 3"},
    [7] = {AK_DIE, 0, "DICE 1"}, [9] = {AK_DIE, 1, "DICE 2"}, [11] = {AK_DIE, 2, "DICE 3"},
    [14] = {AK_SHIFT, 0, "SHIFT -1"}, [16] = {AK_SHIFT, 1, "SHIFT +1"},
    [1] = {AK_FILL, AF_ROLL16, "ROLL 1/16"}, [3] = {AK_FILL, AF_ROLL32, "ROLL 1/32"}, [5] = {AK_FILL, AF_ROLLT, "ROLL 1/16T"},
    [8] = {AK_FILL, AF_RAT2, "RATCHET X2"}, [10] = {AK_FILL, AF_RAT3, "RATCHET X3"},
    [13] = {AK_FILL, AF_DLY, "DUB DELAY"}, [15] = {AK_FILL, AF_REV, "DUB REVERB"}, [17] = {AK_FILL, AF_DESTROY, "DESTROY"},
    [20] = {AK_DENS, 0, "DENSITY -"}, [22] = {AK_DENS, 1, "DENSITY +"},
    [25] = {AK_FILL, AF_REVERSE, "REVERSE"},
};
#define ALT_DIE_HOLD_MS 600u
#define ALT_TAP_MS 150u                   /* a tapped key's bright LED */
#define ALT_PAGE_HOLD_MS 2000u            /* a page button held this long: it closes on release (shorter: stays open) */

/* the quick pages: their buttons, names and four knobs (P_* of the chosen circle's part, or ALT_QK_* below;
 * QK_ENG: the engine's own) */
enum { QP_NONE, QP_SEQ, QP_ENV, QP_FX, QP_LFO, QP_EDIT, QP_SCL, QP_GLO, QP_N };
enum { QK_SCALE = 200, QK_ROOT, QK_DOSE, QK_BPM, QK_SWING, QK_RHY, QK_ENG, QK_ENGX, QK_CHORD };
static const uint8_t ALT_QP_BTN[QP_N] = {0, B_SEQ, B_ENV, B_FX, B_LFO, B_EDIT, B_SCL, B_GLO};
static const char *const ALT_QP_NAME[QP_N] = {"", "RHYTHM", "ENV", "FX", "LFO", "EDIT", "SCALE", "GLOBAL"};
static const uint8_t ALT_QP_K[QP_N][4] = {
    {QK_ENG, QK_ENG, QK_ENG, QK_ENG},
    {QK_RHY, QK_RHY, QK_RHY, QK_RHY},
    {P_ATK, P_DEC, P_SUS, P_REL},
    {P_DIST, P_IA, P_DLY, P_REV},
    {P_LRATE, P_LD_PIT, P_LD_FLT, P_LD_AMP},
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
    uint8_t scene_open, scene_i; /* the SCENES page open; the scene it points at */
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

/* the buttons the view keeps: the page buttons, HOME, PLAY, OCT- OCT+ */
static uint32_t alt_ours(void)
{
    static const uint8_t OURS[] = {B_SEQ, B_ENV, B_FX, B_LFO, B_EDIT, B_SCL, B_GLO, B_HOME, B_SAVE, B_PLAY, B_OCTDN,
                                   B_OCTUP};
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
    alt_preset_cnt[c]++;
}

/* PRESETS: circle c's sound s on */
static void alt_preset_step(uint32_t c, int32_t s)
{
    uint32_t i = (uint32_t)(((int32_t)au.pre[c] + s % (int32_t)ALT_NPRE + (int32_t)ALT_NPRE) % (int32_t)ALT_NPRE);
    alt_load_sound(c, i);
    alt_say(ALT_PRE[i].name);
}

/* scene n: the three circles, the scale, the tempo, the swing (the dice, mutes and locks cleared) */
static void alt_load_scene(uint32_t n)
{
    const alt_scene_t *sc = &ALT_SCENE[n % ALT_NSCENE];
    uint32_t c;
    song.g[G_BPM] = sc->bpm;
    song.g[G_SWING] = sc->swing;
    alt_seq.scale = sc->scale;
    for (c = 0; c < AS_NCIRC; c++) {
        as_cfg_t *cf = &au.cfg[c];
        alt_load_sound(c, alt_pre_find(sc->c[c].pre));
        trk[c].p[P_LEVEL] = (int16_t)clamp(trk[c].p[P_LEVEL] + sc->c[c].gain, 1, 127);
        alt_emode[c] = sc->c[c].mode;
        cf->len = sc->c[c].len;
        cf->hits = sc->c[c].hits;
        cf->rot = sc->c[c].rot;
        cf->prob = sc->c[c].prob;
        cf->root = sc->c[c].root;
        cf->chord = sc->c[c].chord;
        cf->frot = 0;
        cf->mute = 0;
        cf->dose = AS_DOSE_DEF;
        alt_seq.c[c].locks = 0;
        alt_post_now |= (uint8_t)(1u << c);
        alt_post_circle(c);
        alt_clear_cnt[c]++;
    }
}

/* SEQ held: knob i of the chosen circle, s detents */
static void alt_edit_circle(uint32_t i, int32_t s)
{
    as_cfg_t *c = &au.cfg[au.sel];
    switch (i) {
    case 0:
        c->len = (uint8_t)clamp((int32_t)c->len + s, 1, AS_MAXLEN);
        if (c->hits > c->len)
            c->hits = c->len;
        c->rot %= c->len;
        c->frot %= c->len;
        break;
    case 1:
        c->hits = (uint8_t)clamp((int32_t)c->hits + s, 0, c->len);
        break;
    case 2:
        c->rot = (uint8_t)(((int32_t)c->rot + s + 4 * AS_MAXLEN) % c->len);
        break;
    default:
        c->prob = (uint8_t)clamp((int32_t)c->prob + s * 8, 0, 127);
        break;
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

/* a knob of quick page q turned s detents */
static void alt_turn(uint32_t q, uint32_t i, int32_t s)
{
    uint32_t id = alt_qk(q, i);
    as_cfg_t *c = &au.cfg[au.sel];
    switch (id) {
    case QK_RHY:
        alt_edit_circle(i, s);
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
    static const uint8_t FXP[9] = {P_DIST, P_ITYPE, P_IA, P_IMIX, P_DLY, P_REV, P_LRATE, P_LD_FLT, P_LD_AMP};
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
        for (k = 0; k < sizeof FXP; k++)         /* the effects and LFO */
            t->p[FXP[k]] = alt_crange(param_desc_of(e, FXP[k]), 1);
        t->p[P_LD_PIT] = alt_crange(param_desc_of(e, P_LD_PIT), 4);   /* (the pitch LFO milder: the notes stay notes) */
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
        alt_load_scene(0);                       /* power-on: the first scene */
    }
    if (song.sel != au.sel && song.sel < AS_NCIRC)   /* (chosen elsewhere: TRACKS, the editor) */
        au.sel = alt_sel = song.sel;
    if ((pressed >> panel.btn[B_PLAY]) & 1u)
        transport_req = song.playing || seq_counting() ? 2u : 1u;
    if ((pressed >> panel.btn[B_HOME]) & 1u) {   /* HOME: the circles, no page */
        au.qp = QP_NONE;
        au.lockc = 0;
        au.scene_open = 0;
    }
    if ((pressed >> panel.btn[B_SAVE]) & 1u) {   /* SAVE: the SCENES page; on it, SAVE loads the scene */
        if (au.scene_open) {
            alt_load_scene(au.scene_i);
            alt_say(ALT_SCENE[au.scene_i].name);
            au.scene_open = 0;
        } else {
            au.scene_open = 1;
            au.qp = QP_NONE;
            au.lockc = 0;
        }
    }
    if (au.scene_open) {                         /* the SCENES page: PRESETS picks, the rest waits */
        if ((s = panel_enc(EN_PRESET)) != 0)
            au.scene_i = (uint8_t)(((int32_t)au.scene_i + s + 16 * (int32_t)ALT_NSCENE) % (int32_t)ALT_NSCENE);
        for (i = 0; i < 4u; i++)
            (void)panel_enc(EN_K1 + i);
        (void)panel_enc(EN_SELECT);
        (void)panel_enc(EN_ALGO);
        alt_hold = 0;
        au.keys = keys;
        return 1;
    }
    for (q = 1; q < QP_N; q++) {                 /* the page buttons: a tap keeps the page, a long hold does not */
        uint32_t dn = (fm1_in.buttons >> panel.btn[ALT_QP_BTN[q]]) & 1u, was = (au.qp_was >> q) & 1u;
        if (dn && !was) {
            au.qp_t0[q] = fm1_ms;
            au.lockc = 0;
            alt_say(ALT_QP_NAME[q]);
        } else if (!dn && was) {
            if ((int32_t)(fm1_ms - au.qp_t0[q]) < (int32_t)ALT_PAGE_HOLD_MS)
                au.qp = (uint8_t)(au.qp == q ? QP_NONE : q);
            else if (au.qp == q)
                au.qp = QP_NONE;
        }
        au.qp_was = (uint8_t)((au.qp_was & ~(1u << q)) | (dn << q));
    }
    q = alt_page();
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
        if ((down >> k) & 1u && a->kind) {
            alt_say(a->name);
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
                alt_shift_tot[au.sel] += d;
                au.cfg[au.sel].rot = (uint8_t)(((int32_t)au.cfg[au.sel].rot + d + 4 * AS_MAXLEN) % au.cfg[au.sel].len);
                au.tap_ms[k] = fm1_ms | 1u;
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
        faint = a->kind && !lit;
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
static int32_t alt_kv(uint32_t q, uint32_t i, const char **name, char *txt, int32_t *fill)
{
    static const char *const RHY[4] = {"LEN", "HITS", "ROT", "PROB"};
    static const char *const NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const track_t *t = &trk[au.sel];
    const as_cfg_t *c = &au.cfg[au.sel];
    uint32_t id = alt_qk(q, i);
    int32_t v;
    txt[0] = 0;
    *fill = -1;
    switch (id) {
    case QK_RHY:
        *name = RHY[i];
        v = i == 0u ? c->len : i == 1u ? c->hits : i == 2u ? c->rot : c->prob * 100 / 127;
        alt_num(txt, v, i == 3u);
        return v;
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
    uint32_t i;
    cv_text(8, 2, &AF_M, "SCENES", ALT_INK);
    cv_text(8, 26, &AF_S, "PRESETS: PICK   SAVE: LOAD   HOME: BACK", ALT_DIM);
    for (i = 0; i < ALT_NSCENE; i++) {
        int32_t y = 54 + (int32_t)i * 42, on = i == au.scene_i;
        if (on)
            cv_rect(4, y - 4, 232, 38, ALT_DARK);
        cv_text(14, y, &AF_M, ALT_SCENE[i].name, on ? ALT_INK : ALT_DIM);
        cv_text(14, y + 20, &AF_S, ALT_SCENE[i].about, on ? ALT_COL[i % AS_NCIRC] : ALT_GREY);
    }
}

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
        cv_text(8, 194, &AF_S, ALT_QP_NAME[q], ALT_COL[au.sel]);
    for (i = 0; i < 4u; i++) {
        int32_t x = 8 + (int32_t)i * 58, fill;
        const char *name;
        char txt[12];
        alt_kv(q, i, &name, txt, &fill);
        cv_text(x, 209, &AF_S, name, ALT_DIM);
        if (fill >= 0) {
            cv_rect(x, 227, 50, 5, ALT_DARK);
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
    }
    sig = sig * 31u + alt_emode[au.sel] * 37u + alt_seq.scale * 41u + au.scene_open * 43u + au.scene_i * 47u +
          au.pre[au.sel] * 53u;
    sig = sig * 31u + q * 19u + au.lockc * 23u + alt_seq.c[au.lockc ? au.lockc - 1u : 0u].locks * 29u;
    sig = sig * 31u + au.sel * 3u + flash * 5u + (uint32_t)song.playing * 7u + trk[au.sel].preset * 11u +
          (uint32_t)song.g[G_BPM] * 13u + (au.msg && fm1_ms - au.msg_ms < 900u ? (uint32_t)(uintptr_t)au.msg : 1u);
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
 * (b = 0 / 1), 3 circle a's envelope mode b, 4 tempo a, 5 circle a's mode: its sound's own */
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
    default: break;
    }
}
