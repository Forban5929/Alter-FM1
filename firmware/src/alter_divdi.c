/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER : divisions 64 bits pour la vraie FM-1 (pi32v2).
 * Le firmware est relié sans bibliothèque du compilateur : une division d'un int64 par une valeur VARIABLE
 * appelle __udivdi3 / __divdi3 / __umoddi3 / __moddi3, absentes → le lien échouait (03/11, afl_coef de
 * alter_filter.c, puis reso_render…). Version simple « poser la division » bit par bit : résultat exact, identique
 * à l'opérateur / et % du C (quotient tronqué vers 0, reste du signe du dividende).
 * Coût : ~64 tours de boucle par appel. Seuls les calculs de coefficients (par bloc) en font : à surveiller avec la
 * charge CPU de la version test. Inclus seulement par felucca.c (FM-1) ; le navigateur a ses propres divisions. */
#include <stdint.h>

/* (10/11, Romain: CPU cuts with the MASTER FILTER, COMP ..: their coefficients divide by 64 bits every 8 samples)
 * a divisor of 32 bits or less (nearly all of ours): base 2^16 long division with the chip's 32-bit divide
 * (Hacker's Delight divlu), a few divides instead of 64 turns of the loop; the same exact quotient and remainder */
static uint32_t alt_nlz32(uint32_t x)            /* leading zeros (x != 0), without a library */
{
    uint32_t n = 0;
    if (!(x >> 16)) { n += 16; x <<= 16; }
    if (!(x >> 24)) { n += 8; x <<= 8; }
    if (!(x >> 28)) { n += 4; x <<= 4; }
    if (!(x >> 30)) { n += 2; x <<= 2; }
    if (!(x >> 31)) n += 1;
    return n;
}
static uint32_t alt_divlu(uint32_t u1, uint32_t u0, uint32_t v, uint32_t *r)   /* (u1:u0) / v, u1 < v */
{
    const uint32_t b = 65536u;
    uint32_t un1, un0, vn1, vn0, q1, q0, un32, un21, un10, rhat, s = alt_nlz32(v);
    v <<= s;
    vn1 = v >> 16;
    vn0 = v & 0xFFFFu;
    un32 = s ? (u1 << s) | (u0 >> (32u - s)) : u1;
    un10 = u0 << s;
    un1 = un10 >> 16;
    un0 = un10 & 0xFFFFu;
    q1 = un32 / vn1;
    rhat = un32 - q1 * vn1;
    while (q1 >= b || q1 * vn0 > b * rhat + un1) {
        q1--;
        rhat += vn1;
        if (rhat >= b)
            break;
    }
    un21 = un32 * b + un1 - q1 * v;
    q0 = un21 / vn1;
    rhat = un21 - q0 * vn1;
    while (q0 >= b || q0 * vn0 > b * rhat + un0) {
        q0--;
        rhat += vn1;
        if (rhat >= b)
            break;
    }
    *r = (un21 * b + un0 - q0 * v) >> s;
    return q1 * b + q0;
}

__attribute__((used, noinline)) static uint64_t alt_udivmod64(uint64_t n, uint64_t d, uint64_t *rem)
{
    uint64_t q = 0, r = 0;
    int i;
    if (d == 0) {                        /* division par 0 : pas d'exception sur la puce, résultat sans risque */
        if (rem)
            *rem = n;
        return ~0ull;
    }
    if ((d >> 32) == 0 && (n >> 32) == 0) {             /* cas courant : tout tient sur 32 bits */
        uint32_t a = (uint32_t)n, b = (uint32_t)d;
        if (rem)
            *rem = a % b;
        return a / b;
    }
    if ((d >> 32) == 0) {                               /* un diviseur sur 32 bits : deux divisions de 32 bits */
        uint32_t b = (uint32_t)d, nh = (uint32_t)(n >> 32), qh = nh / b, rl, ql;
        ql = alt_divlu(nh - qh * b, (uint32_t)n, b, &rl);
        if (rem)
            *rem = rl;
        return (uint64_t)qh << 32 | ql;
    }
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d) {
            r -= d;
            q |= 1ull << i;
        }
    }
    if (rem)
        *rem = r;
    return q;
}

__attribute__((used)) uint64_t __udivdi3(uint64_t n, uint64_t d)
{
    return alt_udivmod64(n, d, 0);
}

__attribute__((used)) uint64_t __umoddi3(uint64_t n, uint64_t d)
{
    uint64_t r;
    alt_udivmod64(n, d, &r);
    return r;
}

__attribute__((used)) int64_t __divdi3(int64_t n, int64_t d)
{
    uint64_t un = n < 0 ? 0ull - (uint64_t)n : (uint64_t)n, ud = d < 0 ? 0ull - (uint64_t)d : (uint64_t)d;
    uint64_t q = alt_udivmod64(un, ud, 0);
    return (int64_t)(((n < 0) != (d < 0)) ? 0ull - q : q);
}

__attribute__((used)) int64_t __moddi3(int64_t n, int64_t d)
{
    uint64_t un = n < 0 ? 0ull - (uint64_t)n : (uint64_t)n, ud = d < 0 ? 0ull - (uint64_t)d : (uint64_t)d, r;
    alt_udivmod64(un, ud, &r);
    return (int64_t)(n < 0 ? 0ull - r : r);
}
