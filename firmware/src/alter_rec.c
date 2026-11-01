/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's REC (Romain 01/11): the knob moves of the chosen circle, recorded on its steps and played back in a loop.
 * Included by alter_play.c after alt_move (the audio side; the UI only writes ar_on and ar_clr_cnt).
 *
 *   REC tapped    overdub on (its LED lit): a knob turned on the chosen circle's part, whatever the page (everything
 *                 that goes through the queue alt_q: the sound's 8, ADSR, DIST, INSERT, the DELAY and REVERB sends) is
 *                 written at the step the circle is playing, replacing what was there; a knob that keeps moving writes
 *                 the steps it crosses too (ar_live). REC tapped again: off, the moves play on.
 *   playback      one value per step, the written steps joined by straight lines (a ramp a step long towards the next
 *                 step's value: no stairs); an ENUM (MODE, the INSERT's TYPE) jumps at its step.
 *   cleared       REC held 1 s (the chosen circle), OCT- + OCT+ + REC (all), a DIE tap, CHAOS, TOTAL CHAOS, a sound or a
 *                 scene loaded (alt_roll_cnt, alt_preset_cnt: alt_take). Not saved with the scenes (yet).
 * The values are the base (without the die's offsets, alt_applied), so a draw still plays on top. */

#define AR_MAX 64                                /* written steps per circle (all its parameters) */
#define AR_NID 6                                 /* parameters per circle */
#define AR_NONE 0xFFu
typedef struct { uint8_t id, step; int16_t v; } ar_ev_t;
static ar_ev_t ar_ev[AS_NCIRC][AR_MAX];
static uint8_t ar_n[AS_NCIRC];                   /* written steps */
static uint8_t ar_id[AS_NCIRC][AR_NID];          /* the parameters recorded (AR_NONE: a free place) */
static int32_t ar_cur[AS_NCIRC][AR_NID];         /* each one's value now, Q16 .. */
static int32_t ar_inc[AS_NCIRC][AR_NID];         /* .. its ramp a sample, Q16 .. */
static int32_t ar_left[AS_NCIRC][AR_NID];        /* .. and the samples it still runs */
static uint8_t ar_live[AS_NCIRC][AR_NID];        /* steps a knob just turned still writes */
static uint8_t ar_now[AS_NCIRC];                 /* the step each circle is playing */
static uint8_t ar_init;
/* UI -> here */
static volatile uint8_t ar_on;                   /* REC lit: the knobs are written */
static volatile uint32_t ar_clr_cnt[AS_NCIRC];   /* REC held: circle c cleared */
static uint32_t ar_clr_seen[AS_NCIRC];

/* what REC writes: the part's sound, its envelope, its effects (not routings, the matrix, the mix) */
static int ar_param(uint32_t p)
{
    return p <= P_REL || (p >= P_E0 && p < P_E0 + 8u) || p == P_DIST || p == P_DLY || p == P_REV ||
           (p >= P_ITYPE && p <= P_IMIX);
}

static void ar_clear(uint32_t c)
{
    uint32_t i;
    ar_n[c] = 0;
    for (i = 0; i < AR_NID; i++) {
        ar_id[c][i] = AR_NONE;
        ar_left[c][i] = 0;
        ar_live[c][i] = 0;
    }
}

static int ar_enum(uint32_t c, uint32_t p)
{
    return param_desc_of(eng_idx(trk[c].eng_req), p)->fmt == F_ENUM;
}

/* parameter p of part c without the die's offset, and set to b (the offset kept on top) */
static int32_t ar_base(uint32_t c, uint32_t p)
{
    int32_t sl = alt_slot(p);
    return trk[c].p[p] - (sl >= 0 ? alt_applied[c][sl] : 0);
}
static void ar_put(uint32_t c, uint32_t p, int32_t b)
{
    track_t *t = &trk[c];
    const param_desc_t *ds = param_desc_of(eng_idx(t->eng_req), p);
    int32_t sl = alt_slot(p);
    b = clamp(b, ds->min, ds->max);
    if (b == ar_base(c, p))
        return;
    if (sl >= 0) {
        int16_t *ap = &alt_applied[c][sl];
        int32_t v = clamp(b + *ap, ds->min, ds->max);
        t->p[p] = (int16_t)v;
        *ap = (int16_t)(v - b);
    } else
        t->p[p] = (int16_t)b;
}

/* p's place in circle c's list (a new one if room: mk), -1 = none */
static int32_t ar_slot_of(uint32_t c, uint32_t p, int mk)
{
    int32_t i, fr = -1;
    for (i = 0; i < AR_NID; i++) {
        if (ar_id[c][i] == p)
            return i;
        if (ar_id[c][i] == AR_NONE && fr < 0)
            fr = i;
    }
    if (mk && fr >= 0) {
        ar_id[c][fr] = (uint8_t)p;
        ar_left[c][fr] = 0;
        ar_live[c][fr] = 0;
    }
    return mk ? fr : -1;
}

/* step k of circle c gets v for p (replaced if written); 0 = no room */
static int ar_write(uint32_t c, uint32_t p, uint32_t k, int32_t v)
{
    uint32_t i;
    for (i = 0; i < ar_n[c]; i++)
        if (ar_ev[c][i].id == p && ar_ev[c][i].step == k) {
            ar_ev[c][i].v = (int16_t)v;
            return 1;
        }
    if (ar_n[c] >= AR_MAX)
        return 0;
    ar_ev[c][ar_n[c]].id = (uint8_t)p;
    ar_ev[c][ar_n[c]].step = (uint8_t)k;
    ar_ev[c][ar_n[c]].v = (int16_t)v;
    ar_n[c]++;
    return 1;
}

/* p's value at step s of circle c (LEN len): the written step at or before s and the next one, joined by a straight
 * line around the circle (an ENUM: the one at or before); 0 = nothing written */
static int ar_value(uint32_t c, uint32_t p, uint32_t s, uint32_t len, int lin, int32_t *out)
{
    int32_t a = -1, b = -1, va = 0, vb = 0, da = 1 << 20, db = 1 << 20;
    uint32_t i;
    for (i = 0; i < ar_n[c]; i++) {
        const ar_ev_t *e = &ar_ev[c][i];
        int32_t dp, dn;
        if (e->id != p || e->step >= len)
            continue;
        dp = ((int32_t)s - e->step + (int32_t)len) % (int32_t)len;      /* how far back */
        dn = ((int32_t)e->step - (int32_t)s + (int32_t)len) % (int32_t)len;
        if (!dn)
            dn = (int32_t)len;                                          /* how far on (s itself: a whole turn) */
        if (dp < da)
            da = dp, a = e->step, va = e->v;
        if (dn < db)
            db = dn, b = e->step, vb = e->v;
    }
    if (a < 0)
        return 0;
    *out = va;
    if (lin && da && b != a)
        *out = va + (vb - va) * da / (da + db);
    return 1;
}

/* a knob moved p of part c (after alt_move): written at the step playing, if REC is on and the circles run */
static void ar_moved(uint32_t c, uint32_t p, int playing)
{
    int32_t i, b;
    if (!ar_on || !playing || c >= AS_NCIRC || !ar_param(p) || (i = ar_slot_of(c, p, 1)) < 0)
        return;
    b = ar_base(c, p);
    if (!ar_write(c, p, ar_now[c], b))
        return;
    ar_cur[c][i] = b << 16;                      /* (the playback holds what the hand set) */
    ar_left[c][i] = 0;
    ar_live[c][i] = 2;
}

/* circle c plays step k (its LEN len), a sixteenth of p samples: each parameter heads for the next step's value */
static void ar_step(uint32_t c, uint32_t k, uint32_t len, uint32_t p)
{
    uint32_t i;
    ar_now[c] = (uint8_t)k;
    for (i = 0; i < AR_NID; i++) {
        uint32_t id = ar_id[c][i];
        int32_t v;
        if (id == AR_NONE)
            continue;
        if (ar_live[c][i]) {                     /* the knob still moving: this step takes its value too */
            ar_live[c][i]--;
            if (ar_on) {
                ar_write(c, id, k, ar_base(c, id));
                ar_cur[c][i] = ar_base(c, id) << 16;
                ar_left[c][i] = 0;
                continue;
            }
        }
        if (ar_enum(c, id)) {                    /* an ENUM: this step's, at once */
            if (ar_value(c, id, k, len, 0, &v))
                ar_put(c, id, v);
            ar_left[c][i] = 0;
            ar_cur[c][i] = ar_base(c, id) << 16;
        } else if (ar_value(c, id, (k + 1u) % len, len, 1, &v)) {
            ar_cur[c][i] = ar_base(c, id) << 16;
            ar_left[c][i] = (int32_t)(p ? p : 1u);
            ar_inc[c][i] = (int32_t)(((int64_t)(v << 16) - ar_cur[c][i]) / ar_left[c][i]);
        }
    }
}

/* once a block (n samples): the clears asked for, the ramps on */
static void ar_tick(uint32_t n)
{
    uint32_t c, i;
    if (!ar_init) {
        for (c = 0; c < AS_NCIRC; c++)
            ar_clear(c);
        ar_init = 1;
    }
    for (c = 0; c < AS_NCIRC; c++) {
        if (ar_clr_cnt[c] != ar_clr_seen[c]) {
            ar_clr_seen[c] = ar_clr_cnt[c];
            ar_clear(c);
        }
        for (i = 0; i < AR_NID; i++) {
            if (ar_id[c][i] == AR_NONE || ar_left[c][i] <= 0)
                continue;
            if ((ar_left[c][i] -= (int32_t)n) <= 0)
                ar_cur[c][i] += ar_inc[c][i] * ((int32_t)n + ar_left[c][i]);
            else
                ar_cur[c][i] += ar_inc[c][i] * (int32_t)n;
            ar_put(c, ar_id[c][i], (ar_cur[c][i] + 32768) >> 16);
        }
    }
}
