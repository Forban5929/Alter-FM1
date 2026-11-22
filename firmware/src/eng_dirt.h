/* ALTER v1.09 (Romain 22/11: "DIRT ne marche que sur RING BELL"; choice: "les deux" = saturation, then grain):
 * the output stage of DIRT shared by RING, PLATE and CORDE (their in-loop drive stays as it was, it thickens the
 * ringing). Through alt_curve (0..10 gentle): a tanh driven x1 .. x26 with a make-up that keeps the level near,
 * then past 90 the grain: fewer bits, held samples (a lower rate) and rare crackles. Integers only; no state kept
 * between blocks (the hold restarts each block: at most 7 samples of 32). */
#ifndef ENG_DIRT_H
#define ENG_DIRT_H
#include "eng_curve.h"

typedef struct { int32_t g, mk, bits, hold, crk; } edirt_t;

static inline void edirt_set(edirt_t *d, int32_t dirt)   /* once a block */
{
    int32_t k = alt_curve(dirt), gr = dirt > 90 ? dirt - 90 : 0;   /* gr 0..37 */
    d->g = 4096 + k * k * k / 20;                         /* Q12: x1 .. x26 */
    d->mk = (int32_t)(((int64_t)32768 * 14 * 4096) / (14 * 4096 + (d->g - 4096)));   /* Q15: 1 / (1 + (G - 1) / 14) */
    d->bits = gr ? 1 + gr * 11 / 37 : 0;                  /* bits taken off a Q15 sample: 1 .. 12 */
    d->hold = 1 + gr * gr / 228;                          /* 1 .. 7 samples (slowly at first) */
    d->crk = gr * gr * 2;                                 /* the crackle's chance (of 65536): up to 4 % */
}

/* x Q15-ish; h, c: the block's hold value and counter (locals); rng: the voice's noise */
static inline int32_t edirt_run(const edirt_t *d, int32_t x, int32_t *h, int32_t *c, int32_t *rng)
{
    if (d->g == 4096)
        return x;
    x = clamp((int32_t)(((int64_t)x * d->g) >> 12), -65535, 65535);
    x = (int32_t)(((int64_t)softclip(x) * d->mk) >> 15);
    if (!d->bits)
        return x;
    if (*c <= 0) {                                        /* the grain: held, fewer bits */
        *h = x & ~((1 << d->bits) - 1);
        *c = d->hold;
        if ((int32_t)(noise32(rng) >> 16) < d->crk)       /* a crackle: the sign flipped, a jump */
            *h = -*h + ((int32_t)(noise32(rng) >> 18) - 8192);
    }
    (*c)--;
    return *h;
}
#endif
