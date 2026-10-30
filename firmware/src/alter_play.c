/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's sequencer (alter_seq.c) driving part 1 (ALTER) on Felucca's transport: PLAY starts it, the tempo and an
 * external MIDI clock are Felucca's. Included at the top of seq.c, called once per block from its audio-ISR loop
 * (alter_tick, after click_tick). Felucca's own patterns are empty at power-on: ALTER's circles are what plays.
 *
 * Each sixteenth: the step's note on part 1 (the last one released first: a hit is one step long, the ADSR
 * shapes it), its velocity (HITS 96, GHOST 40, ACCENT ~124), a glide when the die drew one (the voice glides
 * only then: GLIDE is set for that note alone), and the die's timbre offsets on MRPH SPRD HARM AMNT. The offsets
 * are undone exactly before the next ones (alt_applied): what the knobs set stays the sound's base.
 *
 * The PLAY view (alter_ui.c, the main loop) talks to this side only through the variables marked UI below: words
 * it alone writes (counters this side follows, alt_hold), or bytes; this side owns alt_seq. The fills (alt_hold, the
 * keys held) act here, at the block: ROLL 1/16 1/32 triplet (the root, ghost rising to accent), RATCHET x2 x3 (each
 * hit repeated within its step), DUB DELAY / DUB REVERB / DESTROY (a send, or DIST + the FOLD insert, up while held,
 * back on release: the tail rings on), DENSITY - + (HITS +-2, GHOST +-1), REVERSE (the circles walk back) and
 * SHIFT -1 +1 (every circle a step early / late). On release each circle is where it would have been. */

#include "alter_seq.c"

#define ALT_PART 0u                              /* the part ALTER plays (part 1) */
enum { AF_ROLL16, AF_ROLL32, AF_ROLLT, AF_RAT2, AF_RAT3, AF_DLY, AF_REV, AF_DESTROY, AF_DENSM, AF_DENSP, AF_REVERSE,
       AF_SHIFTM, AF_SHIFTP, AF_N };
#define AF_ROLLS ((1u << AF_ROLL16) | (1u << AF_ROLL32) | (1u << AF_ROLLT))

static as_seq_t alt_seq;
static uint32_t alt_pos;                         /* samples into the sixteenth */
static uint8_t alt_run, alt_ready, alt_note;     /* transport was running; alt_seq set up; the sounding note (0 = none) */
static int16_t alt_applied[AS_NTIMB];            /* the offsets in the part's knobs now */
static const uint8_t ALT_TIMB_P[AS_NTIMB] = {P_E0, P_E1, P_E2, P_E4};   /* MRPH SPRD HARM AMNT */

/* UI -> here */
static volatile uint16_t alt_hold;               /* the fills held, bit AF_* */
static volatile int32_t alt_knob_tot[4];         /* MRPH SPRD HARM MODE: the knobs' sum of moves (this side: alt_knob_seen) */
static volatile uint32_t alt_preset_cnt;         /* a preset loaded (the knobs' base is the preset's: offsets forgotten) */
static as_cfg_t alt_post_cfg[AS_NCIRC];          /* a circle's new settings .. */
static volatile uint8_t alt_post;                /* .. waiting, bit i (the UI clears it before writing alt_post_cfg[i]) */
static volatile uint8_t alt_kb_own;              /* the keys are the PLAY view's: they play no notes (seq.c keyboard_block) */
/* here -> UI */
static volatile uint32_t alt_hits;               /* notes played (the screen's flash) */

static int32_t alt_knob_seen[4];
static uint32_t alt_preset_seen;
static uint16_t alt_prev;                        /* alt_hold at the last block */
static uint16_t alt_prev_dens;                   /* the DENSITY key applied */
static uint8_t alt_rev_pos[AS_NCIRC];            /* REVERSE: where each circle was at the press .. */
static uint16_t alt_rev_n;                       /* .. and the steps since */
static uint8_t alt_dens_cfg[AS_NCIRC];           /* DENSITY: the playing HITS count before it .. */
static int8_t alt_dens_next[AS_NCIRC];           /* .. and what it added to the waiting one */
static int16_t alt_fx_save[6];                   /* DLY, REV, DIST, ITYPE, IA, IMIX before the dub keys */
static int32_t alt_roll_pos;                     /* ROLL: samples to the next roll note (counting up to its period) */
static uint32_t alt_roll_n;                      /* roll notes so far (the rise) */
static uint8_t alt_rat_left, alt_rat_note, alt_rat_vel;
static uint32_t alt_rat_at, alt_rat_dt;          /* RATCHET: the next repeat (samples into the step), the spacing */

static void alt_off(track_t *t)
{
    if (alt_note) {
        trk_note_off(t, alt_note);
        alt_note = 0;
    }
}

static void alt_on(track_t *t, uint32_t note, uint32_t vel, int glide)
{
    int16_t g = t->p[P_GLIDE];
    alt_off(t);
    t->p[P_GLIDE] = (int16_t)(glide ? (g ? g : 40) : 0);
    trk_note_on(t, note, vel);
    t->p[P_GLIDE] = g;
    alt_note = (uint8_t)note;
    alt_hits++;
}

static uint32_t alt_roll_period(uint32_t h, uint32_t p);
static void alt_repat(as_circ_t *c) { c->pat = as_euclid(c->cfg.len, c->cfg.hits, c->cfg.rot); }

/* DENSITY: on (dir +-1) or off (0) */
static void alt_density(int32_t dir)
{
    static const int8_t D[AS_NCIRC] = {2, 1, 0};
    uint32_t i;
    for (i = 0; i < AS_NCIRC; i++) {
        as_circ_t *c = &alt_seq.c[i];
        if (dir) {
            int32_t n0 = c->next.hits, n1 = clamp(n0 + dir * D[i], 0, c->next.len);
            alt_dens_cfg[i] = c->cfg.hits;
            c->cfg.hits = (uint8_t)clamp((int32_t)c->cfg.hits + dir * D[i], 0, c->cfg.len);
            alt_dens_next[i] = (int8_t)(n1 - n0);
            c->next.hits = (uint8_t)n1;
        } else {
            c->cfg.hits = (uint8_t)(alt_dens_cfg[i] > c->cfg.len ? c->cfg.len : alt_dens_cfg[i]);
            c->next.hits = (uint8_t)clamp((int32_t)c->next.hits - alt_dens_next[i], 0, c->next.len);
            alt_dens_next[i] = 0;
        }
        alt_repat(c);
    }
}

static void alt_shift(int32_t d)                 /* every circle d steps on (+) or back (-) */
{
    uint32_t i;
    for (i = 0; i < AS_NCIRC; i++) {
        as_circ_t *c = &alt_seq.c[i];
        c->pos = (uint8_t)(((int32_t)c->pos + d + 4 * AS_MAXLEN) % c->cfg.len);
    }
}

/* what the UI asked for, once a block */
static void alt_take(track_t *t)
{
    uint32_t i, j, pr = alt_preset_cnt, h = alt_hold, e = h ^ alt_prev;
    uint8_t post = alt_post;
    if (pr != alt_preset_seen) {                 /* a preset: its values are the base, nothing applied on them */
        alt_preset_seen = pr;
        for (j = 0; j < AS_NTIMB; j++)
            alt_applied[j] = 0;
    }
    for (j = 0; j < 4u; j++) {
        int32_t tot = alt_knob_tot[j], d = tot - alt_knob_seen[j];
        alt_knob_seen[j] = tot;
        if (!d)
            continue;
        if (j == 3u)                             /* MODE */
            t->p[P_E3] = (int16_t)clamp(t->p[P_E3] + d, 0, 2);
        else {
            int32_t p = ALT_TIMB_P[j], base = clamp(t->p[p] - alt_applied[j] + d, 0, 127);
            int32_t v = clamp(base + alt_applied[j], 0, 127);
            t->p[p] = (int16_t)v;
            alt_applied[j] = (int16_t)(v - base);
        }
    }
    for (i = 0; i < AS_NCIRC; i++)
        if ((post >> i) & 1u) {
            as_set(&alt_seq, i, alt_post_cfg[i]);
            alt_seq.c[i].cfg.mute = alt_seq.c[i].next.mute;   /* (a mute is at once) */
            if ((h >> AF_DENSM) & 1u || (h >> AF_DENSP) & 1u) {
                static const int8_t D[AS_NCIRC] = {2, 1, 0};
                int32_t dir = (h >> AF_DENSP) & 1u ? 1 : -1, n0 = alt_seq.c[i].next.hits;
                int32_t n1 = clamp(n0 + dir * D[i], 0, alt_seq.c[i].next.len);
                alt_seq.c[i].next.hits = (uint8_t)n1;
                alt_dens_next[i] = (int8_t)(n1 - n0);
            }
            alt_post &= (uint8_t)~(1u << i);
        }
    if (!e)
        return;
    alt_prev = (uint16_t)h;
    if ((e >> AF_DLY) & 1u) {
        if ((h >> AF_DLY) & 1u) {
            alt_fx_save[0] = t->p[P_DLY];
            t->p[P_DLY] = 127;
        } else
            t->p[P_DLY] = alt_fx_save[0];
    }
    if ((e >> AF_REV) & 1u) {
        if ((h >> AF_REV) & 1u) {
            alt_fx_save[1] = t->p[P_REV];
            t->p[P_REV] = 127;
        } else
            t->p[P_REV] = alt_fx_save[1];
    }
    if ((e >> AF_DESTROY) & 1u) {
        if ((h >> AF_DESTROY) & 1u) {
            alt_fx_save[2] = t->p[P_DIST];
            alt_fx_save[3] = t->p[P_ITYPE];
            alt_fx_save[4] = t->p[P_IA];
            alt_fx_save[5] = t->p[P_IMIX];
            t->p[P_DIST] = 100;
            t->p[P_ITYPE] = 3;                   /* FOLD (params.c N_ITYPE) */
            t->p[P_IA] = 110;
            t->p[P_IMIX] = 127;
        } else {
            t->p[P_DIST] = alt_fx_save[2];
            t->p[P_ITYPE] = alt_fx_save[3];
            t->p[P_IA] = alt_fx_save[4];
            t->p[P_IMIX] = alt_fx_save[5];
        }
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
        if ((h >> AF_REVERSE) & 1u) {
            for (i = 0; i < AS_NCIRC; i++)
                alt_rev_pos[i] = alt_seq.c[i].pos;
            alt_rev_n = 0;
        } else
            for (i = 0; i < AS_NCIRC; i++)
                alt_seq.c[i].pos = (uint8_t)((alt_rev_pos[i] + alt_rev_n) % alt_seq.c[i].cfg.len);
    }
    if ((e >> AF_SHIFTP) & 1u)
        alt_shift((h >> AF_SHIFTP) & 1u ? 1 : -1);
    if ((e >> AF_SHIFTM) & 1u)
        alt_shift((h >> AF_SHIFTM) & 1u ? -1 : 1);
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

static void alter_step(track_t *t)
{
    as_event_t ev;
    uint32_t j, h = alt_hold, i;
    as_step(&alt_seq, &ev);
    if ((h >> AF_REVERSE) & 1u) {                /* back two: the next step is the one before this one */
        for (i = 0; i < AS_NCIRC; i++)
            alt_seq.c[i].pos = (uint8_t)((alt_seq.c[i].pos + 2u * alt_seq.c[i].cfg.len - 2u) % alt_seq.c[i].cfg.len);
        alt_rev_n++;
    }
    if (h & AF_ROLLS)                            /* a roll plays instead (the circles go on underneath) */
        return;
    alt_off(t);
    for (j = 0; j < AS_NTIMB; j++) {             /* back to the base, then this step's offset (none without a hit) */
        int32_t p = ALT_TIMB_P[j], base = t->p[p] - alt_applied[j], v = ev.on ? base + ev.timb[j] : base;
        v = clamp(v, 0, 127);
        t->p[p] = (int16_t)v;
        alt_applied[j] = (int16_t)(v - base);
    }
    if (ev.on) {
        alt_on(t, ev.note, ev.vel, ev.glide);
        alt_rat_left = (h >> AF_RAT3) & 1u ? 2u : (h >> AF_RAT2) & 1u ? 1u : 0u;
        if (alt_rat_left) {
            alt_rat_dt = div_samples(2) / (alt_rat_left + 1u);
            alt_rat_at = alt_rat_dt;
            alt_rat_note = ev.note;
            alt_rat_vel = ev.vel;
        }
    }
}

static void alt_ratchets(track_t *t, uint32_t upto)
{
    while (alt_rat_left && alt_rat_at <= upto) {
        alt_rat_vel = (uint8_t)(alt_rat_vel * 85u / 100u + 1u);
        alt_on(t, alt_rat_note, alt_rat_vel, 0);
        alt_rat_at += alt_rat_dt;
        alt_rat_left--;
    }
}

/* n samples on in this block (seq_n: Felucca's, so an external clock moves it too) */
static void alter_tick(uint32_t n)
{
    track_t *t = &trk[ALT_PART];
    uint32_t p = div_samples(2), h;              /* a sixteenth */
    if (!alt_ready) {
        as_init(&alt_seq, 0xA17E5u);
        alt_ready = 1;
    }
    alt_take(t);
    h = alt_hold;
    if (!song.playing) {
        if (alt_run)
            alt_off(t);
        alt_run = 0;
        alt_rat_left = 0;
        return;
    }
    if (!alt_run) {                              /* PLAY: every circle from its first step, at once */
        uint32_t i;
        alt_run = 1;
        alt_pos = 0;
        for (i = 0; i < AS_NCIRC; i++)
            alt_seq.c[i].pos = 0;
        alter_step(t);
        return;
    }
    alt_pos += n;
    alt_ratchets(t, alt_pos < p ? alt_pos : p);
    while (alt_pos >= p) {
        alt_pos -= p;
        alt_rat_left = 0;
        alter_step(t);
        alt_ratchets(t, alt_pos);
    }
    if (h & AF_ROLLS) {
        uint32_t per = alt_roll_period(h, p);
        alt_roll_pos += (int32_t)n;
        while (alt_roll_pos >= (int32_t)per) {
            uint32_t v = 40u + alt_roll_n * 6u;
            alt_roll_pos -= (int32_t)per;
            alt_on(t, as_note(&alt_seq, 0), v > 124u ? 124u : v, 0);
            alt_roll_n++;
        }
    }
}
