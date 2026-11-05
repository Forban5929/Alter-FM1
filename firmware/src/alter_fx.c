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
typedef struct { const char *name; uint8_t time, fb, col, mix, mode, tape; } afx_dly_t;
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
/* DELAY: TIME (params.c N_DTIME: 0 1/4 1 1/8 2 1/16 10 1/8D; Romain 04/11: only these four), FDBK (0..120),
 * COLOR (dark .. bright), its return level, MODE (fx.c DM_*: ECHO, PING-PONG, GHOST), TAPE (fx.c: saturation).
 * SLAP, SPACE (like the dub), TRIPLET, 16TH DOT gone (04/11); DUB LONG was 1/4D: now 1/4, more feedback */
static const afx_dly_t AFX_DLY[] = {
    {"ECHO", 1, 60, 70, 90, 0, 15},       /* (the power-on) */
    {"DUB", 10, 85, 45, 105, 0, 45},
    {"DUB LONG", 0, 102, 35, 105, 0, 55},
    {"PING-PONG", 1, 75, 65, 100, 1, 20},
    {"GHOST", 2, 70, 50, 110, 2, 30},
    {"TAPE", 0, 75, 60, 95, 0, 110},
};
/* REVERB: TYPE (0 SHORT 1 GATED 2 HALL 3 CLOUD: fx.c RV_*), SIZE, DAMP (8 presets: the page's PRESETS) */
static const afx_rev_t AFX_REV[] = {
    {"HALL", 2, 90, 60},                 /* (Felucca's power-on) */
    {"BRIGHT HALL", 2, 100, 10},
    {"CLOUD", 3, 80, 50},
    {"ENDLESS", 3, 120, 70},             /* (SIZE 120: ~40 s, almost a freeze) */
    {"SHORT", 0, 60, 40},
    {"BOX", 0, 20, 90},
    {"GATED", 1, 30, 40},                /* (SIZE: the gate open 160 ms) */
    {"LONG GATE", 1, 90, 70},
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

/* ---- RUMBLE (Romain 04/11): the techno rumble in one knob, for the KICK sound -------------------------------------
 * The kick into a dark reverb (4 combs + 1 diffuser at 11 kHz, ~1 s, its own memory: not the shared reverb), the
 * reverb driven into a tanh (the distorted texture), low-passed (~180 Hz, two poles), and ducked by the kick itself
 * (an envelope of the dry kick: the rumble falls under each hit and swells back after it). Added to the circle's sound
 * after its DIST / INSERT, before its LEVEL (so the MIXER, the sends and the alignment take it as the kick's). One
 * unit: the first circle whose RUMBLE is above 0. 0 = nothing runs (the circle bit for bit as before) */
static volatile uint8_t alt_rmb[4];               /* each circle's RUMBLE 0..127 (alter_ui.c: the KICK's knob 4) */
#define RMB_N 4u
static const uint16_t RMB_LEN[RMB_N] = {331, 409, 457, 491};   /* (11.025 kHz: 30 .. 45 ms, no common factor) */
#define RMB_AP 113u
static int16_t rmb_buf[331 + 409 + 457 + 491] __attribute__((section(".pool")));
static int16_t rmb_apb[RMB_AP] __attribute__((section(".pool")));
static struct {
    uint16_t w[RMB_N], apw;
    int32_t lp[RMB_N], acc, l1, l2, env, prev, now, gn, pk;
    uint8_t ph, owner, live;
} rmb;
static int32_t rmb_tail;                          /* samples of tail left after the knob went to 0 or the kick stopped */
static uint8_t rmb_mute;                          /* (the browser build's tests: no rumble, web_alter op 18) */
static uint32_t rmb_who(void)                     /* the circle with the rumble, or 3 (none) */
{
    uint32_t c;
    if (rmb_mute)
        return 3u;
    for (c = 0; c < 3u; c++)
        if (alt_rmb[c])
            return c;
    return 3u;
}
static int rmb_busy(uint32_t c) { return c == rmb.owner && rmb_tail > 0; }
static __attribute__((noinline)) void rmb_part(uint32_t c, int32_t *b, uint32_t n)
{
    uint32_t i, k, amt = alt_rmb[c & 3u];
    int32_t g, fb = 27500;                        /* (each comb ~0.84: about a second at 11 kHz) */
    if (c != rmb_who()) {
        if (c == rmb.owner && rmb_tail > 0)
            amt = 0;                              /* (its tail runs out) */
        else
            return;
    }
    if (c != rmb.owner) {                         /* a new owner: from silence */
        uint32_t j;
        for (j = 0; j < sizeof rmb_buf / sizeof rmb_buf[0]; j++)
            rmb_buf[j] = 0;
        for (j = 0; j < RMB_AP; j++)
            rmb_apb[j] = 0;
        for (j = 0; j < sizeof rmb; j++)
            ((uint8_t *)&rmb)[j] = 0;
        rmb.owner = (uint8_t)c;
    }
    if (amt)
        rmb_tail = FS * 2;
    else if ((rmb_tail -= (int32_t)n) <= 0)
        return;
    g = (int32_t)amt * 40;                        /* (127: ~1.2 x the drive's output, then the duck) */
    {                                             /* the kick's level (a slow peak): the drive is relative to it, so
                                                   * the rumble follows the kick's LEVEL, whatever it is */
        int32_t m = 0;
        for (i = 0; i < n; i++) {
            int32_t a = b[i] >> 3;
            a = a < 0 ? -a : a;
            m = a > m ? a : m;
        }
        rmb.pk = m > rmb.pk ? m : rmb.pk - (rmb.pk >> 8);
    }
    {
    int32_t ref = rmb.pk + 64, inv = (int32_t)((1u << 28) / (uint32_t)ref);   /* (Q14 / ref, Q14 later) */
    for (i = 0; i < n; i++) {
        int32_t x = b[i] >> 3, a = x < 0 ? -x : x, y;
        /* the duck: the kick's envelope (fast up, ~60 ms down), the rumble pulled under it */
        if (a > rmb.env)
            rmb.env += (a - rmb.env) >> 3;
        else
            rmb.env -= (rmb.env >> 10) + ((rmb.env - a) >> 10);
        rmb.acc += x;
        if (++rmb.ph >= 4u) {                     /* every 4th sample: the reverb at 11 kHz */
            int32_t in = rmb.acc >> 2, s = 0, o = 0;
            uint32_t base = 0;
            rmb.ph = 0;
            rmb.acc = 0;
            for (k = 0; k < RMB_N; k++) {
                int16_t *ln = rmb_buf + base;
                int32_t v = ln[rmb.w[k]];
                rmb.lp[k] += (v - rmb.lp[k]) >> 2;   /* (dark: each echo duller) */
                ln[rmb.w[k]] = (int16_t)clamp(in + ((rmb.lp[k] * fb) >> 15), -32767, 32767);
                if (++rmb.w[k] >= RMB_LEN[k])
                    rmb.w[k] = 0;
                s += v;
                base += RMB_LEN[k];
            }
            s >>= 1;
            o = rmb_apb[rmb.apw];                    /* one diffuser */
            rmb_apb[rmb.apw] = (int16_t)clamp(s + (o >> 1), -32767, 32767);
            if (++rmb.apw >= RMB_AP)
                rmb.apw = 0;
            rmb.prev = rmb.now;
            rmb.now = o - (s >> 1);
        }
        y = rmb.prev + (((rmb.now - rmb.prev) * (int32_t)rmb.ph) >> 2);   /* (back to 44.1 kHz between samples) */
        y = (int32_t)(((int64_t)y * inv) >> 14);  /* (the tail over the kick's peak, Q14) */
        y = softclip(clamp(y * 3, -65535, 65535)); /* the drive: the texture */
        y = (int32_t)(((int64_t)y * ref) >> 15);  /* (back to the kick's scale) */
        rmb.l1 += (y - rmb.l1) >> 5;              /* ~180 Hz, two poles */
        rmb.l2 += (rmb.l1 - rmb.l2) >> 5;
        y = rmb.l2;
        {
            int32_t dk = (int32_t)(((int64_t)rmb.env * inv) >> 11);   /* (the kick's envelope over its peak, x 8) */
            dk = dk > 16384 ? 16384 : dk;          /* (an eighth of its peak or more: down to 6 %) */
            y = (int32_t)(((int64_t)y * (32768 - ((dk * 30720) >> 14))) >> 15);
        }
        rmb.gn += (g - rmb.gn) >> 9;              /* (the knob glides: no step, and a fade to 0) */
        b[i] += (int32_t)(((int64_t)y * rmb.gn) >> 6);
    }
    }
}

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

#include "alter_filter.c"                         /* the MASTER FILTER (before the MASTER block) */

/* the FX page's presets in use (the UI's: alter_fxpage.c, the scenes): 0..2 each circle's DIST, 3 DELAY, 4 REVERB,
 * 5 MASTER; afx_edit bit k: a knob moved since (the name shown with a *) */
enum { AFX_S_DLY = 3, AFX_S_REV, AFX_S_MST, AFX_NS };
static uint8_t afx_sel[AFX_NS];
static uint8_t afx_edit;
static struct {
    uint8_t open, hold, tab, row;    /* the FX page: open; opened by this hold (its release keeps it); the tab; MASTER's row */
    uint8_t ab;                      /* the FILTER tab: the snapshot the knobs edit (0 A, 1 B) */
} afp;

/* ---- the circles' level alignment (Romain, 01/11): after a random draw (CHAOS, TOTAL CHAOS, a DICE, MUTE 1+2+3) a
 * circle measures what it really gives (after DIST and INSERT, at its LEVEL) over its first hits, then its own gain
 * (apart from LEVEL: the player's VOLUME stays his) brings it to AC_TARGET and stays there. The measure: the RMS of
 * windows of 1024 samples (23 ms) while it sounds, the mean of the four loudest (so a sparse rhythm counts as a dense
 * one); done once it has 8 such windows and 4 s (a slow swell, a slow LFO), or after 8 s (silent: unchanged). Too loud on the way
 * (1.5 x the target): down at once. Within -18 .. +18 dB. Once frozen, a guard: a window over 2 x the target
 * (+6 dB, a slow swell the measure missed) lowers it, never up again. A sound chosen by hand (PRESETS, a scene) gets 0 dB again.
 * The UI asks through alt_cal_cmd (a gain Q15, 0 = measure) and alt_cal_cnt (it writes both, the audio side reads) */
#define AC_WIN 1024u
#define AC_TARGET 5600                    /* the RMS aimed at: the median of the 23 sounds, in the mix's units (tools/alter/alter_level.mjs) */
#define AC_UNITY 32768
#define AC_MIN 4129                       /* -18 dB */
#define AC_MAX 260200                     /* +18 dB */
static volatile int32_t alt_cal_cmd[AM_NC];
static volatile uint32_t alt_cal_cnt[AM_NC];
static uint32_t ac_seen[AM_NC];
static volatile int32_t ac_tg[AM_NC] = {AC_UNITY, AC_UNITY, AC_UNITY};   /* the gain it heads for (the UI reads it) */
static int32_t ac_g[AM_NC] = {AC_UNITY, AC_UNITY, AC_UNITY};             /* the gain now, Q15 (gliding) */
static volatile uint32_t ac_last[AM_NC];  /* the last measure (tests) */
static struct {
    uint8_t on, nw;                       /* measuring; windows with sound */
    uint8_t guard;                        /* after a random draw: louder than 2 x the target (+6 dB) lowers it, for good */
    uint32_t t, wn;                       /* samples since it began; samples in the window */
    uint64_t e;                           /* the window's energy */
    uint32_t top[4];                      /* the four loudest windows' RMS */
} ac[AM_NC];
#define AC_G(t) ((uint32_t)((t) - trk) < AM_NC ? ac_g[(t) - trk] : AC_UNITY)

static void ac_poll(uint32_t c, uint32_t bn)   /* once a block of bn samples, every circle (sounding or not) */
{
    uint32_t n = alt_cal_cnt[c];
    if (n != ac_seen[c]) {
        int32_t g = alt_cal_cmd[c];
        ac_seen[c] = n;
        if (g) {
            ac_tg[c] = clamp(g, AC_MIN, AC_MAX);
            ac[c].on = ac[c].guard = 0;
        } else {
            ac[c].on = ac[c].guard = 1;
            ac[c].nw = 0;
            ac[c].t = ac[c].wn = 0;
            ac[c].e = 0;
            ac[c].top[0] = ac[c].top[1] = ac[c].top[2] = ac[c].top[3] = 0;
        }
    }
    if (ac[c].on && (ac[c].t += bn) >= FS * 8u) {   /* 8 s: what it has (none: unchanged) */
        ac[c].on = 0;
        if (ac[c].nw) {
            uint32_t L = (ac[c].top[0] + ac[c].top[1] + ac[c].top[2] + ac[c].top[3]) / (ac[c].nw < 4u ? ac[c].nw : 4u);
            ac_last[c] = L;
            ac_tg[c] = clamp((int32_t)((int64_t)AC_TARGET * AC_UNITY / (L | 1u)), AC_MIN, AC_MAX);
        }
    }
    ac_g[c] += (ac_tg[c] - ac_g[c]) >> 3;  /* (glides over ~8 blocks: no click) */
    if ((ac_tg[c] - ac_g[c]) * (ac_tg[c] - ac_g[c]) < 64)
        ac_g[c] = ac_tg[c];
}

static void ac_measure(uint32_t c, const int32_t *b, int32_t lvl, uint32_t n)   /* mix_part: the circle's block */
{
    uint32_t i;
    afl_take(c, b, lvl, n);                             /* (the MASTER FILTER's sidechain: alter_filter.c) */
    if (c >= AM_NC || !(ac[c].on | ac[c].guard))
        return;
    for (i = 0; i < n; i++) {
        int32_t x = (((b[i] >> 2) * lvl) >> 10) >> 2;
        ac[c].e += (uint64_t)((int64_t)x * x);
    }
    if ((ac[c].wn += n) >= AC_WIN) {
        uint32_t r = am_isqrt((uint32_t)(ac[c].e / ac[c].wn)) * 4u, k, j;
        ac[c].wn = 0;
        ac[c].e = 0;
        if (!ac[c].on) {                  /* frozen: only the guard (never up again: the hits keep their nuances) */
            if ((int64_t)r * ac_g[c] > (int64_t)AC_TARGET * 2 * AC_UNITY)
                ac_tg[c] = ac_g[c] = clamp((int32_t)((int64_t)AC_TARGET * 2 * AC_UNITY / r), AC_MIN, AC_MAX);
            return;
        }
        if (r > 40u) {                    /* (-58 dB: sound, not a tail's last breath) */
            if (ac[c].nw < 255u)
                ac[c].nw++;
            for (k = 0; k < 4u && r <= ac[c].top[k]; k++)
                ;
            for (j = 3u; k < 4u && j > k; j--)
                ac[c].top[j] = ac[c].top[j - 1u];
            if (k < 4u)
                ac[c].top[k] = r;
            if ((int64_t)r * ac_g[c] > (int64_t)AC_TARGET * 3 / 2 * AC_UNITY)   /* too loud: down now */
                ac_tg[c] = ac_g[c] = clamp((int32_t)((int64_t)AC_TARGET * AC_UNITY / r), AC_MIN, AC_MAX);
            if (ac[c].nw >= 8u && ac[c].t >= FS * 4u)
                ac[c].t = FS * 8u;        /* enough: done at the next poll */
        }
    }
}
#include "alter_console.c"                              /* ALTER: each circle's CONSOLE strip (EQ, COMP) */
