/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's sequencer (alter_seq.c) driving parts 1-3 on Felucca's transport: circle i plays part i (at power-on
 * ALTER, PHYS, NOISE), PLAY starts it, the tempo and an external MIDI clock are Felucca's. Included at the top of
 * seq.c, called once per block from its audio-ISR loop (alter_tick, after click_tick). Felucca's own patterns are
 * empty at power-on: the circles are what plays.
 *
 * Each sixteenth, for each circle: the step's note on its part (its last one released first: a hit is one step
 * long, the ADSR shapes it), its velocity (force: ghost 40, normal 96, accent 124, + the die's spread), a glide when
 * the die drew one (GLIDE set for that note alone), and the die's four timbre offsets on the parameters of the
 * part's engine (ALT_TIMB). The offsets are undone exactly before the next ones (alt_applied): what the knobs set
 * stays the sound's base.
 *
 * The PLAY view (alter_ui.c, the main loop) talks to this side only through the variables marked UI below: counters
 * it alone writes (this side follows them), or bytes; this side owns alt_seq. The fills (alt_hold, the keys held)
 * act on ONE circle, the one selected when the first fill key went down (alt_tgt): ROLL 1/16 1/32 triplet (its root,
 * ghost rising to accent, in place of its steps), RATCHET x2 x3 (each of its hits repeated within the step),
 * DUB DELAY / DUB REVERB / DESTROY (its part's send, or DIST + the FOLD insert, up while held, back on release:
 * the tail rings on), DENSITY - + (its HITS -2 +2), REVERSE (it walks back; on release it is where it would have
 * been). SHIFT -1 +1 (a tap): the selected circle turned a step, kept. */

#include "alter_seq.c"

enum { AF_ROLL16, AF_ROLL32, AF_ROLLT, AF_RAT2, AF_RAT3, AF_DLY, AF_REV, AF_DESTROY, AF_DENSM, AF_DENSP, AF_REVERSE,
       AF_N };
#define AF_ROLLS ((1u << AF_ROLL16) | (1u << AF_ROLL32) | (1u << AF_ROLLT))
#define ALT_DENS 2

static as_seq_t alt_seq;
static uint32_t alt_pos;                         /* samples into the sixteenth */
static uint8_t alt_run, alt_ready;               /* transport was running; alt_seq set up */
static uint8_t alt_note[AS_NCIRC];               /* each part's sounding note (0 = none) */
static int16_t alt_applied[AS_NCIRC][8];         /* the die's offsets in each part's E1..E8 now */

/* UI -> here */
static volatile uint16_t alt_hold;               /* the fills held, bit AF_* */
static volatile uint8_t alt_sel;                 /* the selected circle */
static volatile int32_t alt_knob_tot[AS_NCIRC][4];   /* the knobs' sum of moves, per circle (seen: alt_knob_seen) */
static volatile int32_t alt_shift_tot[AS_NCIRC];     /* SHIFT taps, +-1 each */
static volatile uint32_t alt_roll_cnt[AS_NCIRC], alt_clear_cnt[AS_NCIRC];   /* DICE taps, DICE held */
static volatile uint32_t alt_preset_cnt[AS_NCIRC];   /* a preset loaded on a part (the offsets forgotten) */
static as_cfg_t alt_post_cfg[AS_NCIRC];          /* a circle's new settings .. */
static volatile uint8_t alt_post;                /* .. waiting, bit i (the UI clears it before writing alt_post_cfg[i]) */
static volatile uint8_t alt_kb_own;              /* the keys are the PLAY view's: they play no notes (seq.c keyboard_block) */
/* here -> UI */
static volatile uint32_t alt_hits[AS_NCIRC];     /* notes played (the screen's flashes) */
static volatile uint8_t alt_tgt;                 /* the circle the fills act on */

static int32_t alt_knob_seen[AS_NCIRC][4], alt_shift_seen[AS_NCIRC];
static uint32_t alt_roll_seen[AS_NCIRC], alt_clear_seen[AS_NCIRC], alt_preset_seen[AS_NCIRC];
static uint16_t alt_prev;                        /* alt_hold at the last block */
static uint16_t alt_prev_dens;                   /* the DENSITY key applied */
static uint8_t alt_dens_cfg;                     /* DENSITY: the target's playing HITS before it .. */
static int8_t alt_dens_next;                     /* .. and what it added to the waiting one */
static uint8_t alt_rev_pos;                      /* REVERSE: where the target was at the press .. */
static uint16_t alt_rev_n;                       /* .. and the steps since */
static int16_t alt_fx_save[6];                   /* DLY, REV, DIST, ITYPE, IA, IMIX before the dub keys */
static int32_t alt_roll_pos;                     /* ROLL: samples to the next roll note (counting up to its period) */
static uint32_t alt_roll_n;                      /* roll notes so far (the rise) */
static uint8_t alt_rat_left, alt_rat_note, alt_rat_vel;
static uint32_t alt_rat_at, alt_rat_dt;          /* RATCHET: the next repeat (samples into the step), the spacing */

/* the four parameters the die's timbre offsets move, by the part's engine */
static const uint8_t ALT_TIMB_ALTER[AS_NTIMB] = {P_E0, P_E1, P_E2, P_E4};   /* MRPH SPRD HARM AMNT */
static const uint8_t ALT_TIMB_PHYS[AS_NTIMB] = {P_E1, P_E2, P_E3, P_E4};    /* STRC BRIT DAMP POS */
static const uint8_t ALT_TIMB_NOISE[AS_NTIMB] = {P_E1, P_E2, P_E3, P_E5};   /* COLR FREQ RES DENS */
static const uint8_t *alt_timb_p(const track_t *t)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    return e == &ENG_ALTER ? ALT_TIMB_ALTER : e == &ENG_PHYS ? ALT_TIMB_PHYS : e == &ENG_NOISE ? ALT_TIMB_NOISE : 0;
}

static void alt_off(uint32_t c)
{
    if (alt_note[c]) {
        trk_note_off(&trk[c], alt_note[c]);
        alt_note[c] = 0;
    }
}

static void alt_on(uint32_t c, uint32_t note, uint32_t vel, int glide)
{
    track_t *t = &trk[c];
    int16_t g = t->p[P_GLIDE];
    alt_off(c);
    t->p[P_GLIDE] = (int16_t)(glide ? (g ? g : 40) : 0);
    trk_note_on(t, note, vel);
    t->p[P_GLIDE] = g;
    alt_note[c] = (uint8_t)note;
    alt_hits[c]++;
}

/* a parameter of part c moved by d (a knob), its die offset kept on top */
static void alt_move(uint32_t c, uint32_t p, int32_t d)
{
    track_t *t = &trk[c];
    const param_desc_t *ds = param_desc_of(eng_idx(t->eng_req), p);
    int32_t lo = ds->min, hi = ds->max;
    if (p >= P_E0 && p < P_E0 + 8u) {
        int16_t *ap = &alt_applied[c][p - P_E0];
        int32_t base = clamp(t->p[p] - *ap + d, lo, hi), v = clamp(base + *ap, lo, hi);
        t->p[p] = (int16_t)v;
        *ap = (int16_t)(v - base);
    } else
        t->p[p] = (int16_t)clamp(t->p[p] + d, lo, hi);
}

static uint32_t alt_roll_period(uint32_t h, uint32_t p);
static void alt_repat(as_circ_t *c) { c->pat = as_euclid(c->cfg.len, c->cfg.hits, c->cfg.rot); }

/* DENSITY on the target: on (dir +-1) or off (0) */
static void alt_density(int32_t dir)
{
    as_circ_t *c = &alt_seq.c[alt_tgt];
    if (dir) {
        int32_t n0 = c->next.hits, n1 = clamp(n0 + dir * ALT_DENS, 0, c->next.len);
        alt_dens_cfg = c->cfg.hits;
        c->cfg.hits = (uint8_t)clamp((int32_t)c->cfg.hits + dir * ALT_DENS, 0, c->cfg.len);
        alt_dens_next = (int8_t)(n1 - n0);
        c->next.hits = (uint8_t)n1;
    } else {
        c->cfg.hits = (uint8_t)(alt_dens_cfg > c->cfg.len ? c->cfg.len : alt_dens_cfg);
        c->next.hits = (uint8_t)clamp((int32_t)c->next.hits - alt_dens_next, 0, c->next.len);
        alt_dens_next = 0;
    }
    alt_repat(c);
}

/* a dub send of the target: up while held (v), back on release */
static void alt_send(uint32_t e, uint32_t h, uint32_t bit, uint32_t p, int16_t *save, int16_t v)
{
    track_t *t = &trk[alt_tgt];
    if (!((e >> bit) & 1u))
        return;
    if ((h >> bit) & 1u) {
        *save = t->p[p];
        t->p[p] = v;
    } else
        t->p[p] = *save;
}

/* what the UI asked for, once a block */
static void alt_take(void)
{
    uint32_t c, j, h = alt_hold, e = h ^ alt_prev;
    uint8_t post = alt_post;
    for (c = 0; c < AS_NCIRC; c++) {
        const track_t *t = &trk[c];
        const engine_t *en = ENGINES[eng_idx(t->eng_req)];
        uint32_t pr = alt_preset_cnt[c], n;
        int32_t sh;
        if (pr != alt_preset_seen[c]) {          /* a preset: its values are the base, nothing applied on them */
            alt_preset_seen[c] = pr;
            for (j = 0; j < 8u; j++)
                alt_applied[c][j] = 0;
        }
        for (j = 0; j < 4u; j++) {
            int32_t tot = alt_knob_tot[c][j], d = tot - alt_knob_seen[c][j], p = en->knob[j];
            alt_knob_seen[c][j] = tot;
            if (d)
                alt_move(c, (uint32_t)p, param_desc_of(eng_idx(t->eng_req), (uint32_t)p)->fmt == F_ENUM ? d : d * 2);
        }
        if ((sh = alt_shift_tot[c] - alt_shift_seen[c]) != 0) {
            alt_shift_seen[c] += sh;
            as_turn(&alt_seq, c, sh);
        }
        if ((n = alt_roll_cnt[c]) != alt_roll_seen[c]) {
            alt_roll_seen[c] = n;
            as_roll(&alt_seq, c);
        }
        if ((n = alt_clear_cnt[c]) != alt_clear_seen[c]) {
            alt_clear_seen[c] = n;
            as_clear(&alt_seq, c);
        }
        if ((post >> c) & 1u) {
            as_set(&alt_seq, c, alt_post_cfg[c]);
            if (c == alt_tgt && (((h >> AF_DENSM) | (h >> AF_DENSP)) & 1u)) {   /* (DENSITY held: on top) */
                int32_t dir = (h >> AF_DENSP) & 1u ? 1 : -1, n0 = alt_seq.c[c].next.hits;
                int32_t n1 = clamp(n0 + dir * ALT_DENS, 0, alt_seq.c[c].next.len);
                alt_seq.c[c].next.hits = (uint8_t)n1;
                alt_dens_next = (int8_t)(n1 - n0);
            }
            alt_post &= (uint8_t)~(1u << c);
        }
    }
    if (!e)
        return;
    if (!alt_prev)                               /* the first fill down: its circle is the target until all are up */
        alt_tgt = (uint8_t)(alt_sel % AS_NCIRC);
    alt_prev = (uint16_t)h;
    alt_send(e, h, AF_DLY, P_DLY, &alt_fx_save[0], 127);
    alt_send(e, h, AF_REV, P_REV, &alt_fx_save[1], 127);
    if ((e >> AF_DESTROY) & 1u) {
        alt_send(e, h, AF_DESTROY, P_DIST, &alt_fx_save[2], 100);
        alt_send(e, h, AF_DESTROY, P_ITYPE, &alt_fx_save[3], 3);   /* FOLD (params.c N_ITYPE) */
        alt_send(e, h, AF_DESTROY, P_IA, &alt_fx_save[4], 110);
        alt_send(e, h, AF_DESTROY, P_IMIX, &alt_fx_save[5], 127);
    }
    if (((e >> AF_DENSM) | (e >> AF_DENSP)) & 1u) {   /* (one at a time: the last pressed wins) */
        if (((alt_prev_dens >> AF_DENSM) | (alt_prev_dens >> AF_DENSP)) & 1u)
            alt_density(0);                      /* (only what was added is taken back) */
        alt_prev_dens = (uint16_t)(h & ((1u << AF_DENSM) | (1u << AF_DENSP)));
        if ((h >> AF_DENSP) & 1u)
            alt_density(1);
        else if ((h >> AF_DENSM) & 1u)
            alt_density(-1);
    }
    if ((e >> AF_REVERSE) & 1u) {
        as_circ_t *c2 = &alt_seq.c[alt_tgt];
        if ((h >> AF_REVERSE) & 1u) {
            alt_rev_pos = c2->pos;
            alt_rev_n = 0;
        } else
            c2->pos = (uint8_t)((alt_rev_pos + alt_rev_n) % c2->cfg.len);
    }
    if ((e & h & AF_ROLLS) != 0u) {              /* a roll pressed: from the next sixteenth on its grid */
        alt_roll_n = 0;
        alt_roll_pos = (int32_t)alt_pos - (int32_t)div_samples(2) + (int32_t)alt_roll_period(h, div_samples(2));
    }
    if (!h)
        alt_rat_left = 0;
}

static uint32_t alt_roll_period(uint32_t h, uint32_t p)
{
    if ((h >> AF_ROLL32) & 1u)
        return p / 2u;
    if ((h >> AF_ROLLT) & 1u)
        return p * 2u / 3u;
    return p;
}

static void alter_step(void)
{
    as_event_t ev[AS_NCIRC];
    uint32_t c, j, h = alt_hold;
    as_step(&alt_seq, ev);
    if ((h >> AF_REVERSE) & 1u) {                /* back two: the next step is the one before this one */
        as_circ_t *r = &alt_seq.c[alt_tgt];
        r->pos = (uint8_t)((r->pos + 2u * r->cfg.len - 2u) % r->cfg.len);
        alt_rev_n++;
    }
    for (c = 0; c < AS_NCIRC; c++) {
        track_t *t = &trk[c];
        const uint8_t *tp = alt_timb_p(t);
        int fill = h && c == alt_tgt;
        if (fill && (h & AF_ROLLS))              /* a roll plays instead (the circle goes on underneath) */
            continue;
        alt_off(c);
        if (tp)
            for (j = 0; j < AS_NTIMB; j++) {     /* back to the base, then this step's offset (none without a hit) */
                uint32_t p = tp[j];
                const param_desc_t *ds = param_desc_of(eng_idx(t->eng_req), p);
                int16_t *ap = &alt_applied[c][p - P_E0];
                int32_t base = t->p[p] - *ap, v = ev[c].on ? base + ev[c].timb[j] : base;
                v = clamp(v, ds->min, ds->max);
                t->p[p] = (int16_t)v;
                *ap = (int16_t)(v - base);
            }
        if (!ev[c].on)
            continue;
        alt_on(c, ev[c].note, ev[c].vel, ev[c].glide);
        if (fill) {
            alt_rat_left = (h >> AF_RAT3) & 1u ? 2u : (h >> AF_RAT2) & 1u ? 1u : 0u;
            if (alt_rat_left) {
                alt_rat_dt = div_samples(2) / (alt_rat_left + 1u);
                alt_rat_at = alt_rat_dt;
                alt_rat_note = ev[c].note;
                alt_rat_vel = ev[c].vel;
            }
        }
    }
}

static void alt_ratchets(uint32_t upto)
{
    while (alt_rat_left && alt_rat_at <= upto) {
        alt_rat_vel = (uint8_t)(alt_rat_vel * 85u / 100u + 1u);
        alt_on(alt_tgt, alt_rat_note, alt_rat_vel, 0);
        alt_rat_at += alt_rat_dt;
        alt_rat_left--;
    }
}

/* n samples on in this block (seq_n: Felucca's, so an external clock moves it too) */
static void alter_tick(uint32_t n)
{
    uint32_t p = div_samples(2), h, c;           /* a sixteenth */
    if (!alt_ready) {
        as_init(&alt_seq, 0xA17E5u);
        alt_ready = 1;
    }
    alt_take();
    h = alt_hold;
    if (!song.playing) {
        if (alt_run)
            for (c = 0; c < AS_NCIRC; c++)
                alt_off(c);
        alt_run = 0;
        alt_rat_left = 0;
        return;
    }
    if (!alt_run) {                              /* PLAY: every circle from its first step, at once */
        alt_run = 1;
        alt_pos = 0;
        for (c = 0; c < AS_NCIRC; c++)
            alt_seq.c[c].pos = 0;
        alter_step();
        return;
    }
    alt_pos += n;
    alt_ratchets(alt_pos < p ? alt_pos : p);
    while (alt_pos >= p) {
        alt_pos -= p;
        alt_rat_left = 0;
        alter_step();
        alt_ratchets(alt_pos);
    }
    if (h & AF_ROLLS) {
        uint32_t per = alt_roll_period(h, p);
        const as_circ_t *r = &alt_seq.c[alt_tgt];
        alt_roll_pos += (int32_t)n;
        while (alt_roll_pos >= (int32_t)per) {
            uint32_t v = 40u + alt_roll_n * 6u;
            alt_roll_pos -= (int32_t)per;
            alt_on(alt_tgt, as_note(&alt_seq, r->cfg.root, 0), v > 124u ? 124u : v, 0);
            alt_roll_n++;
        }
    }
}
