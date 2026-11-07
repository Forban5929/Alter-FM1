/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's MIDI LEARN (07/11, Romain: "le midi learn pour tout type de controller"; included by alter_ui.c in the
 * SYSTEM menu's part). SYSTEM (HOME held) > LEARN: a list. Its first row, GROUP (KNOB 1): CIRCLE I, II, III (a
 * setting of THAT circle, whichever is chosen), GLOBAL (the shared ones), ACTIONS (the FM-1's keys and buttons).
 * The other rows: what can be linked, with the control linked to it ("CC21 CH1", "C3 CH10", or "-").
 * KNOB 1 turned right on a row: it waits ("WAITING" blinks); the next CC or note your controller sends is linked to
 * it (a setting takes a CC only: a knob or a fader; an action a CC or a note: a button or a pad). KNOB 1 turned left:
 * the link cleared (waiting: cancelled). GLOBAL's last row, CLEAR ALL: every link cleared.
 * One control moves one thing, one thing has one control: a link learned again replaces the old one (either side).
 * A setting follows its control 0..127 over its whole range at once (no pick-up): set the controller's encoders to
 * ABSOLUTE 0-127. A sound's setting goes through the queue as a knob of the FM-1 (REC records it); the CONSOLE's EQ /
 * COMP, the GLOBAL ones are set as they are (not in REC, as on the device). An action is a key held as long as the
 * button (a CC: on from 64): DICE, CHAOS, the fills, the mutes act as their key does, OCT- / OCT+ as the buttons
 * (OCT- + DICE: velocity and chance), PLAY, REC and ARP as theirs. The standard CC map (CC7 LEVEL, CC74 the timbre ..)
 * still works for every CC not linked.
 * Kept with the settings: favorites.factory rows 5..8 (engines 5..8 do not exist, nothing else uses them), 128 bytes:
 * [0] ALM_MAGIC once set, [1] 0, then ALM_N entries of three bytes (midi_control.c). A code ([2]): circle c's setting t
 * c * 32 + t, a GLOBAL g 96 + g, an action a 128 + a (append only: a code is what the flash keeps) */
#define alm_tbl (&favorites.factory[5][0])
static const uint8_t *const alm_tab = &favorites.factory[5][0];
static alm_io_t alm_io_s;
static alm_io_t *const alm_io = &alm_io_s;
typedef char alm_fits[2u + 3u * ALM_N <= 4u * 32u ? 1 : -1];

#define ALM_C0 0u                                /* circle c's settings: c * 32 + t */
#define ALM_G0 96u
#define ALM_A0 128u
enum { ALK_P, ALK_CS, ALK_RMB, ALK_HITS, ALK_DOSE };
typedef struct { uint8_t kind, ref; const char *name; } alm_ct_t;
static const alm_ct_t ALM_CT[] = {               /* (name 0: the sound's own, as DEEP: the engine's names, the INSERT's) */
    {ALK_P, P_E0 + 0, 0}, {ALK_P, P_E0 + 1, 0}, {ALK_P, P_E0 + 2, 0}, {ALK_P, P_E0 + 3, 0},
    {ALK_P, P_E0 + 4, 0}, {ALK_P, P_E0 + 5, 0}, {ALK_P, P_E0 + 6, 0}, {ALK_P, P_E0 + 7, 0},
    {ALK_P, P_ATK, "ATTACK"}, {ALK_P, P_DEC, "DECAY"}, {ALK_P, P_SUS, "SUSTAIN"}, {ALK_P, P_REL, "RELEASE"},
    {ALK_P, P_DIST, "DRIVE"}, {ALK_CS, ACS_LOW, "LOW"}, {ALK_CS, ACS_MID, "MID"}, {ALK_CS, ACS_HIGH, "HIGH"},
    {ALK_CS, ACS_COMP, "COMP"}, {ALK_P, P_DLY, "DELAY"}, {ALK_P, P_REV, "REVERB"}, {ALK_P, P_LEVEL, "LEVEL"},
    {ALK_P, P_PAN, "PAN"}, {ALK_P, P_IMIX, "INSERT MIX"}, {ALK_P, P_IA, 0}, {ALK_P, P_IB, 0}, {ALK_P, P_IC, 0},
    {ALK_P, P_CHOR, "CHORUS"}, {ALK_RMB, 0, "RUMBLE"}, {ALK_P, P_GLIDE, "GLIDE"}, {ALK_HITS, 0, "HITS"},
    {ALK_DOSE, 0, "DICE DOSE"},
};
#define ALM_NCT ((uint32_t)NELEM(ALM_CT))
typedef char alm_ct_fits[ALM_NCT <= 32u ? 1 : -1];
enum { ALG_MASTER, ALG_TEMPO, ALG_SWING, ALG_FFREQ, ALG_FRES, ALG_FDRIVE, ALG_FMIX, ALG_FMORPH, ALG_DFB, ALG_DCOL,
       ALG_RSIZE, ALG_RDAMP, ALG_N };
static const char *const ALM_GT[ALG_N] = {"MASTER", "TEMPO", "SWING", "FILT FREQ", "FILT RES", "FILT DRIVE",
                                          "FILT MIX", "FILT MORPH", "DLY FEEDBK", "DLY COLOR", "REV SIZE", "REV DAMP"};
typedef struct { uint32_t keys; uint8_t btn; const char *name; } alm_at_t;
#define ALM_K(k) (1u << (k))
#define ALM_NOBTN 0xFFu
static const alm_at_t ALM_AT[] = {               /* (keys: ALT_KEY's; btn: a B_* button) */
    {ALM_K(0), ALM_NOBTN, "MUTE 1"}, {ALM_K(2), ALM_NOBTN, "MUTE 2"}, {ALM_K(4), ALM_NOBTN, "MUTE 3"},
    {ALM_K(7), ALM_NOBTN, "DICE 1"}, {ALM_K(9), ALM_NOBTN, "DICE 2"}, {ALM_K(11), ALM_NOBTN, "DICE 3"},
    {ALT_DICE_KEYS, ALM_NOBTN, "CHAOS"},
    {ALM_K(1), ALM_NOBTN, "CIRCLE 1"}, {ALM_K(3), ALM_NOBTN, "CIRCLE 2"}, {ALM_K(5), ALM_NOBTN, "CIRCLE 3"},
    {ALT_TC_KEYS, ALM_NOBTN, "CHAOS TOTAL"},
    {ALM_K(8), ALM_NOBTN, "DENSITY -"}, {ALM_K(10), ALM_NOBTN, "DENSITY +"},
    {ALM_K(14), ALM_NOBTN, "SHIFT -"}, {ALM_K(16), ALM_NOBTN, "SHIFT +"}, {ALM_K(18), ALM_NOBTN, "LOCK"},
    {ALM_K(13), ALM_NOBTN, "DUB DELAY"}, {ALM_K(15), ALM_NOBTN, "DUB REVERB"}, {ALM_K(17), ALM_NOBTN, "DESTROY"},
    {ALM_K(20), ALM_NOBTN, "ROLL 1/16"}, {ALM_K(22), ALM_NOBTN, "ROLL 1/8"}, {ALM_K(25), ALM_NOBTN, "TRIGGER"},
    {ALM_K(19), ALM_NOBTN, "SONG A"}, {ALM_K(21), ALM_NOBTN, "SONG B"}, {ALM_K(23), ALM_NOBTN, "SONG C"},
    {0, B_PLAY, "PLAY / STOP"}, {0, B_REC, "REC"}, {0, B_OCTDN, "OCT -"}, {0, B_OCTUP, "OCT +"},
    {0, B_ARP, "ARP (CHAIN)"},
};
#define ALM_NAT ((uint32_t)NELEM(ALM_AT))
typedef char alm_at_fits[ALM_NAT <= 64u ? 1 : -1];
#define ALM_NGRP 5u                              /* CIRCLE I II III, GLOBAL, ACTIONS */
static const char *const ALM_GRP[ALM_NGRP] = {"CIRCLE I", "CIRCLE II", "CIRCLE III", "GLOBAL", "ACTIONS"};

static uint8_t alm_grp, alm_wait;                /* the group shown; the row waiting for a control + 1 (0: none) */
static uint8_t alm_seen[ALM_N], alm_on[ALM_N];   /* each link's messages already taken; an action held */
static const char *alm_msg;                      /* (the foot's line a while: LEARNED, USE A KNOB, FULL) */
static uint32_t alm_msg_ms;

static uint8_t *alm_e(uint32_t i) { return alm_tbl + 2u + 3u * i; }
static void alm_init(void)                       /* (rows 5..8 held Felucca's favourites before: cleared once) */
{
    uint32_t k;
    if (alm_tbl[0] == ALM_MAGIC)
        return;
    fm1_irq_off();
    for (k = 1; k < 128u; k++)
        alm_tbl[k] = 0;
    alm_tbl[0] = ALM_MAGIC;
    fm1_irq_on();
}
static int32_t alm_find(uint32_t code)           /* the entry linked to code, -1 none */
{
    uint32_t i;
    for (i = 0; i < ALM_N; i++)
        if ((alm_e(i)[1] & 0x80u) && alm_e(i)[2] == code)
            return (int32_t)i;
    return -1;
}
static void alm_say(const char *s)
{
    alm_msg = s;
    alm_msg_ms = fm1_ms | 1u;
}

/* a GLOBAL's value v (0..127) */
static void alm_global(uint32_t g, uint32_t v)
{
    uint32_t sn = afl_morph < 64u ? 0u : 1u;     /* (the FILTER: the snapshot MORPH is nearest) */
    switch (g) {
    case ALG_MASTER:
        alt_mgain = (uint8_t)v;
        alt_sys[5] = (uint8_t)(v + 1u);          /* (kept with the next save of the settings) */
        break;
    case ALG_TEMPO: song.g[G_BPM] = (int16_t)(40 + (int32_t)v * 200 / 127); break;
    case ALG_SWING: song.g[G_SWING] = (int16_t)((int32_t)v * 100 / 127); break;
    case ALG_FFREQ: case ALG_FRES: case ALG_FDRIVE: case ALG_FMIX: {
        static const uint8_t K[4] = {AFL_FREQ, AFL_RES, AFL_DRIVE, AFL_MIX};
        afl[sn][K[g - ALG_FFREQ]] = (uint8_t)v;
        afl_edit |= (uint8_t)(1u << sn);
        break;
    }
    case ALG_FMORPH: afl_morph = (uint8_t)v; break;
    case ALG_DFB: song.g[G_DFDBK] = (int16_t)((int32_t)v * 120 / 127); afx_edit |= 1u << AFX_S_DLY; break;
    case ALG_DCOL: song.g[G_DCOLOR] = (int16_t)v; afx_edit |= 1u << AFX_S_DLY; break;
    case ALG_RSIZE: song.g[G_RSIZE] = (int16_t)v; afx_edit |= 1u << AFX_S_REV; break;
    default: song.g[G_RDAMP] = (int16_t)v; afx_edit |= 1u << AFX_S_REV; break;
    }
}
/* code's setting to v (0..127 over its range) */
static void alm_set(uint32_t code, uint32_t v)
{
    uint32_t c = code / 32u, t = code % 32u;
    const alm_ct_t *a;
    if (code >= ALM_A0)
        return;
    if (code >= ALM_G0) {
        if (code - ALM_G0 < ALG_N)
            alm_global(code - ALM_G0, v);
        return;
    }
    if (c >= AS_NCIRC || t >= ALM_NCT)
        return;
    a = &ALM_CT[t];
    switch (a->kind) {
    case ALK_CS: acs_v[c][a->ref % ACS_N] = (uint8_t)v; break;
    case ALK_RMB: alt_rmb[c] = (uint8_t)v; break;
    case ALK_HITS: {
        as_cfg_t *f = &au.cfg[c];
        uint32_t h = (v * f->len + 63u) / 127u;
        if (h != f->hits) {
            f->hits = (uint8_t)h;
            alt_post_circle(c);
        }
        break;
    }
    case ALK_DOSE:
        au.cfg[c].dose = (uint8_t)v;
        alt_post_circle(c);
        break;
    default: {                                   /* a sound's: through the queue, as its knob (the die's offset on top) */
        const param_desc_t *d = track_desc(&trk[c], a->ref);
        int32_t to, base, sl = alt_slot(a->ref), dd;
        if (!d || d->max <= d->min)
            return;
        to = d->min + ((int32_t)v * (d->max - d->min) + 63) / 127;
        base = trk[c].p[a->ref] - (sl >= 0 ? alt_applied[c][sl] : 0);
        for (dd = to - base; dd; ) {             /* (a queue step moves 100 at most) */
            int32_t st = clamp(dd, -100, 100);
            alt_qpush(c, a->ref, st);
            dd -= st;
        }
        break;
    }
    }
}

/* once a pass (alter_ui_input, first): the links' news. A setting: its latest value; an action: held from on to off
 * (on and off within one pass: held for this pass only, a tap) */
static void alm_poll(void)
{
    uint32_t i, k = 0, b = 0;
    static uint32_t bwas;
    alm_init();
    if (alm_wait && (au.mn != QP_SYS || alt_sys_tab != SYS_TLEARN)) {   /* (the page left: no more waiting) */
        alm_wait = 0;
        alm_io_s.arm = 0;
    }
    for (i = 0; i < ALM_N; i++) {
        const uint8_t *e = alm_e(i);
        uint8_t sq = alm_io_s.seq[i], d = (uint8_t)(sq - alm_seen[i]);
        uint32_t v = alm_io_s.v[i], on, tap;
        alm_seen[i] = sq;
        if (!(e[1] & 0x80u)) {
            alm_on[i] = 0;
            continue;
        }
        if (e[2] < ALM_A0) {
            if (d && au.ready)
                alm_set(e[2], v);
            continue;
        }
        on = v >= 64u;
        tap = d >= 2u && !on && !alm_on[i];
        alm_on[i] = (uint8_t)on;
        if ((on || tap) && e[2] - ALM_A0 < ALM_NAT) {
            const alm_at_t *a = &ALM_AT[e[2] - ALM_A0];
            k |= a->keys;
            if (a->btn != ALM_NOBTN)
                b |= 1u << panel.btn[a->btn];
        }
    }
    alm_keys = k;
    alm_bdown = b & ~bwas;
    alm_btns = b;
    bwas = b;
}

/* ---- the LEARN tab of SYSTEM -------------------------------------------------------------------------------- */
static uint32_t alm_rows(void)                   /* GROUP, then the group's */
{
    return 1u + (alm_grp < AS_NCIRC ? ALM_NCT : alm_grp == 3u ? ALG_N + 1u : ALM_NAT);
}
static uint32_t alm_code(uint32_t r)             /* row r's code (r >= 1); 255: CLEAR ALL */
{
    r--;
    if (alm_grp < AS_NCIRC)
        return ALM_C0 + alm_grp * 32u + r;
    if (alm_grp == 3u)
        return r < ALG_N ? ALM_G0 + r : 255u;
    return ALM_A0 + r;
}
static const char *alm_name(uint32_t r)
{
    uint32_t code;
    if (r == 0)
        return "GROUP";
    code = alm_code(r);
    if (code == 255u)
        return "CLEAR ALL";
    if (code >= ALM_A0)
        return ALM_AT[(code - ALM_A0) % ALM_NAT].name;
    if (code >= ALM_G0)
        return ALM_GT[(code - ALM_G0) % ALG_N];
    {
        const alm_ct_t *a = &ALM_CT[code % 32u % ALM_NCT];
        const param_desc_t *d;
        if (a->name)
            return a->name;
        d = track_desc(&trk[code / 32u % AS_NCIRC], a->ref);
        return d && d->label ? d->label : "-";
    }
}
static const char *alm_text(uint32_t r)
{
    static char tb[16];
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    uint32_t code, j = 0, n, ch;
    int32_t i;
    if (r == 0)
        return ALM_GRP[alm_grp % ALM_NGRP];
    if (alm_wait == r)
        return (fm1_ms / 400u) & 1u ? "WAITING" : "";
    code = alm_code(r);
    if (code == 255u)
        return "KNOB 1 <";
    if ((i = alm_find(code)) < 0)
        return "-";
    n = alm_e((uint32_t)i)[0];
    ch = (alm_e((uint32_t)i)[1] & 15u) + 1u;
    if (n & 0x80u) {                             /* a note: C3 (C4 = 60, as the FM-1's MIDI) */
        const char *s = NN[(n & 127u) % 12u];
        while (*s)
            tb[j++] = *s++;
        alt_num(tb + j, (int32_t)((n & 127u) / 12u) - 1, 0);
    } else {
        tb[j++] = 'C', tb[j++] = 'C';
        alt_num(tb + j, (int32_t)n, 0);
    }
    while (tb[j])
        j++;
    tb[j++] = ' ', tb[j++] = 'C', tb[j++] = 'H';
    alt_num(tb + j, (int32_t)ch, 0);
    return tb;
}
static void alm_clear(uint32_t code)
{
    int32_t i = alm_find(code);
    if (i < 0)
        return;
    fm1_irq_off();
    alm_e((uint32_t)i)[1] = 0;
    fm1_irq_on();
    settings_save();
    alm_say("LINK CLEARED");
}
/* code linked to the control heard (src: number | 0x80 for a note, ch); 0: refused */
static int alm_store(uint32_t code, uint32_t src, uint32_t ch)
{
    uint32_t i;
    int32_t f = -1;
    if (code < ALM_A0 && (src & 0x80u)) {
        alm_say("A SETTING NEEDS A KNOB (CC)");
        return 0;
    }
    fm1_irq_off();
    for (i = 0; i < ALM_N; i++) {                /* (one control, one thing: the old links of either side go) */
        uint8_t *e = alm_e(i);
        if ((e[1] & 0x80u) && (e[2] == code || (e[0] == src && (e[1] & 15u) == ch)))
            e[1] = 0;
        if (!(e[1] & 0x80u) && f < 0)
            f = (int32_t)i;
    }
    if (f >= 0) {
        uint8_t *e = alm_e((uint32_t)f);
        e[0] = (uint8_t)src;
        e[2] = (uint8_t)code;
        e[1] = (uint8_t)(0x80u | (ch & 15u));
        alm_seen[f] = alm_io_s.seq[f];
        alm_on[f] = 0;
    }
    fm1_irq_on();
    if (f < 0) {
        alm_say("FULL: 42 LINKS");
        return 0;
    }
    settings_save();
    alm_say("LEARNED");
    return 1;
}
/* KNOB 1 on row r: GROUP its group; a row: right = wait for a control, left = clear (waiting: cancel) */
static void alm_turn(uint32_t r, int32_t s)
{
    uint32_t code;
    if (r == 0) {
        alm_grp = (uint8_t)clamp((int32_t)alm_grp + (s > 0 ? 1 : -1), 0, (int32_t)ALM_NGRP - 1);
        alm_wait = 0;
        alm_io_s.arm = 0;
        return;
    }
    code = alm_code(r);
    if (s < 0 && alm_wait) {
        alm_wait = 0;
        alm_io_s.arm = 0;
        alm_say("CANCELLED");
        return;
    }
    if (code == 255u) {
        if (s < 0) {
            uint32_t i;
            fm1_irq_off();
            for (i = 0; i < ALM_N; i++)
                alm_e(i)[1] = 0;
            fm1_irq_on();
            settings_save();
            alm_say("ALL LINKS CLEARED");
        }
        return;
    }
    if (s < 0) {
        alm_clear(code);
        return;
    }
    alm_io_s.heard[2] = 0;
    alm_io_s.arm = 1;
    alm_wait = (uint8_t)r;
}
static void alm_input(void)                      /* (SYSTEM's input, the LEARN tab: what came while waiting) */
{
    if (alm_wait && alm_io_s.heard[2]) {
        uint32_t src = alm_io_s.heard[0], ch = alm_io_s.heard[1], r = alm_wait;
        alm_io_s.arm = 0;
        alm_io_s.heard[2] = 0;
        alm_wait = 0;
        (void)alm_store(alm_code(r), src, ch);
    }
}
static uint32_t alm_sig(void)                    /* (the screen: redrawn when this moves) */
{
    uint32_t i, s = alm_grp * 7u + alm_wait * 11u + (alm_wait ? (fm1_ms / 400u) & 1u : 0u) +
                    (alm_msg && fm1_ms - alm_msg_ms < 2000u ? 13u : 0u);
    for (i = 0; i < 2u + 3u * ALM_N; i++)
        s = s * 31u + alm_tbl[i];
    for (i = 0; i < AS_NCIRC; i++)               /* (the sounds' names of their settings) */
        s = s * 31u + au.pre[i];
    return s;
}
