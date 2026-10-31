/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's PLAY view: the HOME page (Felucca's pages stay behind the other buttons). Three circles, three voices:
 * circle i plays part i (alter_play.c). Included in ui_input.c (after led_put); ui_input calls alter_ui_input first,
 * ui_draw_page alter_draw, ui_leds alter_leds. Every button does what its label says (Romain's rule):
 *   KNOB 1-4     the four main parameters of the chosen circle's engine (engine_t.knob)
 *   SEQ          KNOB 1-4 = LEN HITS ROT PROB of the chosen circle (from its next cycle); the others greyed. A tap
 *                keeps it open (another tap closes it); held 2 s or more it closes on release
 *   SELECT       the chosen circle (and Felucca's selected part: ENV, FX .. edit its sound)
 *   PRESETS      the chosen circle's sound, through the presets of ALTER, PHYS and NOISE in a row
 *   ALGORITHM    KNOB 4         PLAY  start / stop      HOME, ENV, LFO, FX, GLO, SCL, EDIT, SAVE: Felucca's
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
#define ALT_SEQ_HOLD_MS 2000u             /* SEQ held this long: it closes on release (shorter: it stays open) */

static struct {
    as_cfg_t cfg[AS_NCIRC];      /* the circles' settings as asked for (alt_post_cfg's source) */
    uint8_t ready, sel, drawn, die_clean_done, handed;   /* handed: a Felucca button is down (its release is Felucca's) */
    uint8_t seq_lock, seq_was;   /* SEQ kept open; SEQ down at the last frame */
    uint32_t seq_t0;             /* SEQ's press (fm1_ms) */
    uint32_t keys;               /* the keys held at the last frame */
    uint32_t die_t0[AS_NCIRC];   /* a die key's press (fm1_ms | 1: never 0), 0 = up; compared signed (the | 1 may
                                  * put it 1 ms ahead) */
    uint32_t tap_ms[27];         /* a key's last tap (its bright LED) */
    const char *msg;             /* the middle's word .. */
    uint32_t msg_ms;             /* .. shown since */
    uint32_t sig, hits_seen[AS_NCIRC], flash_ms[AS_NCIRC], handed_ms;   /* handed_ms: when every Felucca button was up */
} au;

static int alter_view(void)
{
    uint32_t mine = (1u << panel.btn[B_SEQ]) | (1u << panel.btn[B_PLAY]) | (1u << panel.btn[B_OCTDN]) |
                    (1u << panel.btn[B_OCTUP]);
    return ui.home && !ui.menu && !ui.layer && !ui.confirm && !ui.uboot && !name_on() &&
           !(fm1_in.buttons & ~mine);    /* (FX, GLO .. held: Felucca's layers) */
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
static uint32_t alt_npre(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < AS_NCIRC; i++)
        n += ENGINES[ALT_ENGS[i]]->npresets;
    return n;
}
static uint32_t alt_pre_place(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < AS_NCIRC; i++) {
        uint32_t np = ENGINES[ALT_ENGS[i]]->npresets;
        if (t->eng_req == ALT_ENGS[i])
            return n + (t->preset < np ? t->preset : 0u);
        n += np;
    }
    return 0;
}

/* PRESETS: circle c's sound s presets on (across engines) */
static void alt_preset_step(uint32_t c, int32_t s)
{
    track_t *t = &trk[c];
    uint32_t n = alt_npre(), at = (uint32_t)(((int32_t)alt_pre_place(t) + s % (int32_t)n + (int32_t)n) % (int32_t)n);
    uint32_t i, j, e = ALT_ENGS[0], pi = at;
    for (i = 0; i < AS_NCIRC; i++) {
        uint32_t np = ENGINES[ALT_ENGS[i]]->npresets;
        if (pi < np) {
            e = ALT_ENGS[i];
            break;
        }
        pi -= np;
    }
    if (t->eng_req != e)
        set_engine_of(t, e);                     /* (the IRQ off inside: the engine and its values together) */
    fm1_irq_off();                               /* (the ISR's offsets and the preset's values together) */
    apply_preset_to(t, pi);
    for (j = 0; j < ALT_NSLOT; j++)
        alt_applied[c][j] = 0;
    alt_note[c] = 0;                             /* (apply_preset_to released what sounded) */
    alt_gate_left[c] = 0;
    fm1_irq_on();
    alt_preset_cnt[c]++;
    alt_say(ENGINES[e]->presets[t->preset].name);
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

static void alt_select(uint32_t i)
{
    au.sel = (uint8_t)(i % AS_NCIRC);
    alt_sel = au.sel;
    track_select(au.sel);                        /* Felucca's pages (ENV, FX, EDIT ..) edit this circle's sound */
}

/* 1: the PLAY view had this frame's input (ui_input stops); 0: Felucca's (another page, or a button that opens one) */
static int alter_ui_input(uint32_t pressed)
{
    static const uint8_t OURS[] = {B_SEQ, B_PLAY, B_OCTDN, B_OCTUP};
    uint32_t keys = fm1_in.notes, down, k, i, ours = 0, seq;
    int32_t s;
    for (i = 0; i < sizeof OURS; i++)
        ours |= 1u << panel.btn[OURS[i]];
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
    }
    if (song.sel != au.sel && song.sel < AS_NCIRC)   /* (chosen elsewhere: TRACKS, the editor) */
        au.sel = alt_sel = song.sel;
    if ((pressed >> panel.btn[B_PLAY]) & 1u)
        transport_req = song.playing || seq_counting() ? 2u : 1u;
    seq = (fm1_in.buttons >> panel.btn[B_SEQ]) & 1u;
    if (seq && !au.seq_was)
        au.seq_t0 = fm1_ms;
    else if (!seq && au.seq_was)                 /* let go: a tap toggles it open / closed; a long hold closes it */
        au.seq_lock = (int32_t)(fm1_ms - au.seq_t0) < (int32_t)ALT_SEQ_HOLD_MS ? (uint8_t)!au.seq_lock : 0u;
    au.seq_was = (uint8_t)seq;
    seq |= au.seq_lock;
    for (i = 0; i < 4u; i++)
        if ((s = panel_enc(EN_K1 + i)) != 0) {
            if (seq)
                alt_edit_circle(i, s);
            else
                alt_knob_tot[au.sel][i] += s;
        }
    if ((s = panel_enc(EN_SELECT)) != 0)
        alt_select((uint32_t)((int32_t)au.sel + s + 3 * 16));
    if ((s = panel_enc(EN_ALGO)) != 0)
        alt_knob_tot[au.sel][3] += s;
    if ((s = panel_enc(EN_PRESET)) != 0)
        alt_preset_step(au.sel, s);
    /* the keys */
    down = keys & ~au.keys;
    for (k = 0; k < 27u; k++) {
        const alt_key_t *a = &ALT_KEY[k];
        uint32_t i2 = a->arg;
        if ((down >> k) & 1u && a->kind) {
            alt_say(a->name);
            if (a->kind == AK_MUTE) {
                au.cfg[i2].mute ^= 1u;
                alt_post_circle(i2);
            } else if (a->kind == AK_DIE) {
                au.die_t0[i2] = fm1_ms | 1u;
                au.die_clean_done &= (uint8_t)~(1u << i2);
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

static void alt_scene(int seq, uint32_t flash)
{
    const track_t *t = &trk[au.sel];
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    char b[24];
    uint32_t i, j;
    /* the head: the chosen circle, its sound, the transport */
    b[0] = (char)('1' + au.sel), b[1] = ' ', b[2] = 0;
    cv_text(8, 2, &AF_M, b, ALT_COL[au.sel]);
    cv_text(26, 2, &AF_M, t->preset < e->npresets ? e->presets[t->preset].name : e->name, ALT_INK);
    cv_text(8, 22, &AF_S, e->name, ALT_DIM);
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
        int muted = au.cfg[i].mute, dim = seq && i != au.sel, chosen = i == au.sel;
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
    /* the foot */
    if (!seq) {
        for (i = 0; i < 4u; i++) {
            int32_t x = 8 + (int32_t)i * 58, p = e->knob[i], v = t->p[p];
            const param_desc_t *d = track_desc(t, (uint32_t)p);
            if (alt_slot((uint32_t)p) >= 0)
                v -= alt_applied[au.sel][alt_slot((uint32_t)p)];   /* (the base, without the die's offset) */
            cv_text(x, 206, &AF_S, d->label, ALT_DIM);
            if (d->fmt == F_ENUM && d->names)
                cv_text(x, 220, &AF_S, d->names[clamp(v, d->min, d->max) - d->min], ALT_INK);
            else {
                int32_t span = d->max - d->min;
                cv_rect(x, 225, 50, 5, ALT_DARK);
                cv_rect(x, 225, span > 0 ? (clamp(v, d->min, d->max) - d->min) * 50 / span : 0, 5, ALT_INK);
            }
        }
    } else {
        static const char *const N[4] = {"LEN", "HITS", "ROT", "PROB"};
        const as_cfg_t *c = &au.cfg[au.sel];
        int32_t v[4];
        v[0] = c->len, v[1] = c->hits, v[2] = c->rot, v[3] = c->prob * 100 / 127;
        b[0] = 'C', b[1] = 'I', b[2] = 'R', b[3] = 'C', b[4] = 'L', b[5] = 'E', b[6] = ' ', b[7] = (char)('1' + au.sel),
        b[8] = 0;
        cv_text(8, 196, &AF_S, b, ALT_COL[au.sel]);
        cv_text_r(234, 196, &AF_S, alt_seq.c[au.sel].die ? "DICE ON" : "", ALT_DIM, cv_bg);
        for (i = 0; i < 4u; i++) {
            char d[6];
            alt_num(d, v[i], i == 3u);
            cv_text(8 + (int32_t)i * 58, 211, &AF_S, N[i], ALT_DIM);
            cv_text(8 + (int32_t)i * 58, 225, &AF_S, d, ALT_INK);
        }
    }
}

static void alter_draw(void)
{
    uint32_t sig = 0, i, h, flash = 0, seq = ((fm1_in.buttons >> panel.btn[B_SEQ]) & 1u) | au.seq_lock;
    const track_t *t = &trk[au.sel];
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
    for (i = 0; i < 8u; i++)
        sig = sig * 31u + (uint32_t)t->p[P_E0 + i];
    sig = sig * 31u + (uint32_t)t->p[P_REL];
    sig = sig * 31u + seq + au.sel * 3u + flash * 5u + (uint32_t)song.playing * 7u + t->preset * 11u +
          (uint32_t)song.g[G_BPM] * 13u + (au.msg && fm1_ms - au.msg_ms < 900u ? (uint32_t)(uintptr_t)au.msg : 1u);
    ui.force = 1;                                /* (whatever comes after this view draws itself whole) */
    if (au.drawn && sig == au.sig)
        return;
    au.sig = sig;
    au.drawn = 1;
    for (i = 0; i < 2u; i++) {                   /* two halves: the canvas holds 240 x 124 */
        cv_begin(240, 120, RGB(0, 0, 0));
        cv_oy = -(int32_t)i * 120;
        alt_scene((int)seq, flash);
        cv_oy = 0;
        cv_blit(0, i * 120u);
    }
}
