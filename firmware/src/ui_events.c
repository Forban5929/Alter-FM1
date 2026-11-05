/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */

#define EV_VEL 0xFBu                                    /* + ADD's four before the parameters (ui.ev_id): a VEL, */
#define EV_NUDGE 0xFCu                                  /* a NUDGE, */
#define EV_CHANCE 0xFDu                                 /* a CHANCE, */
#define EV_RATCH 0xFEu                                  /* a RATCH (0xFF: none) */
enum { EVK_TOP, EVK_REC, EVK_CHANCE, EVK_RATCH, EVK_ADD, EVK_NUDGE, EVK_QNT, EVK_VEL };   /* (NUDGE, QUANTIZE: 1.2,
                                                                                           * #162; VEL: 1.5, #173) */
#define EVC(k, a) ((uint16_t)((uint32_t)(k) << 8 | (a)))   /* a row: its kind, its record (EVK_REC) or its step */
#define EV_ROWS (3u + MOTION_MAX + 4u * NSTEP)
static int ev_sid(uint32_t id) { return id >= EV_VEL && id <= EV_RATCH; }   /* a step's own field */

/* id has a row's name on track t: motion records it, and the track's sound has it (DRUM's lane levels on a DRUM
 * track, the engine's own E1..E8 that it labels; not DIGITAL's operators, retired); CHANCE, RATCH, NUDGE, VEL always */
static int ev_id_ok(const track_t *t, uint32_t id)
{
    const param_desc_t *d;
    if (ev_sid(id))
        return 1;
    if (id >= P_COUNT || !motion_param(id) || id == P_ED_FX || (id >= P_FM1_ATK && id <= P_FM4_LEVEL))
        return 0;
    if (id >= P_LN0 && id <= P_LN7)
        return drum_track(t);
    d = track_desc(t, id);
    return d && d->label && d->label[0] && d->label[0] != '-' && d->max > d->min;
}

/* a step's CHANCE (kind EVK_CHANCE), RATCH (EVK_RATCH), NUDGE (EVK_NUDGE) or VEL (EVK_VEL): its value, its default
 * (100 %, x1, 0, 96), at it, set (VEL 96 is stored as 0, as a step that never had one) */
static int32_t ev_sval(const step_t *s, uint32_t k)
{
    return k == EVK_CHANCE ? (int32_t)step_chance_v(s) : k == EVK_RATCH ? (int32_t)step_ratchet(s) :
           k == EVK_VEL ? (s->vel ? s->vel : 96) : step_nudge(s);
}
static int32_t ev_sdefv(uint32_t k) { return k == EVK_CHANCE ? 100 : k == EVK_RATCH ? 1 : k == EVK_VEL ? 96 : 0; }
static int ev_sdef(const step_t *s, uint32_t k) { return ev_sval(s, k) == ev_sdefv(k); }
/* 1.5.5 (#209): CHANCE's value turned on (s detents, d of them accelerated): 0..100 % accelerated, then past 100 the
 * conditions one a detent (core.h step_chance_v: 101..); a fast turn stops at 100 on the way in and out */
static const char COND_NAME[NCOND][6] = {"1:2", "2:2", "1:3", "2:3", "3:3", "1:4", "2:4", "3:4", "4:4", "FILL", "!FILL"};
/* a VEL row (shown): a note step; it plays: notes, or a hit not accented, and not an ACC step (127) */
static int ev_vel_row(const step_t *s) { return s->time == ST_NOTE && (s->n || s->hit); }

static uint32_t ev_len(const track_t *t) { return (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP); }

/* the rows of the selected track (rw[EV_ROWS]): their count, PLAY, QUANTIZE and + ADD included */
static uint32_t ev_list(uint16_t *rw)
{
    const track_t *t = TSEL;
    uint8_t idx[MOTION_MAX];
    uint32_t m = motion_rows(song.sel, idx), len = ev_len(t), n = 0, j = 0, s, k;
    static const uint8_t SK[4] = {EVK_CHANCE, EVK_RATCH, EVK_NUDGE, EVK_VEL};
    rw[n++] = EVC(EVK_TOP, 0);
    rw[n++] = EVC(EVK_QNT, 0);
    for (s = 0; s < NSTEP; s++) {
        for (k = 0; s < len && k < 4u; k++)
            if ((k == 3u ? ui.ev_vel && ev_vel_row(&t->step[s]) : !ev_sdef(&t->step[s], SK[k])) ||
                ui.ev_keep == EVC(SK[k], s))
                rw[n++] = EVC(SK[k], s);
        for (; j < m && (motion.event[idx[j]].place & 63u) == s; j++)
            rw[n++] = EVC(EVK_REC, idx[j]);
    }
    rw[n++] = EVC(EVK_ADD, 0);
    return n;
}
static uint32_t ev_row(uint32_t n) { return ui.ev_row < n ? ui.ev_row : n - 1u; }
/* the row selected: its code (*np: the count) */
static uint32_t ev_cur(uint16_t *rw, uint32_t *np)
{
    uint32_t n = ev_list(rw);
    if (np)
        *np = n;
    return rw[ev_row(n)];
}




static int ev_act_ready(void)
{
    uint16_t rw[EV_ROWS];
    uint32_t c = ev_cur(rw, 0), k = c >> 8;
    if (chain_busy())
        return 0;
    if (k == EVK_TOP)
        return motion_count(TSEL) != 0u;
    if (k == EVK_ADD)
        return ev_sid(ui.ev_id) || (motion.count < MOTION_MAX && ev_id_ok(TSEL, ui.ev_id));
    return k == EVK_REC;
}





/* SEQ > DETAIL (1.5.1, #199; ui_input.c sd_turn, ui_draw.c, ui_graph.c graph_detail): KNOB 1..4 are these fields of
 * the cursor step, the ones the rows above list (one storage). A step takes them where they play: a NOTE step (notes or
 * hits); CHANCE a TIE too (a failed roll cuts the held note). A REST or an empty step none (nothing to hear) */
static const uint8_t SD_KIND[4] = {EVK_CHANCE, EVK_RATCH, EVK_NUDGE, EVK_VEL};

