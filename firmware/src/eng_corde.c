/* SPDX-License-Identifier: GPL-3.0-only
 * CORDE (Romain 05/11): a metal string, mistreated. A Karplus-Strong loop (a delay line one period long, a low-pass,
 * a feedback) made brutal: two allpasses in the loop (a stiff wire: its overtones go sharp, it clangs), a drive and a
 * fold in the loop (it screams, breaks up), a bridge that buzzes (a sitar's, a rattling spring).
 *
 *   exciter -> line (one period) -> allpasses (SPRD) -> low-pass (TONE) -> BUZZ -> DIRT -> x RES -> back ; out: the line
 *
 * The four main knobs follow the new engines' grammar (MATTER, SPREAD, RESONANCE, DIRT):
 * MATR  what sets it going (the knob scaled to 0..90, 05/11): 0..42 a soft pluck (a dark burst), 43..84 a hard pick
 *       (bright, a click), 85..90 a bow or a scrape that lasts while the note is held (friction: a crackle)
 * SPRD  the stiffness: 0 a plain string (in tune, harmonic) .. a piano wire .. a cable, a spring (inharmonic, clanging)
 * RES   how long it rings: a muted thud (0.1 s) -> a wire that sustains (15 s), the same at every pitch
 * DIRT  a drive in the loop: 0 clean; up the middle it thickens and distorts; past ~90 a fold too: it screams,
 *       feeds back, comes apart
 * TONE  the loop's low-pass: dull (nylon, rust) .. bright (steel)
 * DCAY  the exciter's length (0.2 .. 80 ms)
 * BUZZ  the bridge: 0 none .. a sitar's jawari .. a rattle (one side of the wave clipped, in the loop)
 * BEND  the pitch falls after the hit (a slack string struck hard): 0 none .. an octave
 * The track's SHP moves MATR, FLT moves TONE; the ADSR on top (a long SUS keeps it ringing).
 *
 * Two voices per part. Each line in the part's engine memory, 16 bits, 2048 samples (C1 and up). The allpasses'
 * delay is taken off the line's length, so a stiff string stays (about) in tune. */

#define CD_POLY 2
#define CD_LEN 2048
typedef struct {
    int16_t buf[CD_LEN];
    int32_t lp, ap1x, ap1y, ap2x, ap2y, hp, hpx;
    int32_t env;                 /* the exciter's level, Q16 */
    int32_t bend;                /* the pitch bend now, Q12 of the length */
    int32_t xlp;
    int32_t dc, dcx;
    int32_t rng;
    uint16_t w;
} corde_t;

ENG_MEM_FITS("CORDE", sizeof(corde_t) * CD_POLY);

static corde_t *corde_of(track_t *t, voice_t *v)
{
    uint32_t i;
    if (t < &trk[0] || t >= &trk[NPART])
        return 0;
    i = (uint32_t)(v - t->v);
    return i < CD_POLY ? (corde_t *)ENG_MEM(t - trk) + i : 0;
}

static void corde_note_on(track_t *t, voice_t *v)
{
    corde_t *S = corde_of(t, v);
    if (!S)
        return;
    S->env = 65536;
    S->xlp = 0;
    S->bend = t->p[P_E7] * t->p[P_E7] * 4096 / 16129;   /* (BEND: up to an octave's worth, finer near 0) */
    if (!S->rng)
        S->rng = (int32_t)((v->note + 1u) * 0x9E3779B9u ^ (uint32_t)(v - t->v) * 0x85EBCA6Bu) | 1;
}

static void corde_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    corde_t *S = corde_of(t, v);
    const int16_t *p = t->p;
    uint32_t i, w, len16, di, f;
    int32_t mt, c, apd, g, dirt, drv, fold, tone, rate, buzz, env, rng, xa, xg;
    if (!S)
        return;
    mt = (clamp((p[P_E0] << 8) + m->shape - (64 << 8), 0, 127 << 8) >> 8) * 90 / 127;   /* MATR (+ SHP); the knob at
                                                  * full = 90 (Romain 05/11: over it the friction was only noise) */
    /* SPRD: the allpasses' coefficient (Q15, 0 .. -0.75: stiffer, more sharp overtones) and their delay at low
     * frequencies, (1 - c) / (1 + c) samples each, taken off the line (Q16) */
    c = -(p[P_E1] * p[P_E1] * 24576 / 16129);
    apd = (int32_t)(((int64_t)(32768 - c) << 16) / (32768 + c));      /* (one allpass, Q16 samples) */
    /* the period, Q16, the bend on it (shorter at the hit: higher) */
    len16 = m->inc > (1u << 16) ? (uint32_t)((1ull << 48) / m->inc) : (uint32_t)(CD_LEN - 8) << 16;
    while (len16 > (uint32_t)(CD_LEN - 8) << 16)
        len16 >>= 1;
    len16 -= (uint32_t)(((uint64_t)len16 * (uint32_t)S->bend) >> 13);
    {
        int64_t l = (int64_t)len16 - 2 * apd - (1 << 15);   /* (the allpasses and the low-pass's half sample) */
        len16 = (uint32_t)(l < (3 << 16) ? 3 << 16 : l);
    }
    di = len16 >> 16;
    f = len16 & 65535u;
    S->bend -= S->bend / 120 + 1;                /* (the bend falls back over ~0.3 s) */
    if (S->bend < 0)
        S->bend = 0;
    {                                            /* RES: 0.1 .. 15 s to -60 dB, the same at every pitch (as PLATE) */
        int32_t n60 = (int32_t)(((int64_t)4410 * reso_exp2(p[P_E2] * 29611 / 127)) >> 16);   /* (x 150: 7.23 oct.) */
        int32_t o = (int32_t)((int64_t)40820 * (int32_t)((len16 >> 16) + 2) / (n60 > 1 ? n60 : 1));
        g = (int32_t)(reso_exp2(-(o > 60000 ? 60000 : o)) >> 1);
        if (g > 32740)
            g = 32740;
    }
    dirt = p[P_E3];
    drv = 4096 + dirt * 80;                      /* DIRT: the drive into the loop's tanh, 1 .. 3.5, Q12 */
    fold = dirt > 90 ? (dirt - 90) * 600 : 0;
    tone = clamp(p[P_E4] * 256 + m->cutoff, 0, 127 << 8);   /* TONE (+ FLT) */
    tone = 3000 + tone * 9 / 10;                 /* (the loop's low-pass, Q15) */
    if (tone > 32767)
        tone = 32767;
    rate = 10 + p[P_E5] * p[P_E5] * 3500 / 16129;   /* DCAY (at least half a period: the burst fills the string) */
    if (rate < (int32_t)(len16 >> 17))
        rate = (int32_t)(len16 >> 17);
    buzz = p[P_E6] ? 30000 - p[P_E6] * 200 : 0;  /* BUZZ: the ceiling one side of the wave hits (Q15) */
    xa = mt < 43 ? 1500 + mt * 300 : 32767;       /* the pluck dark; the pick bright */
    xg = 64 + 16384 / (rate + 16) + (32767 - xa) / 90;   /* (the exciter's gain, Q6: short and dark strikes louder) */
    env = S->env;
    rng = S->rng;
    w = S->w;
    for (i = 0; i < n; i++) {
        int32_t x = 0, r, z, a0, a1;
        if (env > 16) {                           /* the pluck / the pick: a burst */
            int32_t nz = (int32_t)(noise32(&rng) >> 16) - 32768;
            x = (int32_t)(((int64_t)nz * (env >> 1)) >> 15);
            if (mt >= 43 && mt <= 84 && env > 60000)
                x += 12000;                       /* (the pick's click) */
            env -= env / rate + 1;
        }
        if (mt > 84 && v->gate) {                 /* the bow, the scrape: friction while held */
            int32_t q = (int32_t)(noise32(&rng) >> 16), dens = (mt - 84) * 1500;
            if (dens >= 60000)
                x += (q - 32768) >> 3;
            else if (q < dens)
                x += ((int32_t)(noise32(&rng) >> 17) - 16384) >> 1;
        }
        x = clamp((int32_t)(((int64_t)x * xg) >> 6), -32767, 32767);   /* (a short strike as strong as a long one) */
        S->xlp += (int32_t)(((int64_t)(x - S->xlp) * xa) >> 15);
        x = S->xlp;
        {                                         /* the line, one period ago (linear between two) */
            int32_t i0 = (int32_t)w - (int32_t)di, i1 = i0 - 1;
            if (i0 < 0)
                i0 += CD_LEN;
            if (i1 < 0)
                i1 += CD_LEN;
            r = S->buf[i0] + (int32_t)(((int64_t)(S->buf[i1] - S->buf[i0]) * (int32_t)f) >> 16);
        }
        /* two allpasses: y = c x + x1 - c y1 (a stiff wire) */
        a0 = (int32_t)(((int64_t)c * r) >> 15) + S->ap1x - (int32_t)(((int64_t)c * S->ap1y) >> 15);
        S->ap1x = r, S->ap1y = a0;
        a1 = (int32_t)(((int64_t)c * a0) >> 15) + S->ap2x - (int32_t)(((int64_t)c * S->ap2y) >> 15);
        S->ap2x = a0, S->ap2y = a1;
        S->lp += (int32_t)(((int64_t)(a1 - S->lp) * tone) >> 15);   /* the damping */
        z = (int32_t)(((int64_t)S->lp * g) >> 15);
        if (buzz && z > buzz)                     /* BUZZ: the bridge, one side */
            z = buzz + ((z - buzz) >> 3);
        if (dirt) {                               /* DIRT: a tanh (the small-signal gain kept), then a fold */
            z = (int32_t)(((int64_t)z * drv) >> 12);
            z = softclip(clamp(z, -65535, 65535));
            z = (int32_t)(((int64_t)z << 12) / drv);
            if (fold) {
                int32_t cl = 32767 - fold, a = z < 0 ? -z : z;
                if (a > cl)
                    a = 2 * cl - a;
                z = z < 0 ? -a : a;
            }
            S->hp = z - S->hpx + (int32_t)(((int64_t)S->hp * 32440) >> 15);   /* (the loop's DC out: ~50 Hz) */
            S->hpx = z;
            z = S->hp;
        }
        z += x;
        if (++w >= CD_LEN)
            w = 0;
        S->buf[w] = (int16_t)clamp(z, -32767, 32767);
        S->dc = r - S->dcx + (int32_t)(((int64_t)S->dc * 32604) >> 15);   /* the output's DC blocker */
        S->dcx = r;
        out[i] += voice_amp(soft_knee(clamp(S->dc * 3 / 2, -(1 << 20), 1 << 20), 22000), m, i);
    }
    S->env = env;
    S->rng = rng;
    S->w = (uint16_t)w;
}

/* {MATR SPRD RES DIRT TONE DCAY BUZZ BEND} (ALTER's sounds: alter_sounds.c ALT_PRE) */
static const preset_t CORDE_PRESETS[] = {
    {"WIRE", {85, 40, 110, 20, 90, 10, 0, 0}, {0, 100, 127, 80}, 0, 0, FX(0, 0, 10, 20)},
};

static const engine_t ENG_CORDE = {
    .name = "CORDE",
    .page_title = {"MAIN", "MORE"},
    .edit = {
        {"MATR", F_PCT, 0, 127, 60, 0, 0},
        {"SPRD", F_PCT, 0, 127, 30, 0, 0},
        {"RES", F_PCT, 0, 127, 105, 0, 0},
        {"DIRT", F_PCT, 0, 127, 20, 0, 0},
        {"TONE", F_PCT, 0, 127, 90, 0, 0},
        {"DCAY", F_PCT, 0, 127, 10, 0, 0},
        {"BUZZ", F_PCT, 0, 127, 0, 0, 0},
        {"BEND", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = CORDE_PRESETS,
    .npresets = NELEM(CORDE_PRESETS),
    .note_on = corde_note_on,
    .render = corde_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = CD_POLY,
    .mem = sizeof(corde_t) * CD_POLY,
};
