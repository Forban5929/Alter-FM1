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
 * segmented bars. Redrawn when a step moves (alt_sig).
 * The keyboard (v1.2.12, planche 1b, Romain 25/11 21h; v1.2.13, Romain 22h44: the MUTEs and CIRCLE keys where the normal
 * view has them, a selection): white keys 0 2 4 MUTE I II III (at once, as on the normal view: the note ringing goes on),
 * black keys 1 3 5 CIRCLE I II III (a selection, ALL at first: a key adds or takes its circle, none left = ALL again; DUB,
 * MONTEE's reverb and FILL act on it), A B C on the white keys after a gap (keys 7 9 11), FILTRE A / B (the MASTER FILTER's MORPH glides to A or B in two sixteenths and stays; both: the middle), DUB DELAY and
 * DUB REVERB (the selection), TAPE STOP, PREV SC / NEXT SC (the scene before / after, at the next 1), TEMP (held:
 * every knob turned goes back on its release); white keys A B C (that SONG slot from the next bar, no chain), RATE - +
 * (the FILL's: 1/4 1/8 1/16 1/32), FILL (held: the selection plays a hit at every RATE step), MONTEE (held: the
 * MASTER FILTER's FREQ and the selection's REVERB sends rise over 4 bars, the bars counted; let go: back at once on the next 1).
 * (ECHO FIGE: taken out by Romain before it was made; the white keys 3-9 free.) */

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

/* the keys: k = semitones above F3 (ALT_KEY's numbers) */
enum { APK_NONE, APK_MUTE, APK_FA, APK_FB, APK_DDLY, APK_DREV, APK_TAPE, APK_PREV, APK_NEXT, APK_TEMP, APK_SONG,
       APK_RDN, APK_RUP, APK_FILL, APK_UP, APK_SEL };
typedef struct { uint8_t f, a; const char *name; } aper_key_t;
static const aper_key_t APK[27] = {
    [0] = {APK_MUTE, 0, "MUTE I"}, [2] = {APK_MUTE, 1, "MUTE II"}, [4] = {APK_MUTE, 2, "MUTE III"},
    [1] = {APK_SEL, 0, "CIRCLE I"}, [3] = {APK_SEL, 1, "CIRCLE II"}, [5] = {APK_SEL, 2, "CIRCLE III"},
    [8] = {APK_FA, 0, "FILTER A"}, [10] = {APK_FB, 0, "FILTER B"},
    [13] = {APK_DDLY, 0, "DUB DELAY"}, [15] = {APK_DREV, 0, "DUB REVERB"}, [17] = {APK_TAPE, 0, "TAPE STOP"},
    [20] = {APK_PREV, 0, "PREV SC"}, [22] = {APK_NEXT, 0, "NEXT SC"}, [25] = {APK_TEMP, 0, "TEMP"},
    [7] = {APK_SONG, 0, "A"}, [9] = {APK_SONG, 1, "B"}, [11] = {APK_SONG, 2, "C"},
    [18] = {APK_RDN, 0, "RATE -"}, [19] = {APK_RUP, 0, "RATE +"},
    [21] = {APK_FILL, 0, "FILL"},
    [26] = {APK_UP, 0, "MONTEE"},
};
static const char *const APER_RATE[4] = {"1/4", "1/8", "1/16", "1/32"};
#define APER_UP_STEPS 64u                        /* MONTEE: full after 4 bars */
#define APER_UP_LOW 40                           /* MONTEE: the filter's FREQ from here at most */
#define APER_NTEMP 32u

static struct {
    uint8_t bank;
    int16_t v[4];                                /* the knobs' values shown (0..127, -1: none) */
    uint32_t turn_ms;                            /* the last turn (the values follow the machine after it) */
    uint32_t play_t0;                            /* PLAY's press (held 3 s: this page) */
    uint8_t play_stop, play_held;                /* PLAY pressed while playing: stop at its release, unless held */
    uint32_t kt0[27];                            /* each key's press (fm1_ms | 1), 0: up */
    uint8_t sel;                                 /* the circles selected (bits), 0: ALL */
    uint16_t sq;                                 /* the scene row waiting for the 1 (+1), 0: none */
    uint32_t sq_at;
    uint8_t fz_on, fz_from, fz_to;               /* FILTRE A / B: MORPH gliding from .. to */
    uint32_t fz_t0;
    uint8_t rate, fill, up_sel;                  /* the FILL's rate (0..3, 2 = 1/16); FILL held; MONTEE's circles */
    uint8_t up_on, up_drop;                      /* MONTEE: held (rising), let go (waiting for the 1) */
    uint32_t up_g0, up_at, up_ms0, up_n;         /* its first step, the drop's step; (stopped: its press); steps risen */
    int16_t up_base[4], up_v[4];                 /* FREQ + the three REVERB sends before it, as set now */
    uint8_t temp_on, temp_n;                     /* TEMP held; the knobs it gives back */
    struct { uint8_t bank, i, v; } temp[APER_NTEMP];
} aper = {.rate = 2u};

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
static uint32_t aper_play_t0(void) { return aper.play_t0; }
static void aper_play_take(void) { aper.play_t0 = 0; aper.play_stop = 0; aper.play_held = 1; }
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
    if (aper.temp_on) {                          /* TEMP held: this knob's place before, once */
        uint32_t j;
        for (j = 0; j < aper.temp_n; j++)
            if (aper.temp[j].bank == aper.bank && aper.temp[j].i == i)
                break;
        if (j == aper.temp_n && j < APER_NTEMP) {
            aper.temp[j].bank = aper.bank;
            aper.temp[j].i = (uint8_t)i;
            aper.temp[j].v = (uint8_t)aper.v[i];
            aper.temp_n++;
        }
    }
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
    s = s * 31u + aper.rate * 3u + aper.sel * 5u + aper.sq * 7u + aper.up_on * 11u + aper.up_drop * 13u + aper.fill;
    if (aper.up_on || aper.up_drop)
        s = s * 31u + alt_gstep;
    if (aper.sq)
        s = s * 31u + ((fm1_ms >> 8) & 1u);
    return s * 31u + (uint32_t)song.playing;
}

/* the next 1 (alt_gstep's value when the last sixteenth before it plays; acx_poll's rule) and whether it came */
static uint32_t aper_bar_at(void) { return ((alt_gstep ? alt_gstep - 1u : 0u) / 16u + 1u) * 16u; }
static int aper_due(uint32_t at) { return !song.playing || alt_gstep >= at; }
static uint32_t aper_step_ms(void) { return div_samples(2) * 1000u / FS; }
static void aper_mute(uint32_t c)
{
    au.cfg[c].mute ^= 1u;
    alt_post_circle(c);
}
static int aper_row_ok(uint32_t r)               /* a scene to go to: a factory one or a USER one stored */
{
    if (r == 0u || r > ALT_NFACT + ALT_NUSER)
        return 0;
    return r <= ALT_NFACT || alt_used_s[(r - 1u - ALT_NFACT) % ALT_NUSER];
}
static void aper_scene_step(int32_t d)           /* PREV / NEXT SC: the one before / after (the one waiting, else the one
                                                  * loaded last), at the next 1 */
{
    static char nb[16];
    uint32_t n = ALT_NFACT + ALT_NUSER, r = aper.sq ? aper.sq - 1u : alt_row_cur, j;
    for (j = 0; j < n; j++) {
        r = (uint32_t)(((int32_t)r - 1 + d + (int32_t)n) % (int32_t)n) + 1u;
        if (aper_row_ok(r))
            break;
    }
    if (!aper_row_ok(r))
        return;
    aper.sq = (uint16_t)(r + 1u);
    aper.sq_at = aper_bar_at();
    alt_say(alt_row_name(0, r, nb));
}
static void aper_glide(uint32_t to)              /* FILTRE A / B: MORPH to A (0), B (127) or the middle, in two steps */
{
    aper.fz_from = afl_morph;
    aper.fz_to = (uint8_t)to;
    aper.fz_t0 = fm1_ms;
    aper.fz_on = 1;
}
static uint32_t aper_up_code(uint32_t j) { return j ? (j - 1u) * 32u + 18u : ALM_G0 + ALG_FFREQ; }
static void aper_up_set(uint32_t j, int32_t v)
{
    v = clamp(v, 0, 127);
    if (aper.up_v[j] != v) {
        aper.up_v[j] = (int16_t)v;
        alm_set(aper_up_code(j), (uint32_t)v);
    }
}
static void aper_temp_back(void)                 /* TEMP let go: every knob turned back where it was */
{
    uint32_t j, b = aper.bank;
    for (j = 0; j < aper.temp_n; j++) {
        aper.bank = aper.temp[j].bank;
        aper_set(aper.temp[j].i, aper.temp[j].v);
        if (aper.bank == b)                      /* (shown at once: the machine takes it a moment later) */
            aper.v[aper.temp[j].i] = aper.temp[j].v;
    }
    aper.bank = (uint8_t)b;
    aper.temp_n = 0;
    aper.turn_ms = fm1_ms;
}
/* each turn of the main loop (alter_ui_input), on the page or not: what waits for the 1, the glides, MONTEE */
static void aper_poll(void)
{
    uint32_t j;
    if (au.mn != QP_PERF) {                      /* (off the page: the fills and dubs as the normal view has them) */
        alt_ptgt = 0;
        alt_prate = 0;
        alt_dub_mask = 0;
        aper.fill = 0;
        if (aper.temp_on) {
            aper.temp_on = 0;
            aper_temp_back();
        }
        if (aper.up_on) {                        /* (MONTEE held as the page closed: dropped now) */
            aper.up_on = 0;
            aper.up_drop = 1;
            aper.up_at = 0;
        }
    }
    if (aper.sq && aper_due(aper.sq_at)) {
        uint32_t r = aper.sq - 1u;
        aper.sq = 0;
        (void)alt_row_load(0, r);
        aper_read();
    }
    if (aper.fz_on) {
        uint32_t el = fm1_ms - aper.fz_t0, d = 2u * aper_step_ms() + 1u;
        if (el >= d) {
            afl_morph = aper.fz_to;
            aper.fz_on = 0;
        } else
            afl_morph = (uint8_t)((int32_t)aper.fz_from + ((int32_t)aper.fz_to - (int32_t)aper.fz_from) * (int32_t)el / (int32_t)d);
    }
    if (aper.up_on) {                            /* MONTEE: FREQ and the sends from where they were to the top */
        uint32_t n = song.playing ? alt_gstep - aper.up_g0 : (fm1_ms - aper.up_ms0) / (aper_step_ms() + 1u);
        if (n > APER_UP_STEPS)
            n = APER_UP_STEPS;
        aper.up_n = n;
        for (j = 0; j < 4u; j++) {
            int32_t b = aper.up_base[j], top = j ? 110 : 127;
            if (j && !((aper.up_sel >> (j - 1u)) & 1u))
                continue;                        /* (a circle not selected: its send stays) */
            if (!j && b > APER_UP_LOW)           /* (the filter open already: it starts closed, then opens) */
                b = APER_UP_LOW;
            aper_up_set(j, b >= top ? b : b + (top - b) * (int32_t)n / (int32_t)APER_UP_STEPS);
        }
    }
    if (aper.up_drop && aper_due(aper.up_at)) {  /* the drop: all back at once */
        aper.up_drop = 0;
        for (j = 0; j < 4u; j++)
            aper_up_set(j, aper.up_base[j]);
        aper_read();
        alt_say("DROP");
    }
}
/* the keys on the page (alter_ui_input): keys held, down the ones just pressed */
static void aper_keys(uint32_t keys, uint32_t down)
{
    uint32_t k, h = 0, fa, fb, up = au.keys & ~keys;
    for (k = 0; k < 27u; k++) {
        const aper_key_t *a = &APK[k];
        if (!a->f)
            continue;
        if ((down >> k) & 1u) {
            aper.kt0[k] = fm1_ms | 1u;
            if (a->f != APK_SEL)
                alt_say(a->name);
            switch (a->f) {
            case APK_MUTE:                       /* (at once, as on the normal view: from its next hit) */
                aper_mute(a->a);
                break;
            case APK_SEL: {                      /* a circle in or out of the selection; none (or all) = ALL */
                static const char *const SN[8] = {"ALL", "I", "II", "I + II", "III", "I + III", "II + III", "ALL"};
                aper.sel ^= (uint8_t)(1u << a->a);
                if (aper.sel == 7u)
                    aper.sel = 0;
                alt_say(SN[aper.sel]);
                break;
            }
            case APK_FA:
            case APK_FB:
                fa = (keys >> 8) & 1u;
                fb = (keys >> 10) & 1u;
                aper_glide(fa && fb ? 64u : a->f == APK_FA ? 0u : 127u);
                if (fa && fb)
                    alt_say("FILTER A+B");
                break;
            case APK_PREV:
            case APK_NEXT:
                aper_scene_step(a->f == APK_NEXT ? 1 : -1);
                break;
            case APK_TEMP:
                aper.temp_on = 1;
                aper.temp_n = 0;
                break;
            case APK_SONG:
                alt_sg_key(a->a);
                asg.wr = 0;                      /* (no chain on this page: one slot, from the next bar) */
                break;
            case APK_RDN:
            case APK_RUP:
                aper.rate = (uint8_t)clamp((int32_t)aper.rate + (a->f == APK_RUP ? 1 : -1), 0, 3);
                alt_say(APER_RATE[aper.rate]);
                break;
            case APK_FILL:
                aper.fill = 1;
                break;
            case APK_UP:
                if (aper.up_drop) {              /* (pressed again before its drop: on from where it had risen) */
                    aper.up_g0 = alt_gstep - aper.up_n;
                    aper.up_ms0 = fm1_ms - aper.up_n * aper_step_ms();
                } else {
                    uint32_t j;
                    for (j = 0; j < 4u; j++)
                        aper.up_base[j] = aper.up_v[j] = (int16_t)alm_get(aper_up_code(j));
                    aper.up_g0 = alt_gstep;
                    aper.up_ms0 = fm1_ms;
                    aper.up_n = 0;
                    aper.up_sel = aper.sel ? aper.sel : 7u;
                }
                aper.up_drop = 0;
                aper.up_on = 1;
                break;
            }
        }
        if ((up >> k) & 1u) {
            aper.kt0[k] = 0;
            if (a->f == APK_TEMP) {
                aper.temp_on = 0;
                aper_temp_back();
                alt_say("BACK");
            } else if (a->f == APK_UP && aper.up_on) {
                aper.up_on = 0;
                aper.up_drop = 1;
                aper.up_at = aper_bar_at();
            } else if (a->f == APK_FILL)
                aper.fill = 0;
        }
        if ((keys >> k) & 1u)
            h |= a->f == APK_DDLY ? 1u << AF_DLY : a->f == APK_DREV ? 1u << AF_REV : a->f == APK_TAPE ? 1u << AF_TAPE : 0u;
    }
    if (aper.fill)
        h |= 1u << AF_ROLL16;
    alt_dub_mask = (uint8_t)(aper.sel ? aper.sel : 7u);
    alt_prate = (uint8_t)(aper.rate + 1u);
    alt_ptgt = (uint8_t)(aper.sel ? aper.sel : 7u);
    alt_hold = (uint16_t)h;
}
/* the keys' lights: 0 dark, 1 a function, 2 a state on (MUTE, the filter's side, the FILL rate), 3 held / waiting */
static uint32_t aper_led(uint32_t k)
{
    const aper_key_t *a = &APK[k];
    uint32_t held = (ALT_NOTES >> k) & 1u;
    switch (a->f) {
    case APK_NONE:
        return 0;
    case APK_MUTE:
        return au.cfg[a->a].mute ? 2u : 1u;
    case APK_SEL:
        return (aper.sel >> a->a) & 1u ? 2u : 1u;
    case APK_FA:
        return held ? 3u : afl_morph == 0u ? 2u : afl_morph == 64u ? 2u : 1u;
    case APK_FB:
        return held ? 3u : afl_morph == 127u ? 2u : afl_morph == 64u ? 2u : 1u;
    case APK_PREV:
    case APK_NEXT:
        return aper.sq ? 3u : held ? 3u : 1u;
    case APK_SONG:
        return sg_play == a->a + 1u && sg_slot[a->a].used ? 2u : asg.q == a->a + 1u ? 3u : held ? 3u : 1u;
    case APK_UP:
        return aper.up_on || aper.up_drop ? 3u : 1u;
    default:
        return held ? 3u : 1u;
    }
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
    static const int32_t R0[AS_NCIRC] = {63, 45, 27}, R1[AS_NCIRC] = {76, 58, 40};   /* (v1.2.13: I outside, as the
                                                  * normal view) */
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
        uint16_t col = aper.sel && !((aper.sel >> c) & 1u) ? alt_fade(ALT_COL[c], 45) : ALT_COL[c];   /* (not selected:
                                                  * dimmer) */
        for (k = 0; k < len; k++) {
            uint32_t t0 = k * step + gap / 2u, span = step > gap ? step - gap : step, hit = (x->pat >> k) & 1u;
            int now = song.playing && k == cur && !muted;
            if (muted)
                aper_cell(R0[c], R1[c], t0, span, alt_fade(ALT_GREY, 40), 0);
            else if (now)
                aper_cell(R0[c], R1[c], t0, span, hit ? col : alt_fade(col, 40), 1);   /* (v1.2.12: the hit in its colour) */
            else if (hit) {
                int32_t r;
                aper_cell(R0[c] - 1, R1[c] + 1, t0, span, alt_fade(col, 30), 0);   /* (its glow) */
                aper_cell(R0[c], R1[c], t0, span, col, 0);
                for (r = R0[c] + 3; r <= R1[c] - 3; r += 2)   /* (filled lightly: Romain, only outlines before) */
                    alt_arc(APER_CX, APER_CY, r, t0, span, alt_fade(col, 35), 1);
            } else
                aper_cell(R0[c], R1[c], t0, span, alt_fade(ALT_GREY, 45), 0);
        }
    }
    cv_text(APER_CX - text_w(ALT_FS, APER_B[aper.bank].name) / 2, APER_CY - 15, ALT_FS, APER_B[aper.bank].name, ALT_GOLD);
    {
        static const char *const SN[8] = {"ALL", "I", "II", "I+II", "III", "I+III", "II+III", "ALL"};
        const char *t = SN[aper.sel & 7u];
        cv_text(APER_CX - text_w(ALT_FS, t) / 2, APER_CY - 3, ALT_FS, t, aper.sel ? ALT_INK : ALT_DIM);
    }
    {                                            /* under it: MONTEE's bars, the drop, a scene waiting, or the FILL rate */
        char st[16];
        uint16_t sc = ALT_DIM;
        if (aper.up_on) {
            uint32_t n = song.playing ? (alt_gstep - aper.up_g0) / 16u + 1u : 1u;
            alt_scopy(st, "BAR ", sizeof st);
            st[4] = (char)('0' + n / 10u % 10u), st[5] = (char)('0' + n % 10u), st[6] = 0;
            if (n < 10u)
                st[4] = st[5], st[5] = 0;
            sc = ALT_INK;
        } else if (aper.up_drop) {
            uint32_t n = aper.up_at > alt_gstep ? aper.up_at - alt_gstep : 0u;
            alt_scopy(st, "DROP ", sizeof st);
            st[5] = (char)('0' + n / 10u % 10u), st[6] = (char)('0' + n % 10u), st[7] = 0;
            if (n < 10u)
                st[5] = st[6], st[6] = 0;
            sc = ALT_GOLD;
        } else if (aper.sq) {
            static char nb[16];
            alt_scopy(st, alt_row_name(0, aper.sq - 1u, nb), sizeof st);
            sc = (fm1_ms >> 8) & 1u ? ALT_GOLD : ALT_DIM;
        } else
            alt_scopy(st, APER_RATE[aper.rate % 4u], sizeof st);
        cv_text(APER_CX - text_w(ALT_FS, st) / 2, APER_CY + 9, ALT_FS, st, sc);
    }
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
