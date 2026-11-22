/* ALTER v1.12 (Romain 22/11, after the listening page: "trop de doublons dirt/saturation/dst/ins"): the engines' own
 * transformation, in place of DIRT (or another knob): a RING modulator (ALTER on NOIS, CORDE on DIRT) or a sine WAVE
 * FOLDER (RING on DIRT, RESO on DIRT, PHYS on EXC, NOISE on DRFT); PLATE keeps its DIRT (eng_dirt.h). Both through
 * alt_curve: 0 = nothing runs, 0..10 subtle. Integers only; per block: the coefficients; per sample: one sine_i.
 *
 * RING (Romain: "dosage puis couleur"): up to 64 the ring is mixed in, its carrier at x 0.5 the note (low, rolling);
 * past 64 fully ringed and the carrier rises: x 1.5 (rough) near 92, x 2.71 (metal, bells) at 127. The carrier follows
 * the voice's pitch (vmod_t inc); its phase is the voice's own (one uint32_t in each engine's voice state).
 * FOLD: sin(pi/2 g x / 131072) scaled back so that a signal passes as it is at g = 1 (the engines' levels before their
 * knee reach ~60000: a lower reference folded them at the first step); g 1 .. 24 (cube of alt_curve), a make-up. */
#ifndef ENG_XFORM_H
#define ENG_XFORM_H
#include "eng_curve.h"

typedef struct { int32_t on, wet; uint32_t rat; } ering_t;   /* wet Q15, rat Q12 */
typedef struct { int32_t on, g8, amp; } efold_t;

static inline void ering_set(ering_t *r, int32_t k)
{
    int32_t ka = alt_curve(k);
    r->on = k > 0;
    if (k <= 64) {
        int32_t c = ka > 80 ? 80 : ka;                   /* (alt_curve(64) = 80) */
        r->wet = (int32_t)((int64_t)c * c * c * 32768 / 512000);
        r->rat = 2048u;                                  /* x 0.5 */
    } else {
        r->wet = 32768;
        r->rat = 2048u + (uint32_t)(k - 64) * (11100u - 2048u) / 63u;   /* x 0.5 .. x 2.71 */
    }
}
/* x the voice's sample; *ph its carrier's phase; cinc = (inc * rat) >> 12, once a block */
static inline int32_t ering_run(const ering_t *r, int32_t x, uint32_t *ph, uint32_t cinc)
{
    int32_t y;
    if (!r->on)
        return x;
    y = (int32_t)(((int64_t)x * sine_i(*ph)) >> 15);
    *ph += cinc;
    y = (int32_t)(((int64_t)y * 46341) >> 15);           /* (x sqrt 2: a ring halves the power) */
    return x + (int32_t)(((int64_t)(y - x) * r->wet) >> 15);
}
static inline uint32_t ering_inc(const ering_t *r, uint32_t inc) { return (uint32_t)(((uint64_t)inc * r->rat) >> 12); }

#define EFOLD_MK 25                                      /* make-up: 1 + (g - 1) x 25 / 256 (measured) */
static inline void efold_set_mk(efold_t *f, int32_t k, int32_t mk)   /* mk: the make-up, x 1/256 of (g - 1) */
{
    int32_t ka = alt_curve(k), mk8;
    f->on = k > 0;
    f->g8 = 256 + (int32_t)((int64_t)ka * ka * ka * 23 * 256 / (127 * 127 * 127));   /* g, Q8: 1 .. 24 */
    mk8 = 256 + (f->g8 - 256) * mk / 256;
    f->amp = (int32_t)((int64_t)83443 * mk8 / f->g8);    /* 131072 x 2 / pi / g x make-up (x Q15) */
}
static inline void efold_set(efold_t *f, int32_t k) { efold_set_mk(f, k, EFOLD_MK); }
static inline int32_t efold_run(const efold_t *f, int32_t x)
{
    uint32_t ph;
    if (!f->on)
        return x;
    x = clamp(x, -(1 << 20), 1 << 20);
    ph = (uint32_t)(((int64_t)x * f->g8) << 5);         /* (pi/2 x g / 131072 as a turn of 2^32: x g 2^13) */
    return (int32_t)(((int64_t)sine_i(ph) * f->amp) >> 15);
}
#endif
