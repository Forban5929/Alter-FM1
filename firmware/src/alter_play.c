/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's sequencer (alter_seq.c) driving parts 1-3 on Felucca's transport: circle i plays part i (at power-on
 * ALTER, PHYS, NOISE), PLAY starts it, the tempo and an external MIDI clock are Felucca's. Included at the top of
 * seq.c, called once per block from its audio-ISR loop (alter_tick, after click_tick). Felucca's own patterns are
 * empty at power-on: the circles are what plays.
 *
 * Each sixteenth, for each circle with a hit: its note on its part (the last one released first: a new hit cuts
 * the one before), its velocity (force: ghost 40, normal 96, accent 124, + the die's spread), a glide when the die
 * drew one (GLIDE set for that note alone), the note's length (1 step, or the die's 2..4: it rings on until then
 * unless the next hit cuts it), the die's repeats in the step (a burst, or a flam ~25 ms after), the die's envelope
 * offset on DEC and REL and its four timbre offsets on the parameters of the part's engine (ALT_TIMB). The offsets
 * are undone exactly before the next ones (alt_applied): what the knobs set stays the sound's base.
 *
 * The PLAY view (alter_ui.c, the main loop) talks to this side only through the variables marked UI below: counters
 * it alone writes (this side follows them), or bytes; this side owns alt_seq. Everything the keys do acts on the
 * grid: at the next sixteenth (a tap) or from it (a fill). The fills (alt_hold, the keys held) act on ONE circle, the
 * one selected when the first fill key went down (alt_tgt): ROLL 1/16 1/32 triplet (its root, ghost rising to
 * accent, in place of its steps), RATCHET x2 x3 (each of its hits repeated within the step), DUB DELAY / DUB REVERB
 * / DESTROY (its part's send, or DIST + the FOLD insert, up while held, back on release: the tail rings on), REVERSE
 * (it walks back; on release it is where it would have been). Taps on the selected circle: SHIFT -1 +1 (turned a
 * step, kept), DENSITY - + (a hit less / more, kept). The knobs of every quick page reach a part's parameters
 * through one queue (alt_q: the UI writes, this side reads; the die's offsets kept on top). SWING (song.g[G_SWING],
 * 0..100) delays every other sixteenth by up to half a step. */

#include "alter_seq.c"

enum { AF_ROLL16, AF_ROLL32, AF_ROLLT, AF_RAT2, AF_RAT3, AF_DLY, AF_REV, AF_DESTROY, AF_REVERSE, AF_N };
#define AF_ROLLS ((1u << AF_ROLL16) | (1u << AF_ROLL32) | (1u << AF_ROLLT))
#define ALT_NSLOT 12                             /* the offsets: E1..E8, DEC, REL, ATK, SUS */
/* the envelope modes (ALGORITHM, per circle): FREE the sound's own ADSR; PERC, STAB, DRONE an ADSR of their own
 * (the sound's own nudges it a quarter of the way, the die's offset a third as far) and the notes' length */
enum { AM_FREE, AM_PERC, AM_STAB, AM_DRONE, AM_N };
static const char *const ALT_MODE_NAME[AM_N] = {"FREE", "PERC", "STAB", "DRONE"};
static const uint8_t ALT_MODE_ENV[AM_N][4] = {   /* ATK DEC SUS REL */
    {0, 0, 0, 0}, {0, 42, 0, 34}, {0, 62, 56, 46}, {78, 110, 127, 104},
};
#define ALT_FLAM_SAMPLES 1100u                   /* ~25 ms */

static as_seq_t alt_seq;
static uint32_t alt_pos;                         /* samples into the sixteenth */
static uint8_t alt_run, alt_ready;               /* transport was running; alt_seq set up */
static uint8_t alt_note[AS_NCIRC][3];            /* each part's sounding notes: the hit's and its chord's (0 = none) */
static uint8_t alt_gate_left[AS_NCIRC];          /* steps the note still rings */
static int16_t alt_applied[AS_NCIRC][ALT_NSLOT]; /* the die's offsets in each part now (alt_slot) */

/* UI -> here */
static volatile uint16_t alt_hold;               /* the fills held, bit AF_* */
static volatile uint8_t alt_sel;                 /* the selected circle */
static volatile uint8_t alt_emode[AS_NCIRC];     /* each circle's envelope mode (AM_*) */
static volatile uint8_t alt_post_now;            /* alt_post's circles to take at once, not from the next cycle (CHAOS) */
#define ALT_QN 32u
typedef struct { uint8_t c, p; int8_t d; } alt_qe_t;
static volatile alt_qe_t alt_q[ALT_QN];         /* a parameter moved: circle, P_*, by (the UI writes at alt_qw .. */
static volatile uint8_t alt_qw, alt_qr;          /* .. this side reads at alt_qr) */
static volatile int32_t alt_shift_tot[AS_NCIRC];     /* SHIFT taps, +-1 each */
static volatile int32_t alt_dens_tot[AS_NCIRC];      /* DENSITY taps, +-1 each */
static volatile uint32_t alt_roll_cnt[AS_NCIRC], alt_clear_cnt[AS_NCIRC];   /* DICE taps, DICE held */
static volatile uint32_t alt_preset_cnt[AS_NCIRC];   /* a preset loaded on a part (the offsets forgotten) */
static as_cfg_t alt_post_cfg[AS_NCIRC];          /* a circle's new settings .. */
static volatile uint8_t alt_post;                /* .. waiting, bit i (the UI clears it before writing alt_post_cfg[i]) */
static volatile uint8_t alt_kb_own;              /* the keys are the PLAY view's: they play no notes (seq.c keyboard_block) */
/* here -> UI */
static volatile uint32_t alt_hits[AS_NCIRC];     /* notes played (the screen's flashes) */
static volatile uint8_t alt_tgt;                 /* the circle the fills act on */

static int32_t alt_shift_seen[AS_NCIRC], alt_dens_seen[AS_NCIRC];
static uint8_t alt_par;                          /* the sixteenth just played was an odd one (SWING) */
static uint32_t alt_gstep;                       /* sixteenths played since PLAY (a scene lands on this grid) */
static uint32_t alt_roll_seen[AS_NCIRC], alt_clear_seen[AS_NCIRC], alt_preset_seen[AS_NCIRC];
static uint16_t alt_prev;                        /* alt_hold at the last block */
static uint8_t alt_rev_pos;                      /* REVERSE: where the target was at the press .. */
static uint16_t alt_rev_n;                       /* .. and the steps since */
static int16_t alt_fx_save[6];                   /* DLY, REV, DIST, ITYPE, IA, IMIX before the dub keys */
static int32_t alt_roll_pos;                     /* ROLL: samples to the next roll note (counting up to its period) */
static uint32_t alt_roll_n;                      /* roll notes so far (the rise) */
static uint8_t alt_rat_left[AS_NCIRC], alt_rat_note[AS_NCIRC], alt_rat_vel[AS_NCIRC], alt_rat_ch[AS_NCIRC][2];
static uint32_t alt_rat_at[AS_NCIRC], alt_rat_dt[AS_NCIRC];   /* repeats: the next (samples into the step), the spacing */

/* the four parameters the die's timbre offsets move, by the part's engine */
static const uint8_t ALT_TIMB_ALTER[AS_NTIMB] = {P_E0, P_E1, P_E2, P_E4};   /* MRPH SPRD HARM AMNT */
static const uint8_t ALT_TIMB_PHYS[AS_NTIMB] = {P_E1, P_E2, P_E3, P_E4};    /* STRC BRIT DAMP POS */
static const uint8_t ALT_TIMB_NOISE[AS_NTIMB] = {P_E1, P_E2, P_E3, P_E5};   /* COLR FREQ RES DENS */
static const uint8_t *alt_timb_p(const track_t *t)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    return e == &ENG_ALTER ? ALT_TIMB_ALTER : e == &ENG_PHYS ? ALT_TIMB_PHYS : e == &ENG_NOISE ? ALT_TIMB_NOISE : 0;
}

/* a parameter's slot in alt_applied, -1 = none */
static int32_t alt_slot(uint32_t p)
{
    if (p >= P_E0 && p < P_E0 + 8u)
        return (int32_t)(p - P_E0);
    return p == P_DEC ? 8 : p == P_REL ? 9 : p == P_ATK ? 10 : p == P_SUS ? 11 : -1;
}

/* parameter p of part c at base + o (the die's offset o; o = 0: back to the base) */
static void alt_offset(uint32_t c, uint32_t p, int32_t o)
{
    track_t *t = &trk[c];
    const param_desc_t *ds = param_desc_of(eng_idx(t->eng_req), p);
    int16_t *ap = &alt_applied[c][alt_slot(p)];
    int32_t base = t->p[p] - *ap, v = clamp(base + o, ds->min, ds->max);
    t->p[p] = (int16_t)v;
    *ap = (int16_t)(v - base);
}

static void alt_off(uint32_t c)
{
    uint32_t k;
    for (k = 0; k < 3u; k++)
        if (alt_note[c][k]) {
            trk_note_off(&trk[c], alt_note[c][k]);
            alt_note[c][k] = 0;
        }
}

/* part c's note (and its chord's: ch[0], ch[1], 0 = none; a chord softer and never gliding) */
static void alt_on(uint32_t c, uint32_t note, uint32_t vel, int glide, const uint8_t *ch)
{
    track_t *t = &trk[c];
    int16_t g = t->p[P_GLIDE], vm = t->p[P_VOICE];
    uint32_t k, n = 1;
    alt_off(c);
    if (ch && (ch[0] || ch[1]))
        t->p[P_VOICE] = V_POLY;                  /* (a mono preset's LEGATO would play one note) */
    t->p[P_GLIDE] = (int16_t)(glide && !(ch && ch[0]) ? (g ? g : 40) : 0);
    trk_note_on(t, note, vel);
    alt_note[c][0] = (uint8_t)note;
    for (k = 0; ch && k < 2u; k++)
        if (ch[k] && ch[k] != note) {
            trk_note_on(t, ch[k], vel * 7u / 8u);
            alt_note[c][n++] = ch[k];
        }
    t->p[P_GLIDE] = g;
    t->p[P_VOICE] = vm;
    alt_hits[c]++;
}

/* a parameter of part c moved by d (a knob), its die offset kept on top */
static void alt_move(uint32_t c, uint32_t p, int32_t d)
{
    track_t *t = &trk[c];
    const param_desc_t *ds = param_desc_of(eng_idx(t->eng_req), p);
    int32_t lo = ds->min, hi = ds->max, sl = alt_slot(p);
    if (sl >= 0) {
        int16_t *ap = &alt_applied[c][sl];
        int32_t base = clamp(t->p[p] - *ap + d, lo, hi), v = clamp(base + *ap, lo, hi);
        t->p[p] = (int16_t)v;
        *ap = (int16_t)(v - base);
    } else
        t->p[p] = (int16_t)clamp(t->p[p] + d, lo, hi);
}

static uint32_t alt_roll_period(uint32_t h, uint32_t p);

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
    while (alt_qr != alt_qw) {                   /* the knobs */
        alt_qe_t q = alt_q[alt_qr];
        track_t *t = &trk[q.c % AS_NCIRC];
        if (q.p == P_IA && !t->p[P_ITYPE]) {     /* FX's INSERT from OFF: FOLD, full wet */
            t->p[P_ITYPE] = 3;                   /* (params.c N_ITYPE) */
            t->p[P_IMIX] = 127;
        }
        alt_move(q.c % AS_NCIRC, q.p, q.d);
        alt_qr = (uint8_t)((alt_qr + 1u) % ALT_QN);
    }
    for (c = 0; c < AS_NCIRC; c++) {
        uint32_t pr = alt_preset_cnt[c], n;
        int32_t sh;
        if (pr != alt_preset_seen[c]) {          /* a preset: its values are the base, nothing applied on them */
            alt_preset_seen[c] = pr;
            for (j = 0; j < ALT_NSLOT; j++)
                alt_applied[c][j] = 0;
        }
        if ((sh = alt_shift_tot[c] - alt_shift_seen[c]) != 0) {
            alt_shift_seen[c] += sh;
            as_turn(&alt_seq, c, sh);
        }
        if ((sh = alt_dens_tot[c] - alt_dens_seen[c]) != 0) {
            alt_dens_seen[c] += sh;
            as_density(&alt_seq, c, sh);
        }
        if ((post >> c) & 1u) {
            as_set(&alt_seq, c, alt_post_cfg[c]);
            if ((alt_post_now >> c) & 1u) {      /* at once: from the next step */
                as_circ_t *cc = &alt_seq.c[c];
                cc->cfg = cc->next;
                cc->pos = (uint8_t)(alt_gstep % cc->cfg.len);   /* where it would be had it played so from PLAY */
                as_repat(cc);
                alt_post_now &= (uint8_t)~(1u << c);
            }
            alt_post &= (uint8_t)~(1u << c);
        }
        if ((n = alt_roll_cnt[c]) != alt_roll_seen[c]) {
            alt_roll_seen[c] = n;
            as_roll(&alt_seq, c);
        }
        if ((n = alt_clear_cnt[c]) != alt_clear_seen[c]) {
            alt_clear_seen[c] = n;
            as_clear(&alt_seq, c);
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
    if ((e >> AF_REVERSE) & 1u) {
        as_circ_t *c2 = &alt_seq.c[alt_tgt];
        uint32_t len = c2->cfg.len;
        if ((h >> AF_REVERSE) & 1u) {            /* the next step is the one before the one sounding */
            alt_rev_pos = c2->pos;
            alt_rev_n = 0;
            c2->pos = (uint8_t)((c2->pos + 2u * len - 2u) % len);
        } else                                   /* where it would have been, from the next step */
            c2->pos = (uint8_t)((alt_rev_pos + alt_rev_n) % len);
    }
    if ((e & h & AF_ROLLS) != 0u) {              /* a roll pressed: from the next sixteenth on its grid */
        alt_roll_n = 0;
        alt_roll_pos = (int32_t)alt_pos - (int32_t)div_samples(2) + (int32_t)alt_roll_period(h, div_samples(2));
    }
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
    alt_gstep++;
    if ((h >> AF_REVERSE) & 1u) {                /* back two: the next step is the one before this one */
        as_circ_t *r = &alt_seq.c[alt_tgt];
        r->pos = (uint8_t)((r->pos + 2u * r->cfg.len - 2u) % r->cfg.len);
        alt_rev_n++;
    }
    for (c = 0; c < AS_NCIRC; c++) {
        track_t *t = &trk[c];
        const uint8_t *tp = alt_timb_p(t);
        const as_event_t *v = &ev[c];
        int fill = h && c == alt_tgt;
        uint32_t rep;
        alt_rat_left[c] = 0;
        if (fill && (h & AF_ROLLS))              /* a roll plays instead (the circle goes on underneath) */
            continue;
        if (!v->on) {                            /* no hit: the last note rings on its length, then stops */
            if (alt_gate_left[c])
                alt_gate_left[c]--;
            else
                alt_off(c);
            continue;
        }
        if (tp)                                  /* back to the base, then this hit's offsets */
            for (j = 0; j < AS_NTIMB; j++)
                alt_offset(c, tp[j], v->timb[j]);
        {
            uint32_t m = alt_emode[c] % AM_N, gate = v->gate;
            if (m == AM_FREE) {
                alt_offset(c, P_ATK, 0);
                alt_offset(c, P_SUS, 0);
                alt_offset(c, P_DEC, v->env);
                alt_offset(c, P_REL, v->env);
            } else {                             /* the mode's ADSR, nudged by the sound's own and the die */
                static const uint8_t EP[4] = {P_ATK, P_DEC, P_SUS, P_REL};
                for (j = 0; j < 4u; j++) {
                    int32_t base = t->p[EP[j]] - alt_applied[c][alt_slot(EP[j])], tg = ALT_MODE_ENV[m][j];
                    tg += (base - tg) / 4;
                    if (j == 1u || j == 3u)
                        tg += v->env / 3;
                    alt_offset(c, EP[j], tg - base);
                }
                gate = m == AM_PERC ? 1u : m == AM_STAB ? (gate > 2u ? 2u : gate) : 255u;   /* DRONE: until the next hit */
            }
            alt_on(c, v->note, v->vel, v->glide, v->chord);
            alt_gate_left[c] = (uint8_t)(gate > 1u ? gate - 1u : 0u);
        }
        rep = v->rat;                            /* repeats: the die's burst, or a held RATCHET, the more */
        if (fill) {
            uint32_t f = (h >> AF_RAT3) & 1u ? 2u : (h >> AF_RAT2) & 1u ? 1u : 0u;
            rep = f > rep ? f : rep;
        }
        alt_rat_note[c] = v->note;
        alt_rat_ch[c][0] = v->chord[0];
        alt_rat_ch[c][1] = v->chord[1];
        alt_rat_vel[c] = v->vel;
        if (rep) {
            alt_rat_left[c] = (uint8_t)rep;
            alt_rat_dt[c] = div_samples(2) / (rep + 1u);
        } else if (v->flam) {
            alt_rat_left[c] = 1;
            alt_rat_dt[c] = ALT_FLAM_SAMPLES;
        }
        alt_rat_at[c] = alt_rat_dt[c];
    }
}

static void alt_ratchets(uint32_t upto)
{
    uint32_t c;
    for (c = 0; c < AS_NCIRC; c++)
        while (alt_rat_left[c] && alt_rat_at[c] <= upto) {
            alt_rat_vel[c] = (uint8_t)(alt_rat_vel[c] * 80u / 100u + 1u);
            alt_on(c, alt_rat_note[c], alt_rat_vel[c], 0, alt_rat_ch[c]);
            alt_rat_at[c] += alt_rat_dt[c];
            alt_rat_left[c]--;
        }
}

/* the sixteenth playing's length (p: a straight one): SWING puts the odd ones late by up to half a step */
static uint32_t alt_len(uint32_t p)
{
    uint32_t sw = (uint32_t)clamp(song.g[G_SWING], 0, 100) * p / 200u;
    return alt_par ? p - sw : p + sw;
}

/* n samples on in this block (seq_n: Felucca's, so an external clock moves it too) */
static void alter_tick(uint32_t n)
{
    uint32_t p = div_samples(2), h, c, len;      /* a sixteenth */
    if (!alt_ready) {
        as_init(&alt_seq, 0xA17E5u);
        alt_ready = 1;
    }
    alt_take();
    h = alt_hold;
    if (!song.playing) {
        if (alt_run)
            for (c = 0; c < AS_NCIRC; c++) {
                alt_off(c);
                alt_rat_left[c] = 0;
                alt_gate_left[c] = 0;
            }
        alt_run = 0;
        return;
    }
    if (!alt_run) {                              /* PLAY: every circle from its first step, at once */
        alt_run = 1;
        alt_pos = 0;
        alt_par = 0;
        alt_gstep = 0;
        for (c = 0; c < AS_NCIRC; c++)
            alt_seq.c[c].pos = 0;
        alter_step();
        return;
    }
    /* this sixteenth's length: SWING makes the even ones longer and the odd ones shorter by as much */
    len = alt_len(p);
    alt_pos += n;
    alt_ratchets(alt_pos < len ? alt_pos : len);
    while (alt_pos >= len) {
        alt_pos -= len;
        alt_par ^= 1u;
        alter_step();
        alt_ratchets(alt_pos);
        len = alt_len(p);
    }
    if (h & AF_ROLLS) {
        uint32_t per = alt_roll_period(h, p);
        const as_circ_t *r = &alt_seq.c[alt_tgt];
        alt_roll_pos += (int32_t)n;
        while (alt_roll_pos >= (int32_t)per) {
            uint32_t v = 40u + alt_roll_n * 6u;
            alt_roll_pos -= (int32_t)per;
            alt_on(alt_tgt, as_note(&alt_seq, r->cfg.root, 0), v > 124u ? 124u : v, 0, 0);
            alt_roll_n++;
        }
    }
}
