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
 * one selected when the first fill key went down (alt_tgt): ROLL 1/16 1/32 (its root, ghost rising to
 * accent, in place of its steps), RATCHET x2 x3 (no key now) (each of its hits repeated within the step), DUB DELAY / DUB REVERB
 * / DESTROY (its part's send, or DIST + the FOLD insert, up while held, back on release: the tail rings on), REVERSE
 * (it walks back; on release it is where it would have been). Taps on the selected circle: SHIFT -1 +1 (turned a
 * step, kept), DENSITY - + (a hit less / more, kept). The knobs of every quick page reach a part's parameters
 * through one queue (alt_q: the UI writes, this side reads; the die's offsets kept on top). SWING (song.g[G_SWING],
 * 0..100) delays every other sixteenth by up to half a step. */

#include "alter_seq.c"

enum { AF_ROLL32, AF_ROLL16, AF_BUILD, AF_RAT2, AF_RAT3, AF_DLY, AF_REV, AF_DESTROY, AF_REVERSE, AF_N };
#define AF_ROLLS ((1u << AF_ROLL32) | (1u << AF_ROLL16) | (1u << AF_BUILD))
#define AF_DUBS ((1u << AF_DLY) | (1u << AF_REV) | (1u << AF_DESTROY))
/* REC'd fills (03/11): per circle and step, what plays there on every cycle: ROLL 1/32 (the step's hit doubled), the dubs
 * (DUB DELAY / REVERB, DESTROY for that step). ROLL 1/16 is recorded as added steps (alter_seq.c as_add). The UI writes
 * them only to clear or load them (IRQ off) */
enum { AFR_R32 = 1, AFR_DLY = 2, AFR_REV = 4, AFR_DST = 8 };
static uint8_t alt_fxr[AS_NCIRC][AS_MAXLEN];
#define ALT_NSLOT 14                             /* the offsets: E1..E8, DEC, REL, ATK, SUS, the DELAY and REVERB lanes */
/* the envelope modes (ALGORITHM, per circle): FREE the sound's own ADSR; PERC, STAB, DRONE an ADSR of their own
 * (the sound's own nudges it a quarter of the way, the die's offset a third as far) and the notes' length */
enum { AM_FREE, AM_PERC, AM_STAB, AM_DRONE, AM_N };
static const char *const ALT_MODE_NAME[AM_N] = {"FREE", "PERC", "STAB", "DRONE"};
_Static_assert(AM_PERC == 1 && AM_STAB == 2 && AM_DRONE == 3, "alter_lfo.c alt_env_mod reads the modes as 1 2 3");
#define ALT_FLAM_SAMPLES 1100u                   /* ~25 ms */

static as_seq_t alt_seq;
static uint32_t alt_pos;                         /* samples into the sixteenth */
static uint8_t alt_run, alt_ready;               /* transport was running; alt_seq set up */
static uint8_t alt_note[AS_NCIRC][3];            /* each part's sounding notes: the hit's and its chord's (0 = none) */
static uint8_t alt_gate_left[AS_NCIRC];          /* steps the note still rings */
static int16_t alt_applied[AS_NCIRC][ALT_NSLOT]; /* the die's offsets in each part now (alt_slot) */
static int32_t alt_hold_smp[AS_NCIRC];           /* PERC: samples the hit is held before its decay (0 = none) */

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
static int16_t alt_fx_save[AS_NCIRC][6];         /* per circle: DLY, REV, DIST, ITYPE, IA, IMIX before a dub */
static uint16_t alt_dub_on[AS_NCIRC];            /* the dubs applied on each circle now (held or REC'd), bit AF_* */
static uint8_t alt_fxr_now[AS_NCIRC];            /* the REC'd fills of the step each circle plays */
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
    return p == P_DEC ? 8 : p == P_REL ? 9 : p == P_ATK ? 10 : p == P_SUS ? 11 : p == P_DLY ? 12 : p == P_REV ? 13 : -1;
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

static uint8_t alt_lay[AS_NCIRC][2];             /* DRONE: the layers sounding (bit i: part voice i), oldest first .. */
static uint8_t alt_nlay[AS_NCIRC];               /* .. and how many */

/* part c's voices in mask m let go (their release) */
static void alt_release(uint32_t c, uint32_t m)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (((m >> i) & 1u) && trk[c].v[i].active && trk[c].v[i].gate) {
            trk[c].v[i].gate = 0;
            trk[c].v[i].stage = 3;
        }
}

static void alt_off(uint32_t c)
{
    uint32_t k;
    for (k = 0; k < 3u; k++)
        if (alt_note[c][k]) {
            trk_note_off(&trk[c], alt_note[c][k]);
            alt_note[c][k] = 0;
        }
    if (alt_nlay[c])                             /* (a drone's layers, whatever their notes) */
        alt_release(c, alt_lay[c][0] | alt_lay[c][1]);
    alt_nlay[c] = 0;
}

/* the LFOs' offset now on circle c's envelope setting g (GD_ELEN .. GD_ESUS), in 0..127 units (alter_lfo.c) */
static int32_t alt_env_lfo(uint32_t c, uint32_t g)
{
    uint32_t j;
    int32_t o = 0;
    if (alt_gl_has(g))
        o += (((alt_gl_val * alt_gl.amt[0]) >> 6) * 127) >> 15;
    for (j = 0; j < CL_N; j++)
        if (alt_cl_on(c, j) == g)
            o += (((alt_cl_val[c][j] * alt_cl[c][j].depth) >> 6) * 127) >> 15;
    return o;
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
    am_hit(c);                                   /* (the MASTER's duck: alter_fx.c) */
    if (alt_emode[c] == AM_PERC) {               /* PERC: held HOLD (0 .. 500 ms), then let go (its DECAY), PUNCH */
        uint32_t h = alt_envp[c][AE_PHOLD] + (uint32_t)clamp(alt_env_lfo(c, GD_EDEC) + alt_env_lfo(c, GD_ELEN), -127, 127);
        h = h > 127u ? (h > 1000u ? 0u : 127u) : h;
        alt_hold_smp[c] = (int32_t)(h * h * (FS / 2u) / 16129u) + 1;
        alt_punch_lvl[c] = 32767;
    }
}

/* DRONE: part c's hit as a new layer over the one before (its SWELL: no restrike of a voice that sounds). Two layers
 * at most: the oldest lets go (its TAIL) as the third comes; and the one before too when both would not fit the
 * engine's voices (a chord: they crossfade) */
static uint32_t alt_popc(uint32_t m)
{
    uint32_t n = 0;
    for (; m; m &= m - 1u)
        n++;
    return n;
}
static void alt_on_drone(uint32_t c, uint32_t note, uint32_t vel, const uint8_t *ch)
{
    track_t *t = &trk[c];
    uint8_t nn[3] = {(uint8_t)note, 0, 0};
    uint32_t k, n = 1, i, mask = 0, poly = trk_nvoice(t);
    int16_t vm = t->p[P_VOICE], g = t->p[P_GLIDE];
    for (k = 0; ch && k < 2u; k++)
        if (ch[k] && ch[k] != note)
            nn[n++] = ch[k];
    for (k = 0; k < 3u; k++)                     /* (the notes before: not let go by note, their layers are) */
        alt_note[c][k] = 0;
    if (alt_nlay[c] == 2u) {                     /* a third: the oldest fades out */
        alt_release(c, alt_lay[c][0]);
        alt_lay[c][0] = alt_lay[c][1];
        alt_nlay[c] = 1;
    }
    if (alt_nlay[c] == 1u && alt_popc(alt_lay[c][0]) + n > poly) {   /* no room for both: a crossfade */
        alt_release(c, alt_lay[c][0]);
        alt_nlay[c] = 0;
    }
    t->p[P_VOICE] = V_POLY;
    t->p[P_GLIDE] = 0;
    alt_no_reuse = 1;
    for (k = 0; k < n; k++) {
        uint32_t best = NVOICE;
        trk_note_on(t, nn[k], k ? vel * 7u / 8u : vel);
        for (i = 0; i < poly; i++)               /* the voice it took: the newest held on that note */
            if (t->v[i].gate && t->v[i].note == nn[k] && (best == NVOICE || t->v[i].age > t->v[best].age))
                best = i;
        if (best < NVOICE)
            mask |= 1u << best;
        alt_note[c][k] = nn[k];
    }
    alt_no_reuse = 0;
    t->p[P_VOICE] = vm;
    t->p[P_GLIDE] = g;
    for (k = 0; k < alt_nlay[c]; k++)            /* (a voice taken back from an older layer is the new one's now) */
        alt_lay[c][k] &= (uint8_t)~mask;
    alt_lay[c][alt_nlay[c]++] = (uint8_t)mask;
    alt_hits[c]++;
    am_hit(c);                                   /* (the MASTER's duck: alter_fx.c) */
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

#include "alter_rec.c"                          /* REC: the knob moves on the steps (after alt_move) */

static uint32_t alt_roll_period(uint32_t h, uint32_t p);

/* a dub send of circle c: up while on (v), back after */
static void alt_send(uint32_t c, uint32_t e, uint32_t h, uint32_t bit, uint32_t p, int16_t *save, int16_t v)
{
    track_t *t = &trk[c];
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
        ar_moved(q.c % AS_NCIRC, q.p, alt_run && song.playing);
        alt_qr = (uint8_t)((alt_qr + 1u) % ALT_QN);
    }
    for (c = 0; c < AS_NCIRC; c++) {
        uint32_t pr = alt_preset_cnt[c], n;
        int32_t sh;
        if (pr != alt_preset_seen[c]) {          /* a preset: its values are the base, nothing applied on them */
            alt_preset_seen[c] = pr;
            ar_clear(c);                         /* (REC's moves: of the sound before) */
            for (j = 0; j < ALT_NSLOT; j++)
                alt_applied[c][j] = 0;
        }
        if ((sh = alt_shift_tot[c] - alt_shift_seen[c]) != 0) {
            alt_shift_seen[c] += sh;
            if (alt_seq.c[c].cfg.type == AS_T_TURING)   /* TURING: SHIFT is NEXT / PREV */
                for (; sh; sh += sh > 0 ? -1 : 1)
                    (sh > 0 ? as_tnext : as_tprev)(&alt_seq, c);
            else
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
            ar_clear(c);                         /* (a draw: REC's moves gone, Romain 01/11) */
        }
        if ((n = alt_clear_cnt[c]) != alt_clear_seen[c]) {
            alt_clear_seen[c] = n;
            as_clear(&alt_seq, c);
        }
    }
    if (e && !alt_prev)                          /* the first fill down: its circle is the target until all are up */
        alt_tgt = (uint8_t)(alt_sel % AS_NCIRC);
    alt_prev = (uint16_t)h;
    for (c = 0; c < AS_NCIRC; c++) {             /* the dubs on each circle: held (the target) or REC'd on its step */
        uint32_t r = alt_run ? alt_fxr_now[c] : 0u, d, de;
        int16_t *sv = alt_fx_save[c];
        d = (c == alt_tgt ? h & AF_DUBS : 0u) | ((r & AFR_DLY) ? 1u << AF_DLY : 0u) | ((r & AFR_REV) ? 1u << AF_REV : 0u) |
            ((r & AFR_DST) ? 1u << AF_DESTROY : 0u);
        de = d ^ alt_dub_on[c];
        if (!de)
            continue;
        alt_dub_on[c] = (uint16_t)d;
        alt_send(c, de, d, AF_DLY, P_DLY, &sv[0], 127);
        alt_send(c, de, d, AF_REV, P_REV, &sv[1], 127);
        if ((de >> AF_DESTROY) & 1u) {
            alt_send(c, de, d, AF_DESTROY, P_DIST, &sv[2], 100);
            alt_send(c, de, d, AF_DESTROY, P_ITYPE, &sv[3], 3);   /* FOLD (params.c N_ITYPE) */
            alt_send(c, de, d, AF_DESTROY, P_IA, &sv[4], 110);
            alt_send(c, de, d, AF_DESTROY, P_IMIX, &sv[5], 127);
        }
    }
    if (!e)
        return;
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

/* a roll's step (p: a sixteenth): 1/32, 1/16, or BUILD UP: 4 eighths, 8 sixteenths, then thirty-seconds (each
 * stage two beats long, so it stays on the grid), louder and louder (alt_roll_n: the notes so far) */
static uint32_t alt_roll_period(uint32_t h, uint32_t p)
{
    if ((h >> AF_BUILD) & 1u)
        return alt_roll_n < 4u ? 2u * p : alt_roll_n < 12u ? p : p / 2u;
    if ((h >> AF_ROLL32) & 1u)
        return p / 2u;
    return p;
}

/* the LFOs (alter_lfo.c) after n samples more: the clock on the sixteenths' grid (p samples each), counted from
 * PLAY (stopped, it runs on at the tempo); each LFO's value now */
static uint32_t alt_gl_ph;                       /* the GLOBAL's phase (the screen) */
static void alt_gl_tick(uint32_t n, uint32_t p)
{
    uint32_t c, j;
    alt_gl_n += n;
    alt_lfo_p = p;
    alt_gl_val = alt_lfo_at(alt_gl.wave, alt_gl.rate, 9u, alt_gl_n, p, &alt_gl_ph);
    for (c = 0; c < AS_NCIRC; c++) {
        for (j = 0; j < CL_N; j++)
            alt_cl_val[c][j] = alt_lfo_at(alt_cl[c][j].wave, alt_cl[c][j].rate, c * 3u + j, alt_gl_n, p, 0);
        alt_ems[c] = alt_emode[c];
        alt_punch_lvl[c] -= alt_punch_lvl[c] >> 5;   /* PUNCH: ~23 ms */
        if (alt_hold_smp[c] > 0 && (alt_hold_smp[c] -= (int32_t)n) <= 0) {   /* PERC's HOLD over: let it go */
            alt_hold_smp[c] = 0;
            alt_off(c);
        }
    }
}

/* each circle's POS (alter_lfo.c): its step sounding and the time into it, over its length */
static void alt_pos_src(uint32_t p)
{
    uint32_t c, f = p ? ((alt_pos < p ? alt_pos : p - 1u) << 15) / p : 0u;
    for (c = 0; c < AS_NCIRC; c++) {
        const as_circ_t *cc = &alt_seq.c[c];
        uint32_t len = cc->cfg.len ? cc->cfg.len : 1u, now = (cc->pos + len - 1u) % len;
        alt_msrc[c][0] = (int16_t)(alt_run ? (now * 32768u + f) / len : 0u);
    }
}

/* circle c's ADSR for a hit in mode m (die: the die's envelope offset): FREE the sound's own (+ the LFOs on it); the
 * others from their own settings (alter_lfo.c alt_envp), in 0..127 units before the times they map to (P_ATK ..: ms
 * = 10000 ^ (v / 127): 0 = 1 ms, 64 = 100 ms, 96 = 1 s, 127 = 10 s) */
static int32_t alt_env_k(uint32_t c, uint32_t k, int32_t add)   /* a mode setting, the LFOs and the die on it */
{
    return clamp((int32_t)alt_envp[c][k] + add, 0, 127);
}
static int32_t alt_map(int32_t v, int32_t lo, int32_t hi) { return lo + v * (hi - lo) / 127; }
static void alt_env_apply(uint32_t c, uint32_t m, int32_t die)
{
    static const uint8_t EP[4] = {P_ATK, P_DEC, P_SUS, P_REL};
    track_t *t = &trk[c];
    int32_t tg[4], j, la = alt_env_lfo(c, GD_EATK), ld = alt_env_lfo(c, GD_EDEC), ls = alt_env_lfo(c, GD_ESUS),
            ll = alt_env_lfo(c, GD_ELEN);
    if (ENGINES[eng_idx(t->eng_req)] == &ENG_PHYS && m != AM_DRONE)
        alt_offset(c, P_E6, 0);                  /* (the drone's bow off again) */
    if (m == AM_FREE) {                          /* the sound's own ADSR, the die on DEC REL, the LFOs on all */
        tg[0] = la;
        tg[1] = die + ld + ll;
        tg[2] = ls;
        tg[3] = die + ls + ll;
        for (j = 0; j < 4; j++)
            alt_offset(c, EP[j], tg[j]);
        return;
    }
    if (m == AM_PERC) {                          /* ATTACK short, held full, DECAY on the release (alt_tick lets go) */
        tg[0] = alt_map(alt_env_k(c, AE_PATK, la), 0, 40);
        tg[1] = 0;
        tg[2] = 127;
        tg[3] = alt_map(alt_env_k(c, AE_PDEC, ls + ll + die / 2), 24, 100);
    } else if (m == AM_STAB) {                   /* short, LENGTH both decay and release (never long), SUSTAIN */
        int32_t len = alt_map(alt_env_k(c, AE_SLEN, ld + ll + die / 2), 48, 86);   /* (never too short) */
        tg[0] = alt_map(clamp(la, 0, 127), 2, 24);
        tg[1] = len;
        tg[2] = alt_map(alt_env_k(c, AE_SSUS, ls), 0, 110);
        tg[3] = len;
    } else {                                     /* DRONE: SWELL never short, the decay into BODY, TAIL never short */
        tg[0] = alt_map(alt_env_k(c, AE_DSWELL, la), 68, 112);
        tg[1] = 84;
        tg[2] = alt_map(alt_env_k(c, AE_DBODY, ld), 36, 127);
        tg[3] = alt_map(alt_env_k(c, AE_DTAIL, ls + ll + die / 2), 78, 120);
    }
    if (ENGINES[eng_idx(t->eng_req)] == &ENG_PHYS) {   /* DRONE: PHYS's MODAL / STRNG bowed, not struck (BOW, E7) */
        int32_t model = t->p[P_E0] - alt_applied[c][0], bow = t->p[P_E6] - alt_applied[c][6];
        alt_offset(c, P_E6, m == AM_DRONE && model <= 1 && bow < 60 ? 80 - bow : 0);
    }
    for (j = 0; j < 4; j++) {
        int32_t base = t->p[EP[j]] - alt_applied[c][alt_slot(EP[j])];
        alt_offset(c, EP[j], tg[j] - base);
    }
}

/* TRIGGER (alter_ui.c AK_TRIG): a hit of the chosen circle AT ONCE (a pad: within the block), whatever its sound, its
 * last note; with REC (held or on) and the transport running, the hit is added to the circle on the NEAREST of its
 * steps (alter_seq.c as_add: it plays on every cycle): the step just played when the tap came in its first half, else
 * the next one, whose own hit is then skipped once (it sounded already: no flam). Stopped: let go after a sixteenth. */
static volatile uint8_t alt_trig_tot[AS_NCIRC], alt_trig_add_tot[AS_NCIRC];   /* (the UI counts the taps) */
static uint8_t alt_trig_seen[AS_NCIRC], alt_trig_add_seen[AS_NCIRC];
static uint8_t alt_trig_skip[AS_NCIRC];          /* step + 1 whose hit is skipped once (0: none) */
static int32_t alt_trig_left[AS_NCIRC];          /* (stopped) samples before the hit lets go */
static uint32_t alt_len(uint32_t p);
static void alt_trig_now(uint32_t n)
{
    uint32_t c, p = div_samples(2);
    for (c = 0; c < AS_NCIRC; c++) {
        as_circ_t *sc = &alt_seq.c[c];
        int add = alt_trig_add_tot[c] != alt_trig_add_seen[c];
        alt_trig_add_seen[c] = alt_trig_add_tot[c];
        if (alt_trig_left[c] > 0 && (alt_trig_left[c] -= (int32_t)n) <= 0 && !alt_run)
            alt_off(c);
        if (alt_trig_tot[c] == alt_trig_seen[c])
            continue;
        alt_trig_seen[c] = alt_trig_tot[c];
        alt_on(c, alt_note[c][0] ? alt_note[c][0] : as_note(&alt_seq, sc->cfg.root, 0), 112, 0, 0);
        if (alt_gate_left[c] < 1u)
            alt_gate_left[c] = 1;
        alt_trig_left[c] = (int32_t)p;
        if (add && alt_run && sc->cfg.len) {
            uint32_t len = sc->cfg.len, k;
            if (alt_pos * 2u < alt_len(p))
                k = (sc->pos + len - 1u) % len;  /* the step just played */
            else {
                k = sc->pos % len;               /* the next one: its hit skipped once */
                alt_trig_skip[c] = (uint8_t)(k + 1u);
            }
            as_add(&alt_seq, c, k);
        }
    }
}

static void alter_step(void)
{
    as_event_t ev[AS_NCIRC];
    uint32_t c, j, h = alt_hold, kk[AS_NCIRC];
    for (c = 0; c < AS_NCIRC; c++)               /* the LFOs on PROB */
        alt_seq.pmod[c] = (int16_t)alt_prob_of(c);
    for (c = 0; c < AS_NCIRC; c++)               /* (the step each circle plays: REC) */
        kk[c] = alt_seq.c[c].pos;
    as_step(&alt_seq, ev);
    alt_gstep++;
    for (c = 0; c < AS_NCIRC; c++)               /* (TRIGGER + REC on the next step: it sounded already) */
        if (alt_trig_skip[c]) {
            if (kk[c] == alt_trig_skip[c] - 1u)
                ev[c].on = 0;
            alt_trig_skip[c] = 0;
        }
    for (c = 0; c < AS_NCIRC; c++)
        ar_step(c, kk[c] < alt_seq.c[c].cfg.len ? kk[c] : 0u, alt_seq.c[c].cfg.len, div_samples(2));
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
        alt_fxr_now[c] = kk[c] < AS_MAXLEN ? alt_fxr[c][kk[c]] : 0u;   /* (its REC'd fills: the dubs, alt_take) */
        if (fill && (h & AF_ROLLS))              /* a roll plays instead (the circle goes on underneath) */
            continue;
        if (!v->on) {                            /* no hit: the last note rings on its length, then stops */
            if (alt_gate_left[c])
                alt_gate_left[c]--;
            else
                alt_off(c);
            continue;
        }
        alt_msrc[c][1] = (int16_t)(v->force >= AS_ACCENT ? 32767 : v->force * 16383);   /* FORCE */
        alt_offset(c, P_DLY, v->dly);            /* the DELAY and REVERB lanes: this hit thrown into them */
        alt_offset(c, P_REV, v->rev);
        if (tp)                                  /* back to the base, then this hit's offsets */
            for (j = 0; j < AS_NTIMB; j++)
                alt_offset(c, tp[j], v->timb[j]);
        {
            uint32_t m = alt_emode[c] % AM_N, gate = v->gate;
            alt_env_apply(c, m, v->env);
            if (m == AM_PERC)
                gate = 255u;                     /* (its HOLD lets it go, alt_tick) */
            else if (m == AM_STAB)
                gate = gate > 2u ? 2u : gate;
            else if (m == AM_DRONE)
                gate = 255u;                     /* DRONE: until the next hit */
            if (m == AM_DRONE)
                alt_on_drone(c, v->note, v->vel, v->chord);   /* DRONE: a new layer, the oldest fading out */
            else
                alt_on(c, v->note, v->vel, v->glide, v->chord);
            alt_gate_left[c] = (uint8_t)(gate > 1u ? gate - 1u : 0u);
        }
        rep = alt_emode[c] == AM_DRONE ? 0u : v->rat;   /* repeats: the die's burst (not in a drone), or a RATCHET */
        if (fill) {
            uint32_t f = (h >> AF_RAT3) & 1u ? 2u : (h >> AF_RAT2) & 1u ? 1u : 0u;
            rep = f > rep ? f : rep;
        }
        if ((alt_fxr_now[c] & AFR_R32) && !rep && alt_emode[c] != AM_DRONE)
            rep = 1;                             /* a REC'd ROLL 1/32: the hit doubled */
        alt_rat_note[c] = v->note;
        alt_rat_ch[c][0] = v->chord[0];
        alt_rat_ch[c][1] = v->chord[1];
        alt_rat_vel[c] = v->vel;
        if (rep) {
            alt_rat_left[c] = (uint8_t)rep;
            alt_rat_dt[c] = div_samples(2) / (rep + 1u);
        } else if (v->flam && alt_emode[c] != AM_DRONE) {
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
static void alter_tick_(uint32_t n)
{
    uint32_t p = div_samples(2), h, c, len;      /* a sixteenth */
    if (!alt_ready) {
        as_init(&alt_seq, 0xA17E5u);
        alt_ready = 1;
    }
    alt_take();
    alt_trig_now(n);                             /* (TRIGGER: at once) */
    ar_tick(n);
    alt_gl_tick(n, p);
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
        alt_gl_n = 0;                            /* the LFOs from bar 1 */
        alt_gl_tick(0, p);
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
        const as_circ_t *r = &alt_seq.c[alt_tgt];
        alt_roll_pos += (int32_t)n;
        while (alt_roll_pos >= (int32_t)alt_roll_period(h, p)) {   /* (BUILD UP: the step shrinks as it goes) */
            uint32_t per = alt_roll_period(h, p);
            uint32_t v = (h >> AF_BUILD) & 1u ? 30u + alt_roll_n * 3u : 40u + alt_roll_n * 6u;
            alt_roll_pos -= (int32_t)per;
            alt_on(alt_tgt, as_note(&alt_seq, r->cfg.root, 0), v > 124u ? 124u : v, 0, 0);
            alt_roll_n++;
            if (ar_on && r->cfg.len) {           /* REC: ROLL 1/16 adds the step, ROLL 1/32 doubles it too */
                uint32_t k = (r->pos + r->cfg.len - 1u) % r->cfg.len;
                as_add(&alt_seq, alt_tgt, k);
                if ((h >> AF_ROLL32) & 1u)
                    alt_fxr[alt_tgt][k] |= AFR_R32;
            }
        }
    }
    if (ar_on && (h & AF_DUBS)) {                /* REC: the dubs held, written on the step playing */
        const as_circ_t *r = &alt_seq.c[alt_tgt];
        if (r->cfg.len) {
            uint32_t k = (r->pos + r->cfg.len - 1u) % r->cfg.len;
            alt_fxr[alt_tgt][k] |= (uint8_t)(((h >> AF_DLY) & 1u ? AFR_DLY : 0u) | ((h >> AF_REV) & 1u ? AFR_REV : 0u) |
                                             ((h >> AF_DESTROY) & 1u ? AFR_DST : 0u));
        }
    }
}

static void alter_tick(uint32_t n)
{
    alter_tick_(n);
    am_clock(alt_gstep, alt_pos, div_samples(2), alt_run && song.playing);   /* (the duck's beat: alter_fx.c) */
    alt_pos_src(div_samples(2));                 /* (after the steps: a cycle's first hit hears POS 0) */
}
