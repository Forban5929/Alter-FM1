/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's GLOBAL LFO and the circles' own matrix sources (included by mod.c, after its MS_* / MD_* lists: mod.c
 * reads them in the audio ISR; alter_play.c computes them once per block, alter_ui.c edits the settings).
 *
 * GLOBAL: one LFO on the tempo grid for the three circles (parts 0..2) together. Its WAVE (SIN TRI SAW SQR S&H
 * SLEW, RAMP+ RAMP- from 0 to full and back to 0), its RATE (a note value, 1/16 .. 8 bars, counted from PLAY: a
 * 4-bar ramp starts on bar 1) and four slots, each a destination (the same for every engine) and an amount
 * (-64..63, as the matrix's: AMT 64 = the whole range). The destinations: PITCH (+-11.8 semitones at full), SHAPE
 * and FILTER (the engines' m->shape / m->cutoff, as the LFO DEST SHP / FLT: on ALTER MORPH and HARMONICS), LEVEL
 * (a gain, as AMP), the DIST DELAY REVERB sends, PAN, and PROB (the circles' chance, added to their PROB).
 * A destination the GLOBAL reaches is taken from the circle's own LFO: its LFO DEST routing there (P_LD_*) and its
 * matrix slots fed by the LFO (or its S&H / SLEW) on it are left out while the GLOBAL has it (the GLOBAL leads).
 * POS: where circle i is in its cycle, 0 at its first step .. 1 at the end of its last (a ramp on LEN, 0 stopped).
 * FORCE: the force of circle i's last hit, 0 ghost, 0.5 normal, 1 accent (held until the next hit). */

enum { GW_SIN, GW_TRI, GW_SAW, GW_SQR, GW_SH, GW_SLEW, GW_RUP, GW_RDN, GW_N };
enum { GD_OFF, GD_PITCH, GD_SHAPE, GD_FILT, GD_LEVEL, GD_DIST, GD_DLY, GD_REV, GD_PAN, GD_PROB, GD_N };
#define GL_NRATE 8
#define GL_NSLOT 4
#define GL_RATE_DEF 4                            /* 1 bar */
static const char *const GL_WAVE_NAME[GW_N] = {"SIN", "TRI", "SAW", "SQR", "S&H", "SLEW", "RAMP+", "RAMP-"};
static const char *const GL_RATE_NAME[GL_NRATE] = {"1/16", "1/8", "1/4", "1/2", "1 BAR", "2 BAR", "4 BAR", "8 BAR"};
static const char *const GL_DST_NAME[GD_N] = {"OFF", "PITCH", "SHAPE", "FILT", "LEVEL", "DIST", "DELAY", "REVRB",
                                              "PAN", "PROB"};
static const uint8_t GL_RATE_STEPS[GL_NRATE] = {1, 2, 4, 8, 16, 32, 64, 128};   /* sixteenths a cycle */
/* a GLOBAL destination's matrix destination (GD_PROB: none, alter_play.c) */
static const uint8_t GL_MD[GD_N] = {MD_OFF, MD_PITCH, MD_SHP, MD_CUT, MD_AMP, MD_DIST, MD_DLY, MD_REV, MD_PAN, MD_OFF};

typedef struct {
    uint8_t wave, rate;                          /* GW_*, 0 .. GL_NRATE - 1 */
    uint8_t dst[GL_NSLOT];                       /* GD_* */
    int8_t amt[GL_NSLOT];                        /* -64 .. 63 */
} alt_gl_t;

static volatile alt_gl_t alt_gl = {GW_SIN, GL_RATE_DEF, {0}, {0}};   /* UI: the settings (bytes) */
static int32_t alt_gl_val;                       /* here: the GLOBAL now, Q15 (bipolar; the ramps 0..32767) */
static int16_t alt_msrc[3][2];                   /* here: each circle's POS and FORCE, Q15 0..32767 */

static void alt_gl_reset(void)
{
    uint32_t k;
    alt_gl.wave = GW_SIN;
    alt_gl.rate = GL_RATE_DEF;
    for (k = 0; k < GL_NSLOT; k++) {
        alt_gl.dst[k] = GD_OFF;
        alt_gl.amt[k] = 0;
    }
}

static int alt_gl_uni(void) { return alt_gl.wave == GW_RUP || alt_gl.wave == GW_RDN; }

/* the matrix destinations the GLOBAL holds now (bit MD_*): 0 = it rests */
static uint32_t alt_gl_take(void)
{
    uint32_t k, m = 0;
    for (k = 0; k < GL_NSLOT; k++)
        if (alt_gl.amt[k] && alt_gl.dst[k] < GD_N && GL_MD[alt_gl.dst[k]])
            m |= 1u << GL_MD[alt_gl.dst[k]];
    return m;
}

/* the GLOBAL's offset on the circles' PROB (0 = none): AMT 64 at full = +-127 */
static int32_t alt_gl_prob(void)
{
    uint32_t k;
    int32_t o = 0;
    for (k = 0; k < GL_NSLOT; k++)
        if (alt_gl.dst[k] == GD_PROB && alt_gl.amt[k])
            o += (((alt_gl_val * alt_gl.amt[k]) >> 6) * 127) >> 15;
    return o;
}

/* the circle sources (mod.c mod_tsrc): s 0 POS, 1 FORCE of part t */
static int32_t alt_msrc_of(const track_t *t, uint32_t s)
{
    uint32_t i = (uint32_t)(t - trk);
    return i < 3u ? alt_msrc[i][s & 1u] : 0;
}
