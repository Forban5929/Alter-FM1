/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER : divisions 64 bits pour la vraie FM-1 (pi32v2).
 * Le firmware est relié sans bibliothèque du compilateur : une division d'un int64 par une valeur VARIABLE
 * appelle __udivdi3 / __divdi3 / __umoddi3 / __moddi3, absentes → le lien échouait (03/11, afl_coef de
 * alter_filter.c, puis reso_render…). Version simple « poser la division » bit par bit : résultat exact, identique
 * à l'opérateur / et % du C (quotient tronqué vers 0, reste du signe du dividende).
 * Coût : ~64 tours de boucle par appel. Seuls les calculs de coefficients (par bloc) en font : à surveiller avec la
 * charge CPU de la version test. Inclus seulement par felucca.c (FM-1) ; le navigateur a ses propres divisions. */
#include <stdint.h>

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
