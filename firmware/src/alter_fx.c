/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's effects (included by fx.c before events_block, so mix_part and mix_block see it; the UI reads the tables).
 *
 * The presets of the FX page (alter_fxpage.c):
 *   DIST    per circle: Felucca's DIST and INSERT of its part (P_DIST, P_ITYPE, P_IA P_IB P_IC, P_IMIX)
 *   DELAY   shared: Felucca's one delay (G_DTIME G_DFDBK G_DCOLOR G_DMIX); each circle its send (P_DLY)
 *   REVERB  shared: Felucca's one reverb (G_RTYPE G_RSIZE G_RDAMP); each circle its send (P_REV)
 *   MASTER  ours, the eight settings below (amx[]), after the buses, before Felucca's limiter (master_out):
 *           EQ    a DJ isolator: LOW (< ~250 Hz), MID, HIGH (> ~2.5 kHz), 64 = flat, 0 = the band gone (24 dB/oct
 *                 crossovers), 127 = +6 dB
 *           COMP  a bus glue: 4:1, peaks caught at once (its gain every 8 samples), the threshold falls as COMP rises (0 = off, 127 ~ -13 dB), a make-up up to ~+5 dB
 *           DUCK  how deep the pump goes (0 off .. -20 dB); SRC what pumps: the beat (1/4 1/8 1/2, on the grid: the
 *                 Analog Rytm's kick through the MIDI clock) ducks the three circles, a circle's hits (C1 C2 C3)
 *                 duck the two others; SHAPE how long it takes to come back (20 .. 400 ms)
 *           CLIP  a soft clipper (0 off .. ~2.5x into it), its top under the limiter's: the limiter stays idle
 * At FLAT (EQ 64, COMP 0, DUCK 0, CLIP 0) nothing runs: the output is bit for bit Felucca's.
 * The UI writes amx[] (bytes); the audio side reads it and writes the meters (am_gr, am_dk). */

enum { AMX_LOW, AMX_MID, AMX_HIGH, AMX_COMP, AMX_DUCK, AMX_SRC, AMX_SHAPE, AMX_CLIP, AMX_N };
enum { AMS_4, AMS_8, AMS_2, AMS_C1, AMS_C2, AMS_C3, AMS_N };
static const char *const AMS_NAME[AMS_N] = {"1/4", "1/8", "1/2", "CIRC 1", "CIRC 2", "CIRC 3"};
static const char *const AMX_NAME[AMX_N] = {"LOW", "MID", "HIGH", "COMP", "DUCK", "SRC", "SHAPE", "CLIP"};
static volatile uint8_t amx[AMX_N] = {64, 64, 64, 0, 0, AMS_4, 60, 0};
static volatile uint8_t am_gr;           /* the compressor's reduction now, 0..100 % */
static volatile uint8_t am_dk;           /* the duck's depth now, 0..100 % */

/* ---- the presets ------------------------------------------------------------------------------------------- */
typedef struct { const char *name; uint8_t dist, type, a, b, c, mix; } afx_dist_t;
typedef struct { const char *name; uint8_t time, fb, col, mix; } afx_dly_t;
typedef struct { const char *name; uint8_t type, size, damp; } afx_rev_t;
typedef struct { const char *name; uint8_t v[AMX_N]; } afx_mst_t;

/* DIST (levels measured: each within ~3 dB of the sound's own): P_DIST, the INSERT's TYPE (fx.c IT_*), A B C (SOFT..FUZZ: DRIVE TONE LEVEL, 112 = 0 dB; CRUSH: BITS RATE
 * LPF) and MIX. Row 0: the sound's own (its DIST, no insert) */
static const afx_dist_t AFX_DIST[] = {
    {"SOUND", 0, 0, 0, 0, 0, 0},
    {"WARM", 20, IT_SOFT, 40, 90, 108, 70},
    {"CRUNCH", 45, IT_HARD, 55, 80, 100, 85},
    {"FOLD", 10, IT_FOLD, 70, 100, 98, 100},
    {"FUZZ", 35, IT_FUZZ, 90, 70, 100, 100},
    {"SCREAM", 90, IT_FUZZ, 127, 60, 94, 100},
    {"BITS", 0, IT_CRUSH, 40, 70, 95, 100},
    {"DESTROY", 70, IT_CRUSH, 16, 30, 70, 100},
};
/* DELAY: TIME (params.c N_DTIME: 0 1/4 1 1/8 2 1/16 3 1/32 4 8T 5 16T 6 1/2 10 1/8D 11 1/16D 12 1/4D), FDBK (0..120),
 * COLOR (dark .. bright), its return level */
static const afx_dly_t AFX_DLY[] = {
    {"ECHO", 1, 60, 70, 90},             /* (Felucca's power-on) */
    {"SLAP", 3, 10, 100, 90},
    {"DUB", 10, 85, 45, 105},
    {"DUB LONG", 12, 95, 35, 105},
    {"TAPE", 0, 70, 55, 95},
    {"TRIPLET", 4, 60, 70, 95},
    {"16TH DOT", 11, 75, 90, 90},
    {"SPACE", 6, 105, 30, 95},
};
/* REVERB: TYPE (0 ROOM 1 SPRING 2 HALL), SIZE, DAMP */
static const afx_rev_t AFX_REV[] = {
    {"HALL", 2, 90, 60},                 /* (Felucca's power-on) */
    {"ROOM", 0, 30, 40},
    {"BIG ROOM", 0, 90, 60},
    {"METAL", 0, 127, 0},
    {"SPRING", 1, 60, 30},
    {"DARK SPRING", 1, 90, 100},
    {"BRIGHT HALL", 2, 100, 10},
    {"CAVE", 2, 127, 90},
};
/* MASTER: LOW MID HIGH COMP DUCK SRC SHAPE CLIP */
static const afx_mst_t AFX_MST[] = {
    {"FLAT", {64, 64, 64, 0, 0, AMS_4, 60, 0}},
    {"LIVE", {68, 64, 66, 40, 0, AMS_4, 60, 25}},
    {"GLUE", {64, 64, 64, 70, 0, AMS_4, 60, 15}},
    {"LOUD", {72, 60, 70, 95, 0, AMS_4, 60, 70}},
    {"PUMP 1/4", {64, 64, 64, 30, 80, AMS_4, 70, 20}},
    {"PUMP 1/8", {64, 64, 64, 30, 60, AMS_8, 35, 20}},
    {"DUCK C1", {64, 64, 64, 30, 90, AMS_C1, 50, 20}},
    {"DARK", {70, 58, 40, 40, 0, AMS_4, 60, 30}},
};
#define AFX_NDIST (sizeof AFX_DIST / sizeof AFX_DIST[0])
#define AFX_NDLY (sizeof AFX_DLY / sizeof AFX_DLY[0])
#define AFX_NREV (sizeof AFX_REV / sizeof AFX_REV[0])
#define AFX_NMST (sizeof AFX_MST / sizeof AFX_MST[0])

/* ---- the duck: what pumps, how far it is now ---------------------------------------------------------------- */
#define AM_NC 3u                          /* the circles (parts 0..2) */
static int32_t am_dg[AM_NC] = {32768, 32768, 32768};   /* each circle's gain this block, Q15 (mix_part) */
static uint32_t am_since[AM_NC] = {1u << 30, 1u << 30, 1u << 30};   /* samples since its last hit */
static uint32_t am_gstep, am_pos, am_p = 5512;   /* the grid (alter_play.c am_clock): sixteenths, samples into one */
static uint8_t am_play;
#define AM_DG(t) ((uint32_t)((t) - trk) < AM_NC ? am_dg[(t) - trk] : 32768)

static void am_hit(uint32_t c) { if (c < AM_NC) am_since[c] = 0; }   /* alter_play.c alt_on */
static void am_clock(uint32_t gstep, uint32_t pos, uint32_t p, uint32_t playing)
{
    am_gstep = gstep;
    am_pos = pos;
    am_p = p ? p : 1u;
    am_play = (uint8_t)(playing != 0);
}

/* the pump's depth t samples after its trigger, Q15: up in 3 ms, back over SHAPE (squared: a smooth return) */
static int32_t am_duck_env(uint32_t t)
{
    uint32_t a = 132u, r = (20u + (uint32_t)amx[AMX_SHAPE] * 3u) * 44u, u;
    if (t < a)
        return (int32_t)(t * 32768u / a);
    t -= a;
    if (t >= r)
        return 0;
    u = (r - t) * 32768u / r;
    return (int32_t)((u * u) >> 15);
}

/* once a block, after the events: each circle's gain */
static void am_duck_block(uint32_t n)
{
    uint32_t c, src = amx[AMX_SRC] % AMS_N, d = amx[AMX_DUCK];
    int32_t g = 32768, e = 0;
    for (c = 0; c < AM_NC; c++)
        if (am_since[c] < (1u << 30))
            am_since[c] += n;
    if (d) {
        if (src >= AMS_C1)
            e = am_duck_env(am_since[src - AMS_C1]);
        else if (am_play) {
            static const uint8_t DIV[3] = {4, 2, 8};    /* sixteenths a beat */
            uint32_t k = DIV[src], s = am_gstep ? am_gstep - 1u : 0u;
            e = am_duck_env((s % k) * am_p + am_pos);
        }
        g = 32768 - ((e * (int32_t)(d * 232u)) >> 15);  /* DUCK 127: down to 0.1 (-20 dB) */
    }
    for (c = 0; c < AM_NC; c++) {
        int32_t tg = src >= AMS_C1 && c == src - AMS_C1 ? 32768 : g;
        am_dg[c] += (tg - am_dg[c]) >> 1;               /* (half a block's way: the beat's wrap does not click) */
        if (tg == 32768 && am_dg[c] > 32760)
            am_dg[c] = 32768;
    }
    am_dk = (uint8_t)((32768 - g) * 100 / 32768);
}

/* ---- the master chain ------------------------------------------------------------------------------------- */
/* the EQ's crossovers: Linkwitz-Riley 24 dB/oct (two Butterworth state-variable filters each), at ~250 Hz and ~2.5 kHz.
 * A band at 0 is really gone (-24 dB an octave past its edge); the three summed at 64 are flat in level (an all-pass) */
#define AM_F_LO 1167                      /* 2 sin(pi f / FS), Q15: 250 Hz .. */
#define AM_F_HI 11560                     /* .. 2.5 kHz */
#define AM_Q 23170                        /* 1 / Q, Q14: Butterworth (sqrt 2) */
typedef struct { int32_t lp, bp; } am_svf_t;
static am_svf_t am_f[2][6];               /* per side: 250 Hz x 2 for the low, x 2 for the rest; 2.5 kHz x 2 (Q4 states) */
static inline void am_svf(am_svf_t *f, int32_t x, int32_t fc, int32_t *lp, int32_t *hp)
{
    int32_t h;
    f->lp += (int32_t)(((int64_t)f->bp * fc) >> 15);
    h = x - f->lp - (int32_t)(((int64_t)f->bp * AM_Q) >> 14);
    f->bp += (int32_t)(((int64_t)h * fc) >> 15);
    *lp = f->lp;
    *hp = h;
}
static int32_t am_g[3] = {4096, 4096, 4096};   /* the bands' gains now, Q12 */
static int32_t am_env, am_cg = 32768;     /* the compressor's detector, its gain now (Q15) */

static int am_on(void)
{
    return amx[AMX_LOW] != 64 || amx[AMX_MID] != 64 || amx[AMX_HIGH] != 64 || amx[AMX_COMP] || amx[AMX_CLIP] ||
           am_g[0] != 4096 || am_g[1] != 4096 || am_g[2] != 4096 || am_cg != 32768;
}

static int32_t am_band_g(uint32_t v)      /* 0 gone, 64 flat (4096), 127 +6 dB */
{
    return v <= 64u ? (int32_t)(v * v) : 4096 + (int32_t)(v - 64u) * 65;
}

static uint32_t am_isqrt(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x)
        b >>= 2;
    while (b) {
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
        b >>= 2;
    }
    return r;
}


/* COMP's gain for a detector level env, Q15, make-up included: 4:1 over the threshold, (thr / env) ^ 0.75 */
static int32_t am_comp_g(int32_t env, int32_t thr, int32_t mk)
{
    int32_t g = 32768;
    if (env > thr) {
        uint32_t q = (uint32_t)(((int64_t)thr << 15) / env), s = am_isqrt(q << 15), f = am_isqrt(s << 15);
        g = (int32_t)((s * f) >> 15);
    }
    return (int32_t)(((int64_t)g * mk) >> 12);
}

#define AM_SUB 8u                         /* COMP's gain is worked out every 8 samples (0.18 ms) */
/* the master's block: the wet bus folded into the dry mix, then EQ, COMP, CLIP (l, r: the mix; w: the wet bus,
 * emptied) */
static __attribute__((noinline)) void am_master(int32_t *l, int32_t *r, int32_t *w, uint32_t n)
{
    uint32_t i, j, ch, comp = amx[AMX_COMP], clip = amx[AMX_CLIP];
    int32_t g0[3], d[3], cg = am_cg, mk = 4096 + (int32_t)comp * 24, cdrv = 4096 + (int32_t)clip * 48;
    int32_t thr = 32767 - (int32_t)comp * 200, env = am_env, gmin = 32768;
    for (i = 0; i < 3u; i++) {                          /* the bands' gains glide (a quarter of the way a block) */
        int32_t tg = am_band_g(amx[AMX_LOW + i]);
        g0[i] = am_g[i];
        am_g[i] += (tg - am_g[i]) >> 2;
        if ((tg - am_g[i]) * (tg - am_g[i]) < 16)
            am_g[i] = tg;
        d[i] = (am_g[i] - g0[i]) / (int32_t)n;
    }
    for (j = 0; j < n; j += AM_SUB) {
        uint32_t e = j + AM_SUB < n ? j + AM_SUB : n;
        int32_t cg1, cd;
        for (i = j; i < e; i++) {
            int32_t x[2], a;
            x[0] = l[i] + w[i];
            x[1] = r[i] + w[i];
            w[i] = 0;
            for (ch = 0; ch < 2u; ch++) {               /* EQ: the three bands, each by its gain */
                am_svf_t *f = am_f[ch];
                int32_t v = clamp(x[ch], -(1 << 22), 1 << 22) * 16, lo, rest, mid, hi, t0, t1;
                am_svf(&f[0], v, AM_F_LO, &t0, &t1);
                am_svf(&f[1], t0, AM_F_LO, &lo, &rest);
                (void)rest;
                am_svf(&f[2], t1, AM_F_LO, &t0, &rest);
                am_svf(&f[3], rest, AM_F_HI, &t0, &t1);
                am_svf(&f[4], t0, AM_F_HI, &mid, &rest);
                am_svf(&f[5], t1, AM_F_HI, &rest, &hi);
                x[ch] = (int32_t)(((int64_t)lo * g0[0] + (int64_t)mid * g0[1] + (int64_t)hi * g0[2]) >> 16);
            }
            g0[0] += d[0];
            g0[1] += d[1];
            g0[2] += d[2];
            a = x[0] < 0 ? -x[0] : x[0];                /* COMP's detector: up at once, down over ~190 ms */
            if ((x[1] < 0 ? -x[1] : x[1]) > a)
                a = x[1] < 0 ? -x[1] : x[1];
            if (a > env)
                env = a;
            else
                env -= (env >> 13) + (env > 0);
            l[i] = x[0];
            r[i] = x[1];
        }
        cg1 = comp ? am_comp_g(env, thr, mk) : 32768;   /* this stretch's gain, from where the last one ended */
        if (cg1 * 4096 / mk < gmin)
            gmin = cg1 * 4096 / mk;
        if (cg1 < cg)                                   /* down: at once, on this stretch already (its peak in it) */
            cg = cg1;
        cd = (cg1 - cg) / (int32_t)(e - j);
        for (i = j; i < e; i++) {
            cg += cd;
            for (ch = 0; ch < 2u; ch++) {
                int32_t *p = ch ? &r[i] : &l[i], v = *p;
                if (cg != 32768)
                    v = (int32_t)(((int64_t)v * cg) >> 15);
                if (clip)                               /* CLIP: tanh at half scale (the limiter's LIM_T is above) */
                    v = softclip(clamp((int32_t)(((int64_t)v * cdrv) >> 12), -(1 << 20), 1 << 20) * 2) >> 1;
                *p = v;
            }
        }
        cg = cg1;
    }
    am_env = env;
    am_cg = cg;
    am_gr = (uint8_t)(comp ? (uint32_t)(32768 - (gmin > 32768 ? 32768 : gmin)) * 100u / 32768u : 0u);
}

/* the FX page's presets in use (the UI's: alter_fxpage.c, the scenes): 0..2 each circle's DIST, 3 DELAY, 4 REVERB,
 * 5 MASTER; afx_edit bit k: a knob moved since (the name shown with a *) */
enum { AFX_S_DLY = 3, AFX_S_REV, AFX_S_MST, AFX_NS };
static uint8_t afx_sel[AFX_NS];
static uint8_t afx_edit;
static struct {
    uint8_t open, hold, tab, row;    /* the FX page: open; opened by this hold (its release keeps it); the tab; MASTER's row */
} afp;
