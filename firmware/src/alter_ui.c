/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's PLAY view: the HOME page of part 1 while it plays ALTER (Felucca's pages stay behind the other buttons).
 * Included in ui_input.c (after led_put); ui_input calls alter_ui_input first, ui_draw_page alter_draw, ui_leds
 * alter_leds. Every button does what its label says (Romain's rule):
 *   KNOB 1-4     MRPH SPRD HARM MODE (the sound's macros)
 *   SEQ held     KNOB 1-4 = LEN HITS ROT PROB of the chosen circle (from its next cycle); the others greyed
 *   SELECT       the chosen circle (HITS GHOST ACCENT)       PRESETS  the preset      ALGORITHM  MODE
 *   PLAY         start / stop                                HOME, ENV, LFO, FX, GLO, SCL, EDIT, SAVE: Felucca's
 * The keys (no notes here: alt_kb_own): the whites in lit blocks with a dark key between, MUTE x3 (a tap), DIE x3
 * (a tap: a new draw from the next cycle; held 0.6 s: the die off), SHIFT -1 +1 (held); the blacks in their groups
 * 3-2-3-2-1, held: ROLL 1/16 1/32 triplet | RATCHET x2 x3 | DUB DELAY, DUB REVERB, DESTROY | DENSITY - + | REVERSE.
 * The screen: three concentric circles (HITS outside, ACCENT inside), the steps lit as they play, the macros or the
 * circle's settings at the bottom, the key's name in the middle while it is held. Drawn only when something changed. */

enum { AK_NONE, AK_MUTE, AK_DIE, AK_FILL };
typedef struct {
    uint8_t kind, arg;
    const char *name;
} alt_key_t;
static const alt_key_t ALT_KEY[27] = {    /* k: semitones above F3 (whites 0 2 4 6 7 9 11 12 14 16 18 19 ..) */
    [0] = {AK_MUTE, 0, "MUTE HITS"}, [2] = {AK_MUTE, 1, "MUTE GHOST"}, [4] = {AK_MUTE, 2, "MUTE ACCENT"},
    [7] = {AK_DIE, 0, "DICE HITS"}, [9] = {AK_DIE, 1, "DICE GHOST"}, [11] = {AK_DIE, 2, "DICE ACCENT"},
    [14] = {AK_FILL, AF_SHIFTM, "SHIFT -1"}, [16] = {AK_FILL, AF_SHIFTP, "SHIFT +1"},
    [1] = {AK_FILL, AF_ROLL16, "ROLL 1/16"}, [3] = {AK_FILL, AF_ROLL32, "ROLL 1/32"}, [5] = {AK_FILL, AF_ROLLT, "ROLL 1/16T"},
    [8] = {AK_FILL, AF_RAT2, "RATCHET X2"}, [10] = {AK_FILL, AF_RAT3, "RATCHET X3"},
    [13] = {AK_FILL, AF_DLY, "DUB DELAY"}, [15] = {AK_FILL, AF_REV, "DUB REVERB"}, [17] = {AK_FILL, AF_DESTROY, "DESTROY"},
    [20] = {AK_FILL, AF_DENSM, "DENSITY -"}, [22] = {AK_FILL, AF_DENSP, "DENSITY +"},
    [25] = {AK_FILL, AF_REVERSE, "REVERSE"},
};
static const char *const ALT_CNAME[AS_NCIRC] = {"HITS", "GHOST", "ACCENT"};
#define ALT_DIE_HOLD_MS 600u

static struct {
    as_cfg_t cfg[AS_NCIRC];      /* the circles' settings as asked for (alt_post_cfg's source) */
    uint8_t ready, sel, drawn, die_off_done, handed;   /* handed: a Felucca button is down (its release is Felucca's) */
    uint32_t keys;               /* the keys held at the last frame */
    uint32_t die_t0[AS_NCIRC];   /* a die key's press (fm1_ms), 0 = up */
    const char *msg;             /* the middle's word .. */
    uint32_t msg_ms;             /* .. shown since */
    uint32_t sig, hits_seen, flash_ms, handed_ms;   /* handed_ms: when every Felucca button was up */
} au;

static int alter_view(void)
{
    uint32_t mine = (1u << panel.btn[B_SEQ]) | (1u << panel.btn[B_PLAY]) | (1u << panel.btn[B_OCTDN]) |
                    (1u << panel.btn[B_OCTUP]);
    return ui.home && !ui.menu && !ui.layer && !ui.confirm && !ui.uboot && !name_on() &&
           !(fm1_in.buttons & ~mine) && trk[ALT_PART].eng_req == ENGI_ALTER;   /* (FX, GLO .. held: Felucca's layers) */
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

/* 1: the PLAY view had this frame's input (ui_input stops); 0: Felucca's (another page, or a button that opens one) */
static int alter_ui_input(uint32_t pressed)
{
    static const uint8_t OURS[] = {B_SEQ, B_PLAY, B_OCTDN, B_OCTUP};
    track_t *t = &trk[ALT_PART];
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
    }
    if ((pressed >> panel.btn[B_PLAY]) & 1u)
        transport_req = song.playing || seq_counting() ? 2u : 1u;
    seq = (fm1_in.buttons >> panel.btn[B_SEQ]) & 1u;
    for (i = 0; i < 4u; i++)
        if ((s = panel_enc(EN_K1 + i)) != 0) {
            if (seq)
                alt_edit_circle(i, s);
            else
                alt_knob_tot[i] += i == 3u ? s : s * 2;
        }
    if ((s = panel_enc(EN_SELECT)) != 0)
        au.sel = (uint8_t)(((int32_t)au.sel + s + 3 * 16) % AS_NCIRC);
    if ((s = panel_enc(EN_ALGO)) != 0)
        alt_knob_tot[3] += s;
    if ((s = panel_enc(EN_PRESET)) != 0) {
        const engine_t *e = ENGINES[ENGI_ALTER];
        uint32_t pi = (uint32_t)(((int32_t)t->preset + s + 16 * (int32_t)e->npresets) % (int32_t)e->npresets), j;
        fm1_irq_off();                           /* (the ISR's offsets and the preset's values together) */
        apply_preset_to(t, pi);
        for (j = 0; j < AS_NTIMB; j++)
            alt_applied[j] = 0;
        fm1_irq_on();
        alt_say(e->presets[t->preset].name);
    }
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
                au.die_off_done &= (uint8_t)~(1u << i2);
            }
        }
        if (a->kind != AK_DIE)
            continue;
        if ((keys >> k) & 1u) {                  /* held long: the die off */
            if (au.die_t0[i2] && fm1_ms - au.die_t0[i2] >= ALT_DIE_HOLD_MS && !((au.die_off_done >> i2) & 1u)) {
                au.die_off_done |= (uint8_t)(1u << i2);
                au.cfg[i2].die = 0;
                alt_post_circle(i2);
                alt_say("DICE OFF");
            }
        } else if (au.die_t0[i2]) {              /* let go: a tap rolls */
            if (!((au.die_off_done >> i2) & 1u)) {
                au.cfg[i2].die = 1;
                alt_post_circle(i2);
                as_roll(&alt_seq, i2);
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

/* the key LEDs: the blocks that act lit (a muted circle's and a die that is off: the glow), the gaps dark */
static void alter_leds(uint8_t *nl, uint8_t *nd, uint8_t *nb, uint8_t *nm)
{
    uint32_t k;
    if (!alter_view() || !au.ready)
        return;
    for (k = 0; k < 27u; k++) {
        const alt_key_t *a = &ALT_KEY[k];
        int lit = 0, glow = 0;
        if (a->kind == AK_MUTE)
            lit = !au.cfg[a->arg].mute, glow = !lit;
        else if (a->kind == AK_DIE)
            lit = au.cfg[a->arg].die, glow = !lit;
        else if (a->kind == AK_FILL)
            lit = (int)((fm1_in.notes >> k) & 1u), glow = !lit;
        led_clear(nl, 14u + k);
        led_clear(nd, 14u + k);
        led_clear(nb, 14u + k);
        led_clear(nm, 14u + k);
        led_put(nl, 14u + k, lit);
        led_put(nd, 14u + k, glow);
    }
}

/* ------------------------------------------------------------------ the screen --- */
#define ALT_CX 120
#define ALT_CY 114
static const uint8_t ALT_R[AS_NCIRC] = {86, 64, 42};
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

static void alt_scene(int seq, int flash)
{
    const track_t *t = &trk[ALT_PART];
    const engine_t *e = ENGINES[ENGI_ALTER];
    char b[24];
    uint32_t i, j;
    static const char *const MODE[3] = {"SKIN", "LIQUID", "METAL"};
    /* the head: preset, mode, transport */
    cv_text(8, 2, &AF_M, t->preset < e->npresets ? e->presets[t->preset].name : "ALTER", ALT_INK);
    cv_text_r(166, 4, &AF_S, MODE[t->p[P_E3] % 3], ALT_DIM, cv_bg);
    b[0] = 0;
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
        int muted = au.cfg[i].mute, dim = seq && i != au.sel;
        uint16_t col = muted ? ALT_GREY : ALT_COL[i];
        int32_t r = ALT_R[i], x, y;
        if (dim)
            col = alt_fade(col, 30);
        for (j = 0; j < 96u; j++) {              /* the ring */
            alt_at(j * (0xFFFFFFFFu / 96u), r, &x, &y);
            cv_pset(x, y, alt_fade(col, 35));
        }
        for (j = 0; j < len; j++) {              /* the steps */
            int on = (int)((c->pat >> j) & 1u), here = song.playing && j == now;
            alt_at((uint32_t)(((uint64_t)j << 32) / len), r, &x, &y);
            if (here)
                alt_disc(x, y, 6, on ? ALT_INK : ALT_DIM);
            alt_disc(x, y, on ? 4 : 2, on ? col : ALT_GREY);
        }
        if (au.cfg[i].die) {                     /* a die: a small square at the top of its ring */
            alt_at(0, r, &x, &y);
            cv_rect(x + 7, y - 3, 6, 6, col);
        }
    }
    /* the middle: the flash of a hit, or the key held */
    if (au.msg && fm1_ms - au.msg_ms < 900u) {
        int32_t w = text_w(&AF_S, au.msg);
        cv_rect(ALT_CX - w / 2 - 4, ALT_CY - 9, w + 8, 18, RGB(0, 0, 0));
        cv_text(ALT_CX - w / 2, ALT_CY - 8, &AF_S, au.msg, ALT_INK);
    } else
        alt_disc(ALT_CX, ALT_CY, flash ? 9 : 3, flash ? ALT_INK : ALT_GREY);
    /* the foot */
    if (!seq) {
        static const char *const N[3] = {"MRPH", "SPRD", "HARM"};
        for (i = 0; i < 4u; i++) {
            int32_t x = 8 + (int32_t)i * 58;
            if (i < 3u) {
                int32_t v = clamp(t->p[ALT_TIMB_P[i]] - alt_applied[i], 0, 127);
                cv_text(x, 206, &AF_S, N[i], ALT_DIM);
                cv_rect(x, 225, 50, 5, ALT_DARK);
                cv_rect(x, 225, v * 50 / 127, 5, ALT_INK);
            } else {
                cv_text(x, 206, &AF_S, "MODE", ALT_DIM);
                cv_text(x, 220, &AF_S, MODE[t->p[P_E3] % 3], ALT_INK);
            }
        }
    } else {
        static const char *const N[4] = {"LEN", "HITS", "ROT", "PROB"};
        const as_cfg_t *c = &au.cfg[au.sel];
        int32_t v[4];
        v[0] = c->len, v[1] = c->hits, v[2] = c->rot, v[3] = c->prob * 100 / 127;
        cv_text(8, 196, &AF_S, ALT_CNAME[au.sel], ALT_COL[au.sel]);
        cv_text_r(234, 196, &AF_S, c->die ? "DICE ON" : "", ALT_DIM, cv_bg);
        for (i = 0; i < 4u; i++) {
            int32_t x = 8 + (int32_t)i * 58, n = 0, q = v[i];
            char d[6];
            if (q >= 100)
                d[n++] = (char)('0' + q / 100);
            if (q >= 10)
                d[n++] = (char)('0' + q / 10 % 10);
            d[n++] = (char)('0' + q % 10);
            if (i == 3u)
                d[n++] = '%';
            d[n] = 0;
            cv_text(x, 211, &AF_S, N[i], ALT_DIM);
            cv_text(x, 225, &AF_S, d, ALT_INK);
        }
    }
}

static void alter_draw(void)
{
    uint32_t sig = 0, i, h, flash, seq = (fm1_in.buttons >> panel.btn[B_SEQ]) & 1u;
    const track_t *t = &trk[ALT_PART];
    if ((h = alt_hits) != au.hits_seen) {
        au.hits_seen = h;
        au.flash_ms = fm1_ms;
    }
    flash = fm1_ms - au.flash_ms < 70u;
    for (i = 0; i < AS_NCIRC; i++)                /* what the picture depends on */
        sig = sig * 31u + alt_seq.c[i].pos + alt_seq.c[i].pat * 7u + (uint32_t)alt_seq.c[i].cfg.len * 131u +
              au.cfg[i].len + au.cfg[i].hits * 3u + au.cfg[i].rot * 5u + au.cfg[i].prob * 9u + au.cfg[i].mute * 11u +
              au.cfg[i].die * 13u;
    for (i = 0; i < 8u; i++)
        sig = sig * 31u + (uint32_t)t->p[P_E0 + i];
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
        alt_scene((int)seq, (int)flash);
        cv_oy = 0;
        cv_blit(0, i * 120u);
    }
}
