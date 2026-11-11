/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's MASTER FILTER, simple (v0.51, Romain 10/11: Felucca's kind of filter, the A / B MORPH kept; the first one,
 * after a Sherman Filterbank, is kept as an idea in docs/alter/idees/filtre-sherman, branch idee-filtre-sherman: ~21 % of the
 * sound's CPU). Included by alter_fx.c; fx.c calls afl_block before the MASTER (EQ, COMP, CLIP, limiter after it). On the
 * whole mix, the echoes too (the wet bus folded in). Stereo. No REC, no random draw ever touches it.
 *
 *   in -> DRIVE -> the filter (FREQ, RES, TYPE, SLOPE; MOD, FM and MODTYP move it) -> MIX (dry / wet) -> the MASTER
 *
 * One TPT state-variable stage (A. Simper's), as Felucca's voices have, worked out once a block (32 samples): a few
 * multiply-adds a sample and a side. What is off costs nothing: DRIVE 0 (no tanh, no DC blocker), SLOPE 12 dB (one
 * stage), MOD / MODTYP at 64 and FM 0 (no follower), the bands TYPE leaves at 0, MIX full (no dry blend), MIX 0 (nothing).
 * DRIVE   0..+30 dB into a tanh, a little asymmetric, the loudness kept; past the middle a hard clip mixed in. A DC blocker.
 * FREQ    20 Hz .. 18 kHz, exponential (AFL_G: tan(pi f / FS), Q24)
 * RES     k = 2 .. 0.12: a strong peak, never oscillating by itself (no saturation to hold it: a simple filter)
 * TYPE    LP (0) -> BP (64) -> HP (127), a crossfade of the three outputs
 * SLOPE   12 dB / 24 dB (a plain Butterworth stage first, crossfaded in)
 * MOD     the cutoff moved by an envelope follower (one value a block), up to +-3 octaves; SRC what it hears
 * FM      the cutoff shaken by the source (every 8 samples while FM is up: the only per-8 work), up to +-2 octaves
 * MIX     dry / wet, equal power; DETUNE: the left cutoff down, the right up; MODTYP: the follower moves TYPE too
 * Two snapshots A and B of the eleven; MORPH (0 A .. 127 B) blends them (SRC and SLOPE switch at the middle). */

enum { AFL_FREQ, AFL_RES, AFL_TYPE, AFL_DRIVE, AFL_MOD, AFL_SRC, AFL_FM, AFL_MIX, AFL_SLOPE, AFL_DET, AFL_MTYP, AFL_N };
enum { AFS_MIX, AFS_C1, AFS_C2, AFS_C3, AFS_4, AFS_8, AFS_N };
static const char *const AFS_NAME[AFS_N] = {"MIX", "CIRC 1", "CIRC 2", "CIRC 3", "1/4", "1/8"};
/* the knobs, three rows of four (the 12th: MORPH) */
static const char *const AFL_NAME[AFL_N + 1] = {"FREQ", "RES", "TYPE", "DRIVE", "MOD", "SRC", "FM", "MIX",
                                                "SLOPE", "DETUNE", "MODTYP", "MORPH"};
#define AFL_MORPH AFL_N
typedef struct { const char *name; uint8_t v[AFL_N]; } afl_pre_t;
/* FREQ TYPE: 13 = 40 Hz, 51 = 300 Hz, 73 = 1 kHz, 81 = 1.5 kHz, 94 = 3 kHz, 112 = 8 kHz, 119 = 12 kHz;
 * MOD DETUNE MODTYP: 64 = none */
static const afl_pre_t AFL_PRE[] = {
    /*                FREQ RES TYPE DRV MOD SRC      FM  MIX SLP DET MTY */
    {"OFF",          {127, 0,   0,   0, 64, AFS_MIX, 0,   0, 0, 64, 64}},
    {"WARM",         {119, 20,  0,  30, 64, AFS_MIX, 0, 127, 0, 64, 64}},
    {"TAPE",         {112, 12,  0,  55, 64, AFS_MIX, 0, 127, 0, 64, 64}},
    {"RUMBLE CUT",   {13,  30, 127,  0, 64, AFS_MIX, 0, 127, 1, 64, 64}},
    {"GRIT",         {108, 25,  0,  92, 64, AFS_MIX, 0, 127, 0, 64, 64}},
    {"RADIO",        {81,  40, 64,  80, 64, AFS_MIX, 0, 127, 1, 64, 64}},
    {"DARK PUMP",    {81,  45,  0,  25, 104, AFS_4,  0, 127, 1, 64, 64}},
    {"ENV WAH",      {76, 104, 22,  70, 98, AFS_MIX, 20, 127, 1, 64, 64}},
    {"PEAK",         {60, 127,  0, 100, 80, AFS_MIX, 0, 110, 0, 84, 64}},
};
#define AFL_NPRE (sizeof AFL_PRE / sizeof AFL_PRE[0])

static volatile uint8_t afl[2][AFL_N] = {{127, 0, 0, 0, 64, AFS_MIX, 0, 0, 0, 64, 64},
                                         {127, 0, 0, 0, 64, AFS_MIX, 0, 0, 0, 64, 64}};
static volatile uint8_t afl_morph;       /* 0 = A .. 127 = B */
static uint8_t afl_sel[2], afl_edit;     /* each snapshot's preset (the UI's), bit s: a knob moved it since */
static volatile int32_t afl_now = -1;    /* the cutoff heard now (left: modulation included), AFL_G's index Q8; -1 off */
static volatile int32_t afl_tnow;        /* and the TYPE heard now, Q15 */
static volatile int32_t afl_peak;        /* (the tests: the input's peak over the last block, mix units) */

#define AFL_OCT 6678                     /* an octave in AFL_G's index Q8 (256 / log2(900) x 256) */
#define AFL_TOP (256 << 8)
#define AFL_EREF 32768                   /* the follower's full swing (mix units: the factory scenes peak ~28000) */
#define AFL_SUB 8u                       /* the cutoff is worked out every 8 samples */
#define AFL_K1 23726566                  /* the 24 dB slope's first stage: sqrt 2, Q24 (Butterworth) */

/* the tables (tools: f = 20 x 900^(i/256) Hz; AFL_G tan(pi f / 44100) Q24; AFL_DRV DRIVE's gain Q12, 30 dB x (v/127)^1.5) */
static const int32_t AFL_G[257] = {
    23903, 24547, 25208, 25887, 26584, 27300, 28035, 28790,
    29565, 30361, 31179, 32019, 32881, 33766, 34675, 35609,
    36568, 37553, 38564, 39602, 40669, 41764, 42889, 44044,
    45230, 46448, 47698, 48983, 50302, 51656, 53047, 54476,
    55943, 57449, 58996, 60585, 62216, 63892, 65612, 67379,
    69193, 71057, 72970, 74935, 76953, 79025, 81153, 83338,
    85583, 87887, 90254, 92684, 95180, 97743, 100375, 103078,
    105854, 108705, 111632, 114638, 117725, 120895, 124151, 127494,
    130927, 134453, 138074, 141792, 145611, 149532, 153559, 157694,
    161940, 166301, 170780, 175379, 180102, 184952, 189933, 195048,
    200301, 205695, 211235, 216923, 222765, 228765, 234926, 241253,
    247750, 254422, 261275, 268311, 275538, 282959, 290580, 298406,
    306443, 314697, 323174, 331878, 340817, 349998, 359425, 369107,
    379050, 389260, 399746, 410515, 421574, 432931, 444594, 456572,
    468873, 481506, 494479, 507803, 521485, 535537, 549969, 564789,
    580010, 595641, 611695, 628182, 645114, 662503, 680362, 698704,
    717541, 736888, 756757, 777164, 798123, 819648, 841757, 864463,
    887784, 911737, 936338, 961607, 987560, 1014217, 1041597, 1069720,
    1098607, 1128278, 1158755, 1190062, 1222219, 1255252, 1289185, 1324042,
    1359850, 1396635, 1434424, 1473246, 1513130, 1554105, 1596203, 1639455,
    1683894, 1729554, 1776471, 1824679, 1874216, 1925121, 1977433, 2031194,
    2086445, 2143230, 2201594, 2261585, 2323250, 2386641, 2451807, 2518804,
    2587687, 2658514, 2731344, 2806240, 2883266, 2962490, 3043980, 3127810,
    3214054, 3302792, 3394105, 3488078, 3584801, 3684366, 3786871, 3892416,
    4001109, 4113061, 4228388, 4347213, 4469665, 4595879, 4725997, 4860169,
    4998554, 5141319, 5288641, 5440705, 5597711, 5759869, 5927401, 6100546,
    6279555, 6464701, 6656269, 6854570, 7059934, 7272716, 7493297, 7722088,
    7959534, 8206114, 8462347, 8728798, 9006079, 9294859, 9595868, 9909905,
    10237848, 10580663, 10939415, 11315282, 11709572, 12123742, 12559415, 13018413,
    13502783, 14014834, 14557186, 15132816, 15745129, 16398036, 17096053, 17844421,
    18649264, 19517776, 20458479, 21481532, 22599157, 23826186, 25180802, 26685545,
    28368707, 30266289, 32424833, 34905583, 37790807, 41193674, 45274276, 50266798,
    56528008};
static const uint16_t AFL_HZ[257] = {
    20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27,
    28, 28, 29, 30, 31, 31, 32, 33, 34, 35, 36, 37,
    38, 39, 40, 41, 42, 43, 44, 46, 47, 48, 49, 51,
    52, 53, 55, 56, 58, 59, 61, 63, 64, 66, 68, 70,
    72, 74, 76, 78, 80, 82, 84, 86, 89, 91, 93, 96,
    98, 101, 104, 107, 110, 112, 116, 119, 122, 125, 128, 132,
    135, 139, 143, 147, 151, 155, 159, 163, 168, 172, 177, 181,
    186, 191, 197, 202, 207, 213, 219, 224, 231, 237, 243, 250,
    256, 263, 270, 278, 285, 293, 301, 309, 317, 326, 334, 343,
    353, 362, 372, 382, 392, 403, 414, 425, 436, 448, 460, 472,
    485, 498, 512, 525, 539, 554, 569, 584, 600, 616, 633, 650,
    667, 685, 704, 723, 742, 762, 783, 804, 825, 848, 870, 894,
    918, 943, 968, 994, 1021, 1048, 1077, 1106, 1135, 1166, 1197, 1230,
    1263, 1297, 1332, 1367, 1404, 1442, 1481, 1521, 1562, 1604, 1647, 1691,
    1737, 1784, 1832, 1881, 1932, 1984, 2037, 2092, 2148, 2206, 2265, 2326,
    2389, 2453, 2519, 2587, 2657, 2729, 2802, 2877, 2955, 3035, 3116, 3200,
    3286, 3375, 3466, 3559, 3655, 3753, 3854, 3958, 4065, 4174, 4287, 4402,
    4521, 4642, 4767, 4896, 5027, 5163, 5302, 5445, 5591, 5742, 5896, 6055,
    6218, 6386, 6558, 6734, 6916, 7102, 7293, 7489, 7691, 7898, 8111, 8329,
    8554, 8784, 9021, 9263, 9513, 9769, 10032, 10302, 10580, 10865, 11157, 11458,
    11766, 12083, 12408, 12742, 13086, 13438, 13800, 14171, 14553, 14945, 15347, 15761,
    16185, 16621, 17068, 17528, 18000};
static const uint32_t AFL_DRV[128] = {
    4096, 4106, 4124, 4148, 4176, 4208, 4244, 4283, 4326, 4372, 4421, 4473,
    4528, 4587, 4648, 4712, 4780, 4851, 4925, 5002, 5083, 5167, 5254, 5345,
    5440, 5538, 5640, 5746, 5857, 5971, 6089, 6212, 6340, 6472, 6609, 6751,
    6898, 7051, 7209, 7373, 7542, 7718, 7900, 8089, 8284, 8487, 8696, 8914,
    9139, 9372, 9614, 9865, 10124, 10393, 10672, 10961, 11261, 11571, 11893, 12227,
    12573, 12932, 13305, 13691, 14092, 14507, 14939, 15386, 15851, 16333, 16833, 17353,
    17892, 18452, 19034, 19638, 20265, 20917, 21595, 22298, 23030, 23790, 24580, 25402,
    26256, 27145, 28069, 29030, 30029, 31070, 32152, 33279, 34451, 35672, 36942, 38265,
    39643, 41078, 42573, 44130, 45753, 47444, 49206, 51043, 52957, 54954, 57035, 59206,
    61471, 63833, 66297, 68868, 71552, 74352, 77276, 80328, 83514, 86842, 90317, 93947,
    97739, 101701, 105841, 110168, 114690, 119417, 124359, 129527};
/* DRIVE's loudness kept: its gain x 0.85 taken back (measured on the factory scenes), Q12 */
static const uint16_t AFL_MK[128] = {
    4096, 4088, 4072, 4053, 4029, 4003, 3974, 3943, 3910, 3875, 3839, 3801,
    3761, 3721, 3679, 3636, 3592, 3547, 3502, 3456, 3409, 3362, 3315, 3267,
    3218, 3170, 3121, 3072, 3023, 2973, 2924, 2875, 2826, 2776, 2727, 2679,
    2630, 2581, 2533, 2485, 2438, 2390, 2344, 2297, 2251, 2205, 2160, 2115,
    2071, 2027, 1983, 1940, 1898, 1856, 1815, 1774, 1734, 1694, 1655, 1617,
    1579, 1541, 1505, 1469, 1433, 1398, 1364, 1330, 1297, 1264, 1232, 1201,
    1170, 1140, 1110, 1081, 1052, 1024, 997, 970, 944, 918, 893, 868,
    844, 821, 798, 775, 753, 732, 711, 690, 670, 651, 632, 613,
    595, 577, 560, 543, 527, 511, 495, 480, 465, 451, 437, 423,
    410, 397, 384, 372, 360, 349, 337, 326, 316, 305, 295, 286,
    276, 267, 258, 250, 241, 233, 225, 217};

/* the snapshots blended by MORPH: one setting as it plays (0..127) */
static int32_t afl_p(uint32_t k)
{
    int32_t a = afl[0][k], b = afl[1][k], m = afl_morph;
    if (k == AFL_SRC || k == AFL_SLOPE)
        return m < 64 ? a : b;
    return a + (b - a) * m / 127;
}
/* a scene keeps the filter when one is on (either snapshot heard) */
static int afl_active(void) { return afl[0][AFL_MIX] || afl[1][AFL_MIX]; }

/* ---- the audio side ---------------------------------------------------------------------------------------- */
typedef struct { int32_t ic1, ic2; } afl_svf_t;
static struct {
    afl_svf_t s[2][2];                   /* [side][stage] */
    int32_t dcx[2], dcy[2];              /* the DC blocker after DRIVE */
    int32_t env;                         /* the follower (mix units) */
    int32_t f, k, t, w24, drv, mk, mix;  /* glided: FREQ (index Q8), k (Q24), TYPE (Q15), the 24 dB share (Q15),
                                          * DRIVE's gain and its make-up (Q12), MIX (Q15) */
    int32_t rng;
    uint8_t run;
} afs;
static int32_t afl_scb[64];              /* the sidechain circle's block (ac_measure, from mix_part), mix units */
static uint32_t afl_scn;

/* mix_part (through ac_measure): the circle the follower listens to keeps its block, as it goes into the mix */
static void afl_take(uint32_t c, const int32_t *b, int32_t lvl, uint32_t n)
{
    uint32_t i, src = afl_p(AFL_SRC);
    if (!afs.run || src < AFS_C1 || src > AFS_C3 || c != src - AFS_C1)
        return;
    if (n > 64u)
        n = 64u;
    for (i = 0; i < n; i++)
        afl_scb[i] = ((b[i] >> 2) * lvl) >> 10;
    afl_scn = n;
}

/* the beat's envelope t samples after it, Q15: up in 5 ms, down over ~150 ms (squared) */
static int32_t afl_beat(uint32_t t)
{
    uint32_t a = 220u, r = 6615u, u;
    if (t < a)
        return (int32_t)(t * 32767u / a);
    t -= a;
    if (t >= r)
        return 0;
    u = (r - t) * 32767u / r;
    return (int32_t)((u * u) >> 15);
}

/* the three coefficients of a TPT stage at g (Q24) and k (Q24), Q24 */
static void afl_coef(int32_t g, int32_t k, int32_t *a)
{
    int64_t den = (1ll << 24) + (((int64_t)g * (g + k)) >> 24);
    if (den < (1ll << 20))
        den = 1ll << 20;
    a[0] = (int32_t)((1ll << 48) / den);
    a[1] = (int32_t)(((int64_t)g * a[0]) >> 24);
    a[2] = (int32_t)(((int64_t)g * a[1]) >> 24);
}

/* RES 0..127 -> k (Q24): 2 .. 0.12 (the old filter went to -0.4: it screamed; this one stays stable) */
#define AFL_KMIN 2013266
static int32_t afl_k_of(int32_t res) { return (1 << 25) - res * (((1 << 25) - AFL_KMIN) / 127); }

/* one stage, one sample (its state in locals: the loop keeps it in registers): LP, BP and HP crossfaded by wl wb wh
 * (Q15), only the bands heard; kn scales the BP (Q24) */
#define AFL_STAGE(OUT, V0, IC1, IC2, A0, A1, A2, K, KN, WL, WB, WH) do {                                          \
        int32_t v3_ = (V0) - IC2, v1_, v2_;                                                                          \
        int64_t acc_;                                                                                                \
        v1_ = (int32_t)(((int64_t)(A0) * IC1 + (int64_t)(A1) * v3_) >> 24);                                          \
        v2_ = IC2 + (int32_t)(((int64_t)(A1) * IC1 + (int64_t)(A2) * v3_) >> 24);                                    \
        IC1 = clamp(2 * v1_ - IC1, -(1 << 26), 1 << 26);                                                             \
        IC2 = clamp(2 * v2_ - IC2, -(1 << 26), 1 << 26);                                                             \
        acc_ = (WL) ? (int64_t)v2_ * (WL) : 0;                                                                       \
        if (WB)                                                                                                      \
            acc_ += (int64_t)(int32_t)(((int64_t)v1_ * (KN)) >> 24) * (WB);                                          \
        if (WH)                                                                                                      \
            acc_ += (int64_t)((V0) - (int32_t)(((int64_t)(K) * v1_) >> 24) - v2_) * (WH);                            \
        OUT = (int32_t)(acc_ >> 15);                                                                                 \
    } while (0)

/* AFL_G at an index Q8 (0 .. AFL_TOP) */
static int32_t afl_g(int32_t p)
{
    int32_t i = p >> 8, f = p & 255;
    if (p <= 0)
        return AFL_G[0];
    if (i >= 256)
        return AFL_G[256];
    return AFL_G[i] + (int32_t)(((int64_t)(AFL_G[i + 1] - AFL_G[i]) * f) >> 8);
}

static void afl_reset(void)
{
    uint32_t i;
    for (i = 0; i < sizeof afs.s / sizeof(int32_t); i++)
        ((int32_t *)afs.s)[i] = 0;
    afs.dcx[0] = afs.dcx[1] = afs.dcy[0] = afs.dcy[1] = 0;
    afs.env = 0;
    afs.mix = 0;
    afs.f = afl_p(AFL_FREQ) * (AFL_TOP / 127);
    afs.k = afl_k_of(afl_p(AFL_RES));
    afs.t = afl_p(AFL_TYPE) * 258;
    afs.w24 = afl_p(AFL_SLOPE) ? 32768 : 0;
    afs.drv = (int32_t)AFL_DRV[afl_p(AFL_DRIVE) & 127];
    afs.mk = AFL_MK[afl_p(AFL_DRIVE) & 127];
}

/* the block (fx.c mix_block, before the MASTER): l, r the mix; w the wet bus, folded in and emptied */
/* the block: what both sides share, worked out once (a stretch = the block, or 8 samples while FM is up), then each
 * side alone (afl_side), the right one on the second core when it answers */
#define AFL_NST 8u                       /* stretches a block (blocks of 64 at most) */
typedef struct {
    int32_t a2[2][3], a1[2][3], wl, wb, wh, ms, mc;
} afl_st_t;
static struct {
    afl_st_t st[AFL_NST];
    int32_t bias, sb, hard, mk, kn, st1, sub, drive, wet;
    int32_t *lr[2], *w;
    uint32_t n;
} afb;

static __attribute__((noinline)) void afl_side(uint32_t side)
{
    int32_t *o_ = afb.lr[side], *w = afb.w, bias = afb.bias, sb = afb.sb, hard = afb.hard, mk8 = afb.mk * 8;
    int32_t drv = afs.drv, k = afs.k, kn = afb.kn, w24 = afs.w24, drive = afb.drive, st1 = afb.st1, wet = afb.wet;
    int32_t a1c = afs.s[side][0].ic1, a2c = afs.s[side][0].ic2, b1c = afs.s[side][1].ic1, b2c = afs.s[side][1].ic2;
    int32_t dcx = afs.dcx[side], dcy = afs.dcy[side];
    uint32_t i, j, n = afb.n, q, sub = (uint32_t)afb.sub;
    for (j = 0, q = 0; j < n; j += sub, q++) {
        const afl_st_t *t = &afb.st[q];
        const int32_t *c2 = t->a2[side], *c1 = t->a1[side];
        int32_t p0 = c2[0], p1 = c2[1], p2 = c2[2], f0 = c1[0], f1 = c1[1], f2 = c1[2];
        int32_t wl = t->wl, wb = t->wb, wh = t->wh, ms = t->ms, mc = t->mc;
        uint32_t e = j + sub < n ? j + sub : n;
        for (i = j; i < e; i++) {
            int32_t x = clamp(o_[i] + w[i], -(1 << 22), 1 << 22), y = x, o;
            if (drive) {                     /* DRIVE: into tanh units, its bias taken back out; a DC blocker after */
                int32_t u = clamp((int32_t)(((int64_t)x * drv) >> 15), -(1 << 24), 1 << 24);
                y = softclip(u + bias) - sb;
                if (hard) {
                    int32_t h = clamp(u + bias, -32767, 32767) - bias;
                    y += (int32_t)(((int64_t)(h - y) * hard) >> 15);
                }
                y = (int32_t)(((int64_t)y * mk8) >> 12);
                dcy = y - dcx + (int32_t)(((int64_t)dcy * 32700) >> 15);
                dcx = y;
                y = dcy;
            }
            if (st1) {                       /* 24 dB: the plain stage first, crossfaded in */
                AFL_STAGE(o, y, a1c, a2c, f0, f1, f2, AFL_K1, AFL_K1, wl, wb, wh);
                y = y + (int32_t)(((int64_t)(o - y) * w24) >> 15);
            }
            AFL_STAGE(o, y, b1c, b2c, p0, p1, p2, k, kn, wl, wb, wh);
            o_[i] = wet ? o : (int32_t)(((int64_t)x * mc + (int64_t)o * ms) >> 15);
        }
    }
    afs.s[side][0].ic1 = a1c, afs.s[side][0].ic2 = a2c, afs.s[side][1].ic1 = b1c, afs.s[side][1].ic2 = b2c;
    afs.dcx[side] = dcx, afs.dcy[side] = dcy;
}
static volatile uint32_t afl_c1_tk;
#if FELUCCA_CPU2 && defined(C1_RUN)
/* (afl_c1_tk, v0.54: the right side's time on the second core, TIMER4 ticks (fx.c's split) */
static void afl_side_c1(uint32_t arg)
{
#if defined(FM1_TICKS_PER_US)
    uint32_t t0 = fm1_ticks();
    (void)arg;
    afl_side(1);
    afl_c1_tk = fm1_ticks() - t0;
#else
    (void)arg;
    afl_side(1);
#endif
}
#endif

static __attribute__((noinline)) void afl_block(int32_t *l, int32_t *r, int32_t *w, uint32_t n)
{
    int32_t mixT = afl_p(AFL_MIX) * 258, kT, tT, w24T, drvT, src, mod, mtyp, fm, det, dv, pk = 0, envq = 0, foll;
    int32_t m0, dm, beat0 = -1, sub;
    uint32_t i, j, q;
    if (!afs.run) {
        if (!mixT)
            return;
        afl_reset();
        afs.run = 1;
    }
    if (n > AFL_NST * AFL_SUB)
        n = AFL_NST * AFL_SUB;
    kT = afl_k_of(afl_p(AFL_RES));
    tT = afl_p(AFL_TYPE) * 258;
    w24T = afl_p(AFL_SLOPE) ? 32768 : 0;
    dv = afl_p(AFL_DRIVE);
    drvT = (int32_t)AFL_DRV[dv & 127];
    src = afl_p(AFL_SRC);
    mod = afl_p(AFL_MOD) - 64;
    mtyp = afl_p(AFL_MTYP) - 64;
    fm = afl_p(AFL_FM);
    det = afl_p(AFL_DET) - 64;
    det = (det < 0 ? -det * det : det * det) * AFL_OCT / 4096;   /* to an octave, fine near the middle */
    /* the glides (a quarter of the way a block, ~3 ms; MIX straight, 10 ms end to end) */
    afs.f += (afl_p(AFL_FREQ) * (AFL_TOP / 127) - afs.f) >> 2;
    afs.k += (kT - afs.k) >> 2;
    afs.t += (tT - afs.t) >> 2;
    afs.w24 += (w24T - afs.w24) >> 2;
    afs.drv += (drvT - afs.drv) >> 2;
    afs.mk += (AFL_MK[dv & 127] - afs.mk) >> 2;
    m0 = afs.mix;
    afs.mix = mixT > afs.mix ? (afs.mix + 2400 < mixT ? afs.mix + 2400 : mixT)
                             : (afs.mix - 2400 > mixT ? afs.mix - 2400 : mixT);
    dm = (afs.mix - m0) / (int32_t)n;
    afb.bias = dv * dv * 7000 / 16129;   /* (tanh units: up to ~0.2) */
    afb.hard = dv > 64 ? (dv - 64) * 330 : 0;
    afb.mk = afs.mk;
    afb.sb = softclip(afb.bias);
    afb.drive = dv || afs.drv != (int32_t)AFL_DRV[0] || afs.mk != AFL_MK[0];   /* (0, settled: no tanh at all) */
    afb.st1 = afs.w24 || w24T;           /* (12 dB, settled: one stage) */
    afb.kn = clamp(afs.k, 5872026, 1 << 25) * 2;   /* the BP brought up: 2 k, k at least 0.35 */
    afb.wet = m0 >= 32766 && afs.mix >= 32766;     /* (MIX full, 127 x 258: no dry blend) */
    if (src >= AFS_4 && am_play) {
        static const uint8_t DIV[2] = {4, 2};
        uint32_t kk = DIV[src - AFS_4], s = am_gstep ? am_gstep - 1u : 0u;
        beat0 = (int32_t)((s % kk) * am_p + am_pos);
    }
    /* the follower: only when something listens to it (MOD, MODTYP off at 64; FM 0), one value a block */
    foll = (mod || mtyp || fm) && src < AFS_4;
    if (foll) {
        for (i = 0; i < n; i++) {
            int32_t a;
            if (src >= AFS_C1 && src <= AFS_C3)
                a = i < afl_scn ? afl_scb[i] : 0;
            else
                a = ((l[i] + w[i]) >> 1) + ((r[i] + w[i]) >> 1);
            a = a < 0 ? -a : a;
            if (a > pk)
                pk = a;
        }
        afs.env += (int32_t)(((int64_t)(pk - afs.env) * (pk > afs.env ? 8850 : 475)) >> 16);   /* ~5 ms up, ~100 down (a block: 0.73 ms) */
    }
    sub = fm ? (int32_t)AFL_SUB : (int32_t)(AFL_NST * AFL_SUB);
    afb.sub = sub;
    for (j = 0, q = 0; j < n; j += (uint32_t)sub, q++) {
        uint32_t side;
        int32_t sq = 0, p, t, gs[2];
        afl_st_t *st = &afb.st[q];
        if (src >= AFS_4)
            envq = beat0 >= 0 ? afl_beat((uint32_t)beat0 + j) : 0;
        else if (foll)
            envq = (int32_t)clamp((int32_t)(((int64_t)afs.env * 32767) / AFL_EREF), 0, 32767);
        if (fm) {
            int32_t x = src >= AFS_C1 && src <= AFS_C3 ? (j < afl_scn ? afl_scb[j] : 0) : (l[j] + r[j] + 2 * w[j]) >> 1;
            sq = (int32_t)clamp((int32_t)(((int64_t)x * 32767) / AFL_EREF), -32767, 32767);
        }
        p = afs.f + mod * envq / 64 * (3 * AFL_OCT) / 32768 + fm * sq / 127 * (2 * AFL_OCT) / 32768;
        t = clamp(afs.t + mtyp * envq / 64, 0, 32768);
        afl_now = clamp(p - det, 0, AFL_TOP);
        afl_tnow = t;
        st->wl = t < 16384 ? 32768 - 2 * t : 0;
        st->wh = t > 16384 ? 2 * t - 32768 : 0;
        st->wb = 32768 - st->wl - st->wh;
        for (side = 0; side < 2u; side++)
            gs[side] = afl_g(clamp(side ? p + det : p - det, 0, AFL_TOP));
        for (side = 0; side < 2u; side++) {
            if (side && gs[1] == gs[0]) {    /* (DETUNE at the middle: the same coefficients) */
                for (i = 0; i < 3u; i++)
                    st->a2[1][i] = st->a2[0][i], st->a1[1][i] = st->a1[0][i];
                break;
            }
            afl_coef(gs[side], afs.k, st->a2[side]);
            if (afb.st1)
                afl_coef(gs[side], AFL_K1, st->a1[side]);
        }
        if (!afb.wet) {                      /* MIX: equal power, its sine / cosine at this stretch's start */
            st->ms = sine_i((uint32_t)(afs.mix > 32767 ? 32767 : m0 + dm * (int32_t)j) << 15);
            st->mc = sine_i(((uint32_t)(afs.mix > 32767 ? 32767 : m0 + dm * (int32_t)j) << 15) + 0x40000000u);
        }
    }
    afb.lr[0] = l;
    afb.lr[1] = r;
    afb.w = w;
    afb.n = n;
#if FELUCCA_CPU2 && defined(C1_RUN)
    if (C1_ON() && C1_RUN(afl_side_c1, 0)) {
        afl_side(0);
        if (C1_WAIT())                   /* it stopped answering: the right side here */
            afl_side(1);
    } else
#endif
    {
        afl_side(0);
        afl_side(1);
    }
    for (i = 0; i < n; i++)
        w[i] = 0;
    afl_peak = pk;
    if (!mixT && !afs.mix) {             /* off: nothing runs from the next block (the output as without it) */
        afs.run = 0;
        afl_now = -1;
    }
}

/* ---- the UI side ------------------------------------------------------------------------------------------- */
static void afl_off(void)                /* (a scene without one) */
{
    uint32_t s, k;
    for (s = 0; s < 2u; s++)
        for (k = 0; k < AFL_N; k++)
            afl[s][k] = AFL_PRE[0].v[k];
    afl_morph = 0;
    afl_sel[0] = afl_sel[1] = 0;
    afl_edit = 0;
}
/* preset i into snapshot s (the other one too while it is off: the morph starts from somewhere) */
static void afl_pre_apply(uint32_t s, uint32_t i)
{
    uint32_t k, o = s ^ 1u;
    i %= AFL_NPRE;
    for (k = 0; k < AFL_N; k++)
        afl[s & 1u][k] = AFL_PRE[i].v[k];
    afl_sel[s & 1u] = (uint8_t)i;
    afl_edit &= (uint8_t)~(1u << (s & 1u));
    if (!afl[o][AFL_MIX]) {
        for (k = 0; k < AFL_N; k++)
            afl[o][k] = AFL_PRE[i].v[k];
        afl_sel[o] = (uint8_t)i;
        afl_edit &= (uint8_t)~(1u << o);
    }
}
/* knob k (0..10 a setting of snapshot s, 11 MORPH) by s steps */
static void afl_turn(uint32_t sn, uint32_t k, int32_t s)
{
    volatile uint8_t *v;
    int32_t lo = 0, hi = 127, st = s * 2;
    if (k == AFL_MORPH) {
        afl_morph = (uint8_t)clamp((int32_t)afl_morph + st, 0, 127);
        return;
    }
    v = &afl[sn & 1u][k % AFL_N];
    if (k == AFL_SRC)
        hi = AFS_N - 1, st = s > 0 ? 1 : -1;
    else if (k == AFL_SLOPE)
        hi = 1, st = s > 0 ? 1 : -1;
    else if (k == AFL_FREQ || k == AFL_RES)
        st = s;                          /* (fine: a step is ~1/13 octave) */
    *v = (uint8_t)clamp((int32_t)*v + st, lo, hi);
    afl_edit |= (uint8_t)(1u << (sn & 1u));
}

/* a USER scene's record: the two snapshots, MORPH, the presets */
#define AFL_REC (2u * AFL_N + 4u)
static void afl_get(uint8_t *b)
{
    uint32_t s, k;
    for (s = 0; s < 2u; s++)
        for (k = 0; k < AFL_N; k++)
            b[s * AFL_N + k] = afl[s][k];
    b[2u * AFL_N] = afl_morph;
    b[2u * AFL_N + 1u] = afl_sel[0];
    b[2u * AFL_N + 2u] = afl_sel[1];
    b[2u * AFL_N + 3u] = afl_edit;
}
static void afl_put(const uint8_t *b)
{
    uint32_t s, k;
    for (s = 0; s < 2u; s++)
        for (k = 0; k < AFL_N; k++)
            afl[s][k] = (uint8_t)clamp(b[s * AFL_N + k], 0, k == AFL_SRC ? AFS_N - 1 : k == AFL_SLOPE ? 1 : 127);
    afl_morph = (uint8_t)clamp(b[2u * AFL_N], 0, 127);
    afl_sel[0] = (uint8_t)(b[2u * AFL_N + 1u] % AFL_NPRE);
    afl_sel[1] = (uint8_t)(b[2u * AFL_N + 2u] % AFL_NPRE);
    afl_edit = b[2u * AFL_N + 3u] & 3u;
}
