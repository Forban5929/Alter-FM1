/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER (v0.75): a division by a value fixed for the block, as a multiply (the chip divides slowly; a sample's loop of
 * PLATE, CORDE, RING divided up to 6 times). Exact: the same quotient as C's / (truncated toward 0) when |n| * d < 2^s
 * and d > 2^(s - 32) (M fits in 32 bits): n * ceil(2^s / d) / 2^s is over n / d by less than n / 2^s < 1 / d, never
 * enough to reach the next whole number. One 32 x 32 -> 64 multiply (one instruction on the pi32v2). */
#ifndef ENG_DVM_H
#define ENG_DVM_H
static inline uint32_t dvm_make(uint32_t d, uint32_t s)   /* once a block (d > 0) */
{
    return (uint32_t)(((1ull << s) + d - 1u) / d);
}
static inline int32_t dvm_div(int32_t n, uint32_t mg, uint32_t s)
{
    uint32_t a = n < 0 ? 0u - (uint32_t)n : (uint32_t)n;
    int32_t q = (int32_t)(((uint64_t)a * mg) >> s);
    return n < 0 ? -q : q;
}
#endif
