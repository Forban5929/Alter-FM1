/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's LFOs (included by mod.c, after its MS_* / MD_* lists: mod.c reads them in the audio ISR; alter_play.c
 * counts the time once per block, alter_ui.c edits the settings and draws them). Ten LFOs, each a simple one: WAVE
 * (SIN TRI SAW SQR S&H SLEW, RAMP+ RAMP- from 0 to full), RATE (a note value of the tempo, 1/32 .. 8 bars), DEPTH
 * (-64..63: AMT 64 = the whole range, as the matrix's) and one DEST, the same list for every engine:
 *   PITCH (+-11.8 semitones at full), SHAPE and FILTER (the engines' m->shape / m->cutoff: on ALTER MORPH and
 *   HARMONICS), LEVEL (a gain, as AMP), the DIST DELAY REVERB sends, PAN, PROB (the circle's chance, added to PROB).
 * Three per circle (alt_cl[c][0..2]: LFO 1 2 3 of the circle chosen on the page), and the GLOBAL (alt_gl, its first
 * slot) for the three circles together. A destination the GLOBAL reaches is taken from the circle's own LFOs (their
 * DEST there, the part's P_LD_* routing there, its matrix slots fed by its LFO or its S&H / SLEW there).
 * All run on one clock: alt_gl_n, the samples since PLAY (stopped, it runs on), in whole sixteenths (alt_lfo_p
 * samples each), so a 1-bar LFO starts again on every bar line with the circles' steps; S&H and SLEW draw their value
 * from the cycle's number (alt_lfo_rnd), so the screen draws exactly what plays.
 * The matrix (MTX) keeps Felucca's four slots per circle; there its LFO is the circle's LFO 1 (S&H / SLEW on its
 * cycles), and two sources of ours:
 *   POS: where circle i is in its cycle, 0 at its first step .. 1 at the end of its last (0 stopped).
 *   FORCE: the force of circle i's last hit, 0 ghost, 0.5 normal, 1 accent (held until the next hit). */

enum { GW_SIN, GW_TRI, GW_SAW, GW_SQR, GW_SH, GW_SLEW, GW_RUP, GW_RDN, GW_N };
enum { GD_OFF, GD_PITCH, GD_SHAPE, GD_FILT, GD_LEVEL, GD_DIST, GD_DLY, GD_REV, GD_PAN, GD_PROB, GD_ELEN, GD_EATK,
       GD_EDEC, GD_ESUS, GD_N };   /* (GD_E*: the envelope, by the circle's mode: at each hit, alter_play.c) */
#define GL_NSLOT 4
#define CL_N 3                                   /* LFOs a circle */
static const char *const GL_WAVE_NAME[GW_N] = {"SIN", "TRI", "SAW", "SQR", "S&H", "SLEW", "RAMP+", "RAMP-"};
static const char *const GL_DST_NAME[GD_N] = {"OFF", "PITCH", "SHAPE", "FILTER", "LEVEL", "DIST", "DELAY", "REVERB",
                                              "PAN", "PROB", "LENGTH", "ATTACK", "DECAY", "SUS/REL"};
/* the RATEs, fast to slow: a cycle in sixths of a sixteenth */
#define GL_NRATE 11
#define GL_RATE_DEF 7                            /* 1 bar */
#define CL_RATE_DEF 5                            /* 1/4 */
#define GL_BAR_6TH 96u                           /* a bar */
static const char *const GL_RATE_NAME[GL_NRATE] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/2", "1 BAR",
                                                   "2 BAR", "4 BAR", "8 BAR"};
static const uint16_t GL_RATE_6TH[GL_NRATE] = {3, 4, 6, 8, 12, 24, 48, 96, 192, 384, 768};
/* a destination's matrix destination (GD_PROB: none, alter_play.c) */
static const uint8_t GL_MD[GD_N] = {MD_OFF, MD_PITCH, MD_SHP, MD_CUT, MD_AMP, MD_DIST, MD_DLY, MD_REV, MD_PAN, MD_OFF, MD_OFF,
                                    MD_OFF, MD_OFF, MD_OFF};

typedef struct {
    uint8_t wave, rate;                          /* GW_*, 0 .. GL_NRATE - 1 */
    uint8_t dst[GL_NSLOT];                       /* GD_* (the screen edits the first) */
    int8_t amt[GL_NSLOT];                        /* -64 .. 63 */
} alt_gl_t;
typedef struct {
    uint8_t wave, rate, dst;                     /* GW_*, the RATE, GD_* */
    int8_t depth;                                /* -64 .. 63 */
} alt_cl_t;

static volatile alt_gl_t alt_gl = {GW_SIN, GL_RATE_DEF, {0}, {0}};   /* UI: the settings (bytes) */
static volatile alt_cl_t alt_cl[3][CL_N];        /* UI: each circle's three */
static volatile uint32_t alt_gl_n;               /* here: the clock, samples since PLAY .. */
static volatile uint32_t alt_lfo_p = 5512;       /* .. and a sixteenth's samples */
static int32_t alt_gl_val;                       /* here: the GLOBAL now, Q15 (bipolar; the ramps 0..32767) */
static int32_t alt_cl_val[3][CL_N];              /* here: each circle LFO now */
static int16_t alt_msrc[3][2];                   /* here: each circle's POS and FORCE, Q15 0..32767 */

static void alt_gl_reset(void)
{
    uint32_t k, c;
    alt_gl.wave = GW_SIN;
    alt_gl.rate = GL_RATE_DEF;
    for (k = 0; k < GL_NSLOT; k++) {
        alt_gl.dst[k] = GD_OFF;
        alt_gl.amt[k] = 0;
    }
    for (c = 0; c < 3u; c++)
        for (k = 0; k < CL_N; k++) {
            alt_cl[c][k].wave = GW_SIN;
            alt_cl[c][k].rate = CL_RATE_DEF;
            alt_cl[c][k].dst = GD_OFF;
            alt_cl[c][k].depth = 0;
        }
}

static int alt_wave_uni(uint32_t w) { return w == GW_RUP || w == GW_RDN; }

/* cycle n's random value of LFO seed (S&H, SLEW), bipolar Q15 */
static int32_t alt_lfo_rnd(uint32_t n, uint32_t seed)
{
    uint32_t x = (n + 1u) * 0x9E3779B9u ^ (seed * 0x85EBCA6Bu + 0x51ED270Bu);
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    x *= 0x297A2D39u;
    x ^= x >> 15;
    return (int32_t)(x >> 16) - 32768;
}

/* an LFO of wave w, rate r (GL_RATE_*), random seed at clock time n (samples since PLAY; p a sixteenth): its value,
 * Q15 (the ramps 0..32767); *ph its phase */
static int32_t alt_lfo_at(uint32_t w, uint32_t r, uint32_t seed, uint32_t n, uint32_t p, uint32_t *ph)
{
    uint32_t per = p * GL_RATE_6TH[r % GL_NRATE] / 6u, cyc, f;
    int32_t u, v;
    if (!per)
        per = 1;
    cyc = n / per;
    f = (uint32_t)(((uint64_t)(n - cyc * per) << 32) / per);
    if (ph)
        *ph = f;
    u = (int32_t)(f >> 17);                      /* 0 .. 32767 across the cycle */
    switch (w % GW_N) {
    case GW_SIN: v = sine_i(f); break;
    case GW_TRI: v = u < 16384 ? u * 4 - 32767 : (32767 - u) * 4 - 32767; break;
    case GW_SAW: v = u * 2 - 32767; break;
    case GW_SQR: v = f < 0x80000000u ? 32767 : -32767; break;
    case GW_SH: v = alt_lfo_rnd(cyc, seed); break;
    case GW_SLEW: {
        int32_t a = alt_lfo_rnd(cyc, seed), b = alt_lfo_rnd(cyc + 1u, seed);
        v = a + (((b - a) * u) >> 15);
        break;
    }
    case GW_RUP: v = u; break;
    default: v = 32767 - u; break;
    }
    return v < -32767 ? -32767 : v > 32767 ? 32767 : v;
}

/* the GLOBAL has destination g with an amount */
static int alt_gl_has(uint32_t g) { return g && alt_gl.dst[0] == g && alt_gl.amt[0]; }

/* the matrix destinations the GLOBAL holds now (bit MD_*): 0 = it rests */
static uint32_t alt_gl_take(void)
{
    uint32_t g = alt_gl.dst[0];
    return alt_gl.amt[0] && g < GD_N && GL_MD[g] ? 1u << GL_MD[g] : 0u;
}

/* LFO j of circle c acts now: its destination (GD_*), 0 = none (off, no depth, or the GLOBAL leads there) */
static uint32_t alt_cl_on(uint32_t c, uint32_t j)
{
    uint32_t g;
    if (c >= 3u || j >= CL_N)
        return 0;
    g = alt_cl[c][j].dst;
    if (!g || g >= GD_N || !alt_cl[c][j].depth || alt_gl_has(g))
        return 0;
    return g;
}

/* circle c has an LFO acting on a matrix destination */
static int alt_cl_any(uint32_t c)
{
    uint32_t j;
    for (j = 0; j < CL_N; j++)
        if (GL_MD[alt_cl_on(c, j)])
            return 1;
    return 0;
}

/* the PROB offset of circle c (0 = none): the GLOBAL's and its own LFOs'; AMT 64 at full = +-127 */
static int32_t alt_prob_of(uint32_t c)
{
    uint32_t j;
    int32_t o = 0;
    if (alt_gl_has(GD_PROB))
        o += (((alt_gl_val * alt_gl.amt[0]) >> 6) * 127) >> 15;
    for (j = 0; j < CL_N; j++)
        if (alt_cl_on(c, j) == GD_PROB)
            o += (((alt_cl_val[c][j] * alt_cl[c][j].depth) >> 6) * 127) >> 15;
    return o;
}

/* the matrix's LFO, S&H and SLEW of part t (circle 0..2: its LFO 1; mod.c mod_tsrc): s MS_LFO / MS_SH / MS_SLEW */
static int32_t alt_mlfo(const track_t *t, uint32_t s)
{
    uint32_t c = (uint32_t)(t - trk);
    if (s == MS_LFO)
        return alt_cl_val[c][0];
    return alt_lfo_at(s == MS_SH ? GW_SH : GW_SLEW, alt_cl[c][0].rate, c * 3u, alt_gl_n, alt_lfo_p, 0);
}

/* the circle sources (mod.c mod_tsrc): s 0 POS, 1 FORCE of part t */
static int32_t alt_msrc_of(const track_t *t, uint32_t s)
{
    uint32_t i = (uint32_t)(t - trk);
    return i < 3u ? alt_msrc[i][s & 1u] : 0;
}

/* the ALTER LFOs acting on part t (a circle): k 0 the GLOBAL, 1..3 the circle's LFO 1 2 3. Its matrix destination
 * (MD_*, 0 = none: off, no depth, PROB, or the GLOBAL leads there), its amount *a, its value now *x, and the source
 * kind *s the matrix reads it as (a ramp is 0..1: MS_ENV, as AMP takes it) */
static uint32_t alt_lfo_k(const track_t *t, uint32_t k, int32_t *a, int32_t *x, uint32_t *s)
{
    uint32_t c = (uint32_t)(t - trk), g, w;
    if (c >= 3u)
        return 0;
    if (k == 0) {
        g = alt_gl.dst[0];
        *a = alt_gl.amt[0];
        *x = alt_gl_val;
        w = alt_gl.wave;
        if (!*a)
            return 0;
    } else {
        g = alt_cl_on(c, k - 1u);
        *a = alt_cl[c][k - 1u].depth;
        *x = alt_cl_val[c][k - 1u];
        w = alt_cl[c][k - 1u].wave;
    }
    *s = alt_wave_uni(w) ? MS_ENV : MS_LFO;
    return g < GD_N ? GL_MD[g] : 0u;
}


/* ---- the envelope modes' own settings (alter_play.c AM_*: FREE PERC STAB DRONE), per circle, 0..127 each --------
 * PERC  ATTACK (short) HOLD DECAY PUNCH: an AD with a hold, after the Analog Rytm; PUNCH dives the pitch at each hit
 * STAB  LENGTH (decay and release together, never long) SUSTAIN PLUCK (the timbre opens at the hit) + the CHORD
 * DRONE SWELL (never short) BODY (the sustain the decay falls into) TAIL (never too short) OPEN (the timbre opens with
 *       the swell). A note still sounding is never played again (alter_play.c): no restrike, no click; PHYS's MODAL and
 *       STRNG are bowed (BOW) instead of struck */
enum { AE_PATK, AE_PHOLD, AE_PDEC, AE_PPUNCH, AE_SLEN, AE_SSUS, AE_SPLUCK, AE_DSWELL, AE_DBODY, AE_DTAIL, AE_DOPEN,
       AE_N };
static const uint8_t AE_DEF[AE_N] = {10, 20, 60, 0, 50, 25, 30, 70, 90, 80, 40};
static volatile uint8_t alt_envp[3][AE_N];       /* UI: each circle's */
static volatile uint8_t alt_ems[3];              /* here: each circle's mode (alter_play.c mirrors alt_emode) */
static int32_t alt_punch_lvl[3];                 /* here: PUNCH's dive now, Q15 (1 at a hit, falling) */

static void alt_env_default(uint32_t c)
{
    uint32_t k;
    for (k = 0; k < AE_N; k++)
        alt_envp[c % 3u][k] = AE_DEF[k];
}

/* the mode adds to part t's block (mod.c mod_global): 1 when PUNCH, PLUCK or OPEN is on */
static int alt_env_mod(uint32_t c)
{
    if (c >= 3u)
        return 0;
    return (alt_ems[c] == 1 && alt_envp[c][AE_PPUNCH] && alt_punch_lvl[c] > 64) ||
           (alt_ems[c] == 2 && alt_envp[c][AE_SPLUCK]) || (alt_ems[c] == 3 && alt_envp[c][AE_DOPEN]);
}
