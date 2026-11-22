/* SPDX-License-Identifier: GPL-3.0-only
 * v1.08 (Romain 22/11, the knob audit: "too subtle from 0 to 50, but 0 to 1 must stay subtle up to 10"): a knob's
 * curve for the settings that did little in their lower half. 0..10 climbs gently to 40 (where the old setting began
 * to be heard), then 10..127 covers 40..127 evenly. The factory sounds' values were converted (alt_curve_inv, by
 * hand in alter_sounds.c) so they sound as before. */
#ifndef ENG_CURVE_H
#define ENG_CURVE_H
static inline int32_t alt_curve(int32_t x)
{
    x = x < 0 ? 0 : x > 127 ? 127 : x;
    return x <= 10 ? x * 4 : 40 + (x - 10) * 87 / 117;
}
#endif
