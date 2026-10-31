/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's LFOs (included by mod.c, after its MS_* / MD_* lists: mod.c reads them in the audio ISR; alter_play.c
 * computes the GLOBAL once per block, alter_ui.c edits the settings). Four LFOs, each a classic one: WAVE, RATE (a
 * note value of the tempo), DEPTH (-64..63: AMT 64 = the whole range, as the matrix's) and one DEST, the same list
 * for every engine:
 *   PITCH (+-11.8 semitones at full), SHAPE and FILTER (the engines' m->shape / m->cutoff: on ALTER MORPH and
 *   HARMONICS), LEVEL (a gain, as AMP), the DIST DELAY REVERB sends, PAN, PROB (the circle's chance, added to PROB).
 * LFO 1 2 3: each circle's own (its part's Felucca LFO: WAVE = P_LWAVE, RATE = P_LSYNC, TRIG FREE; DEST and DEPTH
 * here, alt_cl). GLOBAL: one LFO for the three circles together (alt_gl): its own WAVEs (SIN TRI SAW SQR S&H SLEW,
 * RAMP+ RAMP- from 0 to full), its RATE counted from PLAY (a 4-bar ramp starts on bar 1), four DEST slots.
 * A destination the GLOBAL reaches is taken from the circle's own LFO (its DEST there, its P_LD_* routing there, its
 * matrix slots fed by the LFO or its S&H / SLEW there): the GLOBAL leads.
 * The matrix (MTX) keeps Felucca's four slots per circle; two sources of ours:
 *   POS: where circle i is in its cycle, 0 at its first step .. 1 at the end of its last (0 stopped).
 *   FORCE: the force of circle i's last hit, 0 ghost, 0.5 normal, 1 accent (held until the next hit). */

enum { GW_SIN, GW_TRI, GW_SAW, GW_SQR, GW_SH, GW_SLEW, GW_RUP, GW_RDN, GW_N };
enum { GD_OFF, GD_PITCH, GD_SHAPE, GD_FILT, GD_LEVEL, GD_DIST, GD_DLY, GD_REV, GD_PAN, GD_PROB, GD_N };
#define GL_NSLOT 4
static const char *const GL_WAVE_NAME[GW_N] = {"SIN", "TRI", "SAW", "SQR", "S&H", "SLEW", "RAMP+", "RAMP-"};
static const char *const GL_DST_NAME[GD_N] = {"OFF", "PITCH", "SHAPE", "FILTER", "LEVEL", "DIST", "DELAY", "REVERB",
                                              "PAN", "PROB"};
/* the RATEs, fast to slow: a cycle in sixths of a sixteenth (the GLOBAL); P_LSYNC's value for each (a circle's,
 * params.c N_LSYNC: Felucca has no 8 BAR) */
#define GL_NRATE 11
#define CL_NRATE 10
#define GL_RATE_DEF 7                            /* 1 bar */
#define CL_RATE_DEF 5                            /* 1/4 */
static const char *const GL_RATE_NAME[GL_NRATE] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/2", "1 BAR",
                                                   "2 BAR", "4 BAR", "8 BAR"};
static const uint16_t GL_RATE_6TH[GL_NRATE] = {3, 4, 6, 8, 12, 24, 48, 96, 192, 384, 768};
static const uint8_t CL_RATE_SYNC[CL_NRATE] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
/* a destination's matrix destination (GD_PROB: none, alter_play.c) */
static const uint8_t GL_MD[GD_N] = {MD_OFF, MD_PITCH, MD_SHP, MD_CUT, MD_AMP, MD_DIST, MD_DLY, MD_REV, MD_PAN, MD_OFF};

typedef struct {
    uint8_t wave, rate;                          /* GW_*, 0 .. GL_NRATE - 1 */
    uint8_t dst[GL_NSLOT];                       /* GD_* */
    int8_t amt[GL_NSLOT];                        /* -64 .. 63 */
} alt_gl_t;
typedef struct {
    uint8_t dst;                                 /* GD_* */
    int8_t depth;                                /* -64 .. 63 */
} alt_cl_t;

static volatile alt_gl_t alt_gl = {GW_SIN, GL_RATE_DEF, {0}, {0}};   /* UI: the settings (bytes) */
static volatile alt_cl_t alt_cl[3];              /* UI: each circle's LFO DEST and DEPTH */
static int32_t alt_gl_val;                       /* here: the GLOBAL now, Q15 (bipolar; the ramps 0..32767) */
static uint32_t alt_gl_ph;                       /* .. and its phase (the screen) */
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
    for (k = 0; k < 3u; k++) {
        alt_cl[k].dst = GD_OFF;
        alt_cl[k].depth = 0;
    }
}

static int alt_gl_uni(void) { return alt_gl.wave == GW_RUP || alt_gl.wave == GW_RDN; }

/* the GLOBAL has destination g in a slot with an amount */
static int alt_gl_has(uint32_t g)
{
    uint32_t k;
    for (k = 0; k < GL_NSLOT; k++)
        if (alt_gl.dst[k] == g && alt_gl.amt[k])
            return 1;
    return 0;
}

/* the matrix destinations the GLOBAL holds now (bit MD_*): 0 = it rests */
static uint32_t alt_gl_take(void)
{
    uint32_t k, m = 0;
    for (k = 0; k < GL_NSLOT; k++)
        if (alt_gl.amt[k] && alt_gl.dst[k] < GD_N && GL_MD[alt_gl.dst[k]])
            m |= 1u << GL_MD[alt_gl.dst[k]];
    return m;
}

/* circle c's LFO acts now: its destination (GD_*), 0 = none (off, no depth, or the GLOBAL leads there) */
static uint32_t alt_cl_on(uint32_t c)
{
    uint32_t g = c < 3u ? alt_cl[c].dst : 0u;
    if (!g || g >= GD_N || !alt_cl[c].depth || alt_gl_has(g))
        return 0;
    return g;
}

/* the PROB offset of circle c (0 = none): the GLOBAL's, or the circle's own LFO (lfo: its value); AMT 64 at full =
 * +-127 */
static int32_t alt_prob_of(uint32_t c, int32_t lfo)
{
    uint32_t k;
    int32_t o = 0;
    for (k = 0; k < GL_NSLOT; k++)
        if (alt_gl.dst[k] == GD_PROB && alt_gl.amt[k])
            o += (((alt_gl_val * alt_gl.amt[k]) >> 6) * 127) >> 15;
    if (alt_cl_on(c) == GD_PROB)
        o += (((lfo * alt_cl[c].depth) >> 6) * 127) >> 15;
    return o;
}

/* a circle's RATE as a place in CL_RATE_SYNC (P_LSYNC OFF, the free rate: shown as 1/4) */
static uint32_t alt_cl_rate(int32_t sync)
{
    uint32_t k;
    for (k = 0; k < CL_NRATE; k++)
        if (CL_RATE_SYNC[k] == sync)
            return k;
    return CL_RATE_DEF;
}

/* the circle sources (mod.c mod_tsrc): s 0 POS, 1 FORCE of part t */
static int32_t alt_msrc_of(const track_t *t, uint32_t s)
{
    uint32_t i = (uint32_t)(t - trk);
    return i < 3u ? alt_msrc[i][s & 1u] : 0;
}
