/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's SAVE / LOAD (included by alter_ui.c after alter_sounds.c). Two tabs, each a list:
 *   NEW (load only: a blank scene, or an empty pattern), 4 factory ones (protected), 16 USER slots.
 * A SCENE is everything: per circle its sound (engine, E1..E8, ADSR, sends, insert, level, pan, glide, LFO), envelope
 * mode, locks and rhythm (LEN HITS ROT PROB FROT, DOSE, ROOT, CHORD, each step's force and chance), its LFO's
 * three LFOs (WAVE RATE DEPTH DEST) and its four matrix slots; and the scale, tempo, swing and the GLOBAL LFO (alter_lfo.c). A PATTERN is the rhythm only (per circle LEN HITS ROT PROB FROT, force and chance per step):
 * loading one leaves the sounds, notes and everything else as they are.
 * The USER slots live in RAM (alt_bank_s, alt_bank_p) and on the flash, in Felucca's project objects 3 and 4
 * (storage.c OBJ_PROJECT0 + 2 / + 3; an own magic tells ours from a Felucca project saved there); the USER scenes'
 * rhythms in object 2 (OBJ_PROJECT0 + 1: alt_bank_r). A USER scene's
 * envelope modes' settings (alter_lfo.c AE_*) are kept with the patterns, in object 4 (alt_bank_p.env). Packed small: a
 * step's force in 2 bits, its chance in 4 (16 levels). */

#define ALT_NUSER 16
#define ALT_NFACT 4
#define ALT_NLIST (1 + ALT_NFACT + ALT_NUSER)   /* NEW, the factory ones, the USER slots */
#define ALT_OBJ_SCENES 3u                        /* storage.c OBJ_PROJECT0 + 2 (OBJ_PROJECT0 = 1) */
#define ALT_OBJ_PATS 4u                          /* OBJ_PROJECT0 + 3 */
#define ALT_OBJ_SRHY 2u                          /* OBJ_PROJECT0 + 1: the USER scenes' rhythms (the scenes' object is full) */
#define ALT_MAGIC_R 0x524C5441u                  /* "ATLR" */
#define ALT_MAGIC_S 0x534C5441u                  /* "ATLS" */
#define ALT_MAGIC_P 0x504C5441u                  /* "ATLP" */
static int st_load(uint32_t obj, void *dst, uint32_t max);   /* (storage.c, included later) */
static int st_save(uint32_t obj, const void *src, uint32_t len);

/* the part's parameters a scene keeps besides E1..E8 (each fits a signed byte) */
static const uint8_t ALT_SP[] = {P_ATK, P_DEC, P_SUS, P_REL, P_DIST, P_CHOR, P_DLY, P_REV, P_ITYPE, P_IA, P_IMIX,
                                 P_LEVEL, P_PAN, P_GLIDE};
#define ALT_NSP (sizeof ALT_SP)

typedef struct {                 /* a circle's rhythm */
    uint8_t len, hits, rot, prob, frot;
    uint8_t type, tlock, tdens, trange;          /* its engine (alter_seq.c AS_T_*), TURING's settings */
    uint8_t vel[AS_NLANE / 2];                   /* the lanes, 4 bits a step: VELO (x 127 / 15, 0 = 1) .. */
    uint8_t chance[AS_MAXLEN / 2];               /* .. CHANCE, 32 steps (x 127 / 15) .. */
    uint8_t dly[AS_NLANE / 2];                   /* .. DELAY (x 127 / 15) .. */
    uint8_t rev[AS_NLANE / 2];                   /* .. REVERB (x 127 / 15) .. */
    uint8_t pitch[AS_NLANE / 2];                 /* .. PITCH (-7..7 + 8) */
} alt_rhy_t;

typedef struct {                 /* a circle in a scene */
    uint8_t eng, pre, mode, locks, dose, root, chord, rsv;
    int8_t e[8];
    int8_t p[ALT_NSP];
    uint8_t lfo[3 * CL_N];                       /* its three LFOs: WAVE | RATE << 3, DEST, DEPTH (alter_lfo.c) */
    uint8_t m[2 * NMSLOT];                       /* the matrix slots: SRC (4 bits) DST (5) AMT + 64 (7), 2 bytes each */
} alt_scirc_t;

typedef struct {
    uint8_t used, bpm, scale, swing;
    alt_scirc_t c[AS_NCIRC];
    uint8_t gwave, grate, gdst;                  /* the GLOBAL LFO */
    int8_t gamt;
} alt_scene_rec_t;

typedef struct {
    uint8_t used, rsv[3];
    alt_rhy_t c[AS_NCIRC];
} alt_pat_rec_t;

static struct { uint32_t magic; alt_scene_rec_t s[ALT_NUSER]; } alt_bank_s;
#define ALT_FXREC 32u                            /* a USER scene's effects (alt_fx_get) */
static struct {
    uint32_t magic;
    alt_rhy_t r[ALT_NUSER][AS_NCIRC];            /* the USER scenes' rhythms (object 2) .. */
    uint8_t fxok[ALT_NUSER];                     /* .. and their effects (the FX page; 0: stored before it) .. */
    uint8_t fx[ALT_NUSER][ALT_FXREC];
    uint8_t rat[ALT_NUSER][AS_NCIRC][AS_NLANE / 4];   /* .. and their RATCH lanes, 2 bits a step (0: stored before it) */
} alt_bank_r;
#define ALT_BANK_R_OLD (4u + sizeof(alt_rhy_t) * ALT_NUSER * AS_NCIRC)   /* (before the effects: still read) */
#define ALT_BANK_R_FX (ALT_BANK_R_OLD + ALT_NUSER * (1u + ALT_FXREC))     /* (before the RATCH lane: still read) */
_Static_assert(sizeof ALT_USER_AMBIENT == sizeof(alt_scene_rec_t) + sizeof(alt_rhy_t) * AS_NCIRC + 2u + ALT_FXREC +
               AS_NCIRC * AE_N, "a USER scene kept as a factory one matches the records (else: re-extract it)");
static void afp_dly_apply(uint32_t i);           /* (alter_fxpage.c) */
static void afp_rev_apply(uint32_t i);
static void afp_mst_apply(uint32_t i);

/* the effects in a USER scene: the shared DELAY and REVERB, the MASTER, the presets in use, each circle's INSERT B C
 * (its DIST, TYPE, A, MIX are with its sound: ALT_SP) */
static const uint8_t ALT_FXG[7] = {G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX, G_RTYPE, G_RSIZE, G_RDAMP};
static void alt_fx_get(uint8_t *b)
{
    uint32_t k;
    for (k = 0; k < 7u; k++)
        b[k] = (uint8_t)song.g[ALT_FXG[k]];
    for (k = 0; k < AMX_N; k++)
        b[7u + k] = amx[k];
    for (k = 0; k < AFX_NS; k++)
        b[15u + k] = afx_sel[k];
    b[21] = afx_edit;
    for (k = 0; k < AS_NCIRC; k++) {
        b[22u + 2u * k] = (uint8_t)trk[k].p[P_IB];
        b[23u + 2u * k] = (uint8_t)trk[k].p[P_IC];
    }
}
static void alt_fx_put(const uint8_t *b)
{
    uint32_t k;
    for (k = 0; k < 7u; k++)
        song.g[ALT_FXG[k]] = b[k];
    for (k = 0; k < AMX_N; k++)
        amx[k] = b[7u + k];
    for (k = 0; k < AFX_NS; k++)
        afx_sel[k] = b[15u + k];
    afx_edit = b[21];
    fm1_irq_off();
    for (k = 0; k < AS_NCIRC; k++) {
        trk[k].p[P_IB] = b[22u + 2u * k];
        trk[k].p[P_IC] = b[23u + 2u * k];
    }
    fm1_irq_on();
}
static struct {
    uint32_t magic;
    alt_pat_rec_t p[ALT_NUSER];
    uint8_t envok[ALT_NUSER];                    /* (the USER scenes' envelope modes' settings live here, object 4: the */
    uint8_t env[ALT_NUSER][AS_NCIRC][AE_N];      /* scenes' own object is full; 0: a scene stored before them) */
    uint8_t prat[ALT_NUSER][AS_NCIRC][AS_NLANE / 4];   /* the USER patterns' RATCH lanes, 2 bits a step */
} alt_bank_p;
#define ALT_BANK_P_OLD (sizeof alt_bank_p - sizeof alt_bank_p.prat)   /* (before the RATCH lane: still read) */
/* the USER scenes' MASTER FILTER (alter_filter.c), object 1 (OBJ_PROJECT0, free since Felucca's projects left):
 * kept only when one was on at SAVE (used = 0: the scene loads with the filter off); and each circle's steps added by
 * TRIGGER + REC (alter_seq.c as_circ_t.add) */
#define ALT_OBJ_FILT 1u
#define ALT_MAGIC_F 0x464C5441u                  /* "ATLF" */
static struct {
    uint32_t magic;
    uint8_t used[ALT_NUSER];
    uint8_t r[ALT_NUSER][AFL_REC];
    uint32_t add[ALT_NUSER][AS_NCIRC];
    uint8_t fx[ALT_NUSER][AS_NCIRC][AS_MAXLEN];  /* the fills REC'd per step (alter_play.c alt_fxr) */
} alt_bank_f;
#define ALT_BANK_F_OLD (4u + ALT_NUSER * (1u + AFL_REC))   /* (before the added steps: still read) */
#define ALT_BANK_F_ADD (ALT_BANK_F_OLD + ALT_NUSER * AS_NCIRC * 4u)   /* (before the REC'd fills: still read) */
_Static_assert(sizeof alt_bank_f == ALT_BANK_F_ADD + ALT_NUSER * AS_NCIRC * AS_MAXLEN, "(no padding)");
_Static_assert(sizeof alt_bank_s <= 3840u && sizeof alt_bank_p <= 3840u && sizeof alt_bank_r <= 3840u &&
               sizeof alt_bank_f <= 3840u,
               "ALTER's banks fit a storage object");

/* the factory patterns: per circle LEN HITS ROT PROB */
static const struct {
    const char *name, *about;
    uint8_t c[AS_NCIRC][4];
} ALT_FPAT[ALT_NFACT] = {
    {"FOUR FLOOR", "KICK 4 . OFFBEATS . 8THS", {{16, 4, 0, 127}, {16, 4, 2, 127}, {16, 8, 1, 120}}},
    {"ACID", "4 . SYNCOPATED 7 . 11 LOOSE", {{16, 4, 0, 127}, {16, 7, 0, 127}, {16, 11, 1, 100}}},
    {"BROKEN", "5 . 3 LATE . 9 LOOSE", {{16, 5, 0, 127}, {16, 3, 4, 127}, {16, 9, 2, 110}}},
    {"POLY", "16 . 12 . 7 STEPS", {{16, 4, 0, 127}, {12, 5, 0, 127}, {7, 3, 0, 127}}},
};

/* ---- the rhythm, in and out ---------------------------------------------------------------------------------- */
static uint32_t alt_q4(uint32_t v) { return (v * 15u + 63u) / 127u & 15u; }   /* 0..127 in 4 bits, and back */
static uint32_t alt_u4(uint32_t q) { return (q & 15u) * 127u / 15u; }
static void alt_rhy_get(uint32_t c, alt_rhy_t *r)
{
    const as_cfg_t *cf = &au.cfg[c];
    const as_circ_t *sc = &alt_seq.c[c];
    uint32_t k;
    r->len = cf->len, r->hits = cf->hits, r->rot = cf->rot, r->prob = cf->prob, r->frot = cf->frot;
    r->type = cf->type, r->tlock = cf->tlock, r->tdens = cf->tdens, r->trange = cf->trange;
    for (k = 0; k < AS_MAXLEN / 2u; k++)
        r->chance[k] = (uint8_t)(alt_q4(sc->chance[2u * k]) | alt_q4(sc->chance[2u * k + 1u]) << 4);
    for (k = 0; k < AS_NLANE / 2u; k++) {
        r->vel[k] = (uint8_t)(alt_q4(sc->svel[2u * k]) | alt_q4(sc->svel[2u * k + 1u]) << 4);
        r->dly[k] = (uint8_t)(alt_q4(sc->sdly[2u * k]) | alt_q4(sc->sdly[2u * k + 1u]) << 4);
        r->rev[k] = (uint8_t)(alt_q4(sc->srev[2u * k]) | alt_q4(sc->srev[2u * k + 1u]) << 4);
        r->pitch[k] = (uint8_t)(((sc->spitch[2u * k] + 8) & 15) | ((sc->spitch[2u * k + 1u] + 8) & 15) << 4);
    }
}

/* circle c's rhythm from r, at once (from the next step); the die forgotten */
static void alt_rhy_put(uint32_t c, const alt_rhy_t *r)
{
    as_cfg_t *cf = &au.cfg[c];
    as_circ_t *sc = &alt_seq.c[c];
    uint32_t k;
    cf->len = r->len ? r->len : 16u;
    cf->hits = r->hits, cf->rot = r->rot, cf->prob = r->prob, cf->frot = r->frot;
    cf->type = (uint8_t)(r->type % AS_NTYPE), cf->tlock = r->tlock, cf->tdens = r->tdens, cf->trange = r->trange;
    for (k = 0; k < AS_MAXLEN; k++)
        sc->chance[k] = (uint8_t)alt_u4(r->chance[k >> 1] >> ((k & 1u) * 4u));
    for (k = 0; k < AS_NLANE; k++) {
        uint32_t v = alt_u4(r->vel[k >> 1] >> ((k & 1u) * 4u));
        sc->svel[k] = (uint8_t)(v ? v : 1u);
        sc->sdly[k] = (uint8_t)alt_u4(r->dly[k >> 1] >> ((k & 1u) * 4u));
        sc->srev[k] = (uint8_t)alt_u4(r->rev[k >> 1] >> ((k & 1u) * 4u));
        sc->spitch[k] = (int8_t)(((r->pitch[k >> 1] >> ((k & 1u) * 4u)) & 15) - 8);
        sc->srat[k] = 0;                         /* (the RATCH lane: its own bytes, alt_rat_put) */
    }
    for (k = 0; k < AS_NLR; k++)
        sc->rmask[k] = 0;                        /* (the RANGEs are not stored yet: a load clears them) */
    alt_post_now |= (uint8_t)(1u << c);
    alt_post_circle(c);
    alt_clear_cnt[c]++;
}

/* circle c's RATCH lane in 4 bytes (2 bits a step), and back */
static void alt_rat_get(uint32_t c, uint8_t *b)
{
    uint32_t k;
    for (k = 0; k < AS_NLANE / 4u; k++)
        b[k] = 0;
    for (k = 0; k < AS_NLANE; k++)
        b[k >> 2] |= (uint8_t)((alt_seq.c[c].srat[k] & 3u) << ((k & 3u) * 2u));
}
static void alt_rat_put(uint32_t c, const uint8_t *b)
{
    uint32_t k;
    for (k = 0; k < AS_NLANE; k++)
        alt_seq.c[c].srat[k] = (uint8_t)((b[k >> 2] >> ((k & 3u) * 2u)) & 3u);
}

/* a plain rhythm: LEN HITS ROT PROB, every step normal and sure */
static void alt_rhy_plain(alt_rhy_t *r, uint32_t len, uint32_t hits, uint32_t rot, uint32_t prob)
{
    uint32_t k;
    for (k = 0; k < sizeof *r; k++)
        ((uint8_t *)r)[k] = 0;
    r->len = (uint8_t)len, r->hits = (uint8_t)hits, r->rot = (uint8_t)rot, r->prob = (uint8_t)prob;
    r->type = AS_T_EUCLID, r->tlock = 100, r->tdens = 64, r->trange = 4;
    for (k = 0; k < AS_MAXLEN / 2u; k++)
        r->chance[k] = 0xFF;
    for (k = 0; k < AS_NLANE / 2u; k++) {
        r->vel[k] = (uint8_t)(alt_q4(96) * 0x11u);   /* normal */
        r->pitch[k] = 0x88;                      /* 0 */
    }
}

/* ---- the scene, in and out ----------------------------------------------------------------------------------- */
static int32_t alt_base(uint32_t c, uint32_t p)  /* a parameter without the die's offset */
{
    int32_t sl = alt_slot(p);
    return trk[c].p[p] - (sl >= 0 ? alt_applied[c][sl] : 0);
}

static void alt_scene_get(alt_scene_rec_t *s, alt_rhy_t *alt_scene_rhy)
{
    uint32_t c, k;
    s->used = 1;
    s->bpm = (uint8_t)clamp(song.g[G_BPM], 40, 240);
    s->scale = alt_seq.scale;
    s->swing = (uint8_t)song.g[G_SWING];
    for (c = 0; c < AS_NCIRC; c++) {
        alt_scirc_t *o = &s->c[c];
        const track_t *t = &trk[c];
        o->eng = 0;
        for (k = 0; k < ALT_NENG; k++)
            if (t->eng_req == ALT_ENGS[k])
                o->eng = (uint8_t)k;
        o->pre = au.pre[c];
        o->mode = alt_emode[c];
        o->locks = alt_seq.c[c].locks;
        o->dose = au.cfg[c].dose;
        o->root = au.cfg[c].root;
        o->chord = au.cfg[c].chord;
        o->rsv = 0;
        for (k = 0; k < 8u; k++)
            o->e[k] = (int8_t)clamp(alt_base(c, P_E0 + k), -128, 127);
        for (k = 0; k < ALT_NSP; k++)
            o->p[k] = (int8_t)clamp(alt_base(c, ALT_SP[k]), -128, 127);
        alt_rhy_get(c, &alt_scene_rhy[c]);   /* (the rhythm: its own object, alt_bank_r) */
        for (k = 0; k < CL_N; k++) {
            o->lfo[3u * k] = (uint8_t)(alt_cl[c][k].wave | alt_cl[c][k].rate << 3);
            o->lfo[3u * k + 1u] = alt_cl[c][k].dst;
            o->lfo[3u * k + 2u] = (uint8_t)alt_cl[c][k].depth;
        }
        for (k = 0; k < NMSLOT; k++) {
            uint32_t w = ((uint32_t)t->p[P_M1SRC + 3u * k] & 15u) | ((uint32_t)t->p[P_M1DST + 3u * k] & 31u) << 4 |
                         ((uint32_t)(t->p[P_M1AMT + 3u * k] + 64) & 127u) << 9;
            o->m[2u * k] = (uint8_t)w;
            o->m[2u * k + 1u] = (uint8_t)(w >> 8);
        }
    }
    s->gwave = alt_gl.wave;
    s->grate = alt_gl.rate;
    s->gdst = alt_gl.dst[0];
    s->gamt = alt_gl.amt[0];
}

static void alt_scene_put(const alt_scene_rec_t *s, const alt_rhy_t *rhy)
{
    uint32_t c, k;
    song.g[G_BPM] = s->bpm;
    song.g[G_SWING] = s->swing;
    alt_seq.scale = (uint8_t)(s->scale % AS_NSCALE);
    for (c = 0; c < AS_NCIRC; c++) {
        const alt_scirc_t *o = &s->c[c];
        track_t *t = &trk[c];
        alt_load_sound(c, o->pre);               /* the engine and everything else at a known place .. */
        if (t->eng_req != ALT_ENGS[o->eng % ALT_NENG])
            set_engine_of(t, ALT_ENGS[o->eng % ALT_NENG]);
        fm1_irq_off();                           /* .. then the scene's own values */
        for (k = 0; k < 8u; k++)
            t->p[P_E0 + k] = o->e[k];
        for (k = 0; k < ALT_NSP; k++)
            t->p[ALT_SP[k]] = o->p[k];
        t->p[P_LD_PIT] = t->p[P_LD_FLT] = t->p[P_LD_SHP] = t->p[P_LD_AMP] = 0;   /* (the LFOs are ours: alt_cl) */
        for (k = 0; k < CL_N; k++) {
            alt_cl[c][k].wave = (uint8_t)((o->lfo[3u * k] & 7u) % GW_N);
            alt_cl[c][k].rate = (uint8_t)((o->lfo[3u * k] >> 3) % GL_NRATE);
            alt_cl[c][k].dst = (uint8_t)(o->lfo[3u * k + 1u] % GD_N);
            alt_cl[c][k].depth = (int8_t)clamp((int8_t)o->lfo[3u * k + 2u], -64, 63);
        }
        for (k = 0; k < NMSLOT; k++) {
            uint32_t w = o->m[2u * k] | (uint32_t)o->m[2u * k + 1u] << 8;
            t->p[P_M1SRC + 3u * k] = (int16_t)((w & 15u) < MS_N ? w & 15u : 0u);
            t->p[P_M1DST + 3u * k] = (int16_t)((w >> 4 & 31u) < MD_N ? w >> 4 & 31u : 0u);
            t->p[P_M1AMT + 3u * k] = (int16_t)((int32_t)(w >> 9 & 127u) - 64);
        }
        for (k = 0; k < ALT_NSLOT; k++)
            alt_applied[c][k] = 0;
        fm1_irq_on();
        alt_emode[c] = (uint8_t)(o->mode % AM_N);
        alt_seq.c[c].locks = o->locks;
        au.cfg[c].dose = o->dose;
        au.cfg[c].root = o->root;
        au.cfg[c].chord = (uint8_t)(o->chord % AS_NCHORD);
        au.cfg[c].mute = 0;
        alt_rhy_put(c, &rhy[c]);
    }
    alt_gl.wave = (uint8_t)(s->gwave % GW_N);
    alt_gl.rate = (uint8_t)(s->grate % GL_NRATE);
    alt_gl.dst[0] = (uint8_t)(s->gdst % GD_N);
    alt_gl.amt[0] = (int8_t)clamp(s->gamt, -64, 63);
}

/* factory scene n (alter_sounds.c ALT_SCENE) */
static void alt_load_scene(uint32_t n)
{
    const alt_scene_t *sc = &ALT_SCENE[n % ALT_NSCENE];
    uint32_t c;
    afl_off();                                   /* (the factory scenes: no MASTER FILTER, no added steps) */
    for (c = 0; c < AS_NCIRC; c++) {
        alt_add_set(c, 0);
        alt_fxr_put(c, 0);
    }
    if (sc->user) {                              /* a USER scene of Romain's, as he stored it (alter_ui.c alt_dump_user) */
        static alt_scene_rec_t rec;
        static alt_rhy_t rhy[AS_NCIRC];
        const uint8_t *b = sc->user;
        uint32_t k;
        for (k = 0; k < sizeof rec; k++)
            ((uint8_t *)&rec)[k] = *b++;
        for (k = 0; k < sizeof rhy; k++)
            ((uint8_t *)rhy)[k] = *b++;
        alt_scene_put(&rec, rhy);
        if (*b++)
            alt_fx_put(b);
        b += ALT_FXREC;
        for (c = 0; c < AS_NCIRC; c++)
            for (k = 0; k < AE_N; k++)
                alt_envp[c][k] = b[0] ? b[1 + c * AE_N + k] : AE_DEF[k];
        return;
    }
    song.g[G_BPM] = sc->bpm;
    song.g[G_SWING] = sc->swing;
    alt_seq.scale = sc->scale;
    afp_dly_apply(sc->dpre);                     /* the FX page's shared ones (each circle's DIST: its sound's) */
    afp_rev_apply(sc->rpre);
    afp_mst_apply(sc->mpre);
    alt_gl_reset();
    for (c = 0; c < AS_NCIRC; c++) {
        as_cfg_t *cf = &au.cfg[c];
        alt_rhy_t r;
        alt_load_sound(c, alt_pre_find(sc->c[c].pre));
        trk[c].p[P_LEVEL] = (int16_t)clamp(trk[c].p[P_LEVEL] + sc->c[c].gain, 1, 127);
        if (sc->c[c].dly)
            trk[c].p[P_DLY] = sc->c[c].dly;
        alt_emode[c] = sc->c[c].mode;
        cf->root = sc->c[c].root;
        cf->chord = sc->c[c].chord;
        cf->mute = 0;
        cf->dose = AS_DOSE_DEF;
        alt_seq.c[c].locks = 0;
        alt_env_default(c);
        if (sc->c[c].phold)
            alt_envp[c][AE_PHOLD] = sc->c[c].phold;
        if (sc->c[c].pdec)
            alt_envp[c][AE_PDEC] = sc->c[c].pdec;
        alt_rhy_plain(&r, sc->c[c].len, sc->c[c].hits, sc->c[c].rot, sc->c[c].prob);
        alt_rhy_put(c, &r);
        if (sc->c[c].lstep)
            alt_seq.c[c].sdly[(sc->c[c].lstep - 1u) % AS_NLANE] = sc->c[c].ldly;
    }
}

/* NEW SCENE: the three engines' first sounds, no hits, FREE, 120 BPM, minor */
static void alt_new_scene(void)
{
    static const char *const PRE[AS_NCIRC] = {"THUD", "TOMS", "HAT"};
    static const uint8_t ROOT[AS_NCIRC] = {36, 48, 60};
    uint32_t c;
    afl_off();
    for (c = 0; c < AS_NCIRC; c++) {
        alt_add_set(c, 0);
        alt_fxr_put(c, 0);
    }
    song.g[G_BPM] = 120;
    song.g[G_SWING] = 0;
    alt_seq.scale = AS_SC_MIN;
    afp_dly_apply(0);                            /* ECHO, HALL, FLAT */
    afp_rev_apply(0);
    afp_mst_apply(0);
    alt_gl_reset();
    for (c = 0; c < AS_NCIRC; c++) {
        alt_rhy_t r;
        alt_load_sound(c, alt_pre_find(PRE[c]));
        alt_emode[c] = AM_FREE;
        au.cfg[c].root = ROOT[c];
        au.cfg[c].chord = AS_CH_OFF;
        au.cfg[c].mute = 0;
        au.cfg[c].dose = AS_DOSE_DEF;
        alt_seq.c[c].locks = 0;
        alt_env_default(c);
        alt_rhy_plain(&r, 16, 0, 0, 127);
        alt_rhy_put(c, &r);
    }
}

/* ---- the banks on the flash ---------------------------------------------------------------------------------- */
static void alt_banks_load(void)
{
    if (st_load(ALT_OBJ_SCENES, &alt_bank_s, sizeof alt_bank_s) != (int)sizeof alt_bank_s || alt_bank_s.magic != ALT_MAGIC_S) {
        uint32_t i;
        for (i = 0; i < sizeof alt_bank_s; i++)
            ((uint8_t *)&alt_bank_s)[i] = 0;
        alt_bank_s.magic = ALT_MAGIC_S;
    }
    {
        int n = st_load(ALT_OBJ_SRHY, &alt_bank_r, sizeof alt_bank_r);
        uint32_t i;
        if (n == (int)ALT_BANK_R_OLD || n == (int)ALT_BANK_R_FX) {   /* (stored before the effects or the RATCH lane) */
            for (i = (uint32_t)n; i < sizeof alt_bank_r; i++)
                ((uint8_t *)&alt_bank_r)[i] = 0;
        } else if (n != (int)sizeof alt_bank_r)
            alt_bank_r.magic = 0;
    }
    if (alt_bank_r.magic != ALT_MAGIC_R || alt_bank_s.magic != ALT_MAGIC_S) {   /* (no rhythms for the scenes: the scenes go too) */
        uint32_t i;
        for (i = 0; i < sizeof alt_bank_r; i++)
            ((uint8_t *)&alt_bank_r)[i] = 0;
        alt_bank_r.magic = ALT_MAGIC_R;
        for (i = 0; i < ALT_NUSER; i++)
            alt_bank_s.s[i].used = 0;
    }
    {
        int n = st_load(ALT_OBJ_PATS, &alt_bank_p, sizeof alt_bank_p);
        uint32_t i;
        if (n == (int)ALT_BANK_P_OLD)            /* (stored before the RATCH lane) */
            for (i = (uint32_t)n; i < sizeof alt_bank_p; i++)
                ((uint8_t *)&alt_bank_p)[i] = 0;
        else if (n != (int)sizeof alt_bank_p)
            alt_bank_p.magic = 0;
    }
    if (alt_bank_p.magic != ALT_MAGIC_P) {
        uint32_t i;
        for (i = 0; i < sizeof alt_bank_p; i++)
            ((uint8_t *)&alt_bank_p)[i] = 0;
        alt_bank_p.magic = ALT_MAGIC_P;
    }
    {
        int n = st_load(ALT_OBJ_FILT, &alt_bank_f, sizeof alt_bank_f);
        uint32_t i;
        if (n == (int)ALT_BANK_F_OLD || n == (int)ALT_BANK_F_ADD)   /* (stored before the added steps, the fills) */
            for (i = (uint32_t)n; i < sizeof alt_bank_f; i++)
                ((uint8_t *)&alt_bank_f)[i] = 0;
        else if (n != (int)sizeof alt_bank_f)
            alt_bank_f.magic = 0;
    }
    if (alt_bank_f.magic != ALT_MAGIC_F) {       /* (none yet, or an old Felucca project there) */
        uint32_t i;
        for (i = 0; i < sizeof alt_bank_f; i++)
            ((uint8_t *)&alt_bank_f)[i] = 0;
        alt_bank_f.magic = ALT_MAGIC_F;
    }
}

/* ---- the list: row i of tab t (0 SCENES, 1 PATTERNS) --------------------------------------------------------- */
static const char *alt_row_name(uint32_t t, uint32_t i, char *buf)
{
    static const char *const NEW[2] = {"NEW SCENE", "NEW PATTERN"};
    if (i == 0)
        return NEW[t & 1u];
    if (i <= ALT_NFACT)
        return t ? ALT_FPAT[i - 1u].name : ALT_SCENE[i - 1u].name;
    {
        uint32_t u = i - 1u - ALT_NFACT, used = t ? alt_bank_p.p[u].used : alt_bank_s.s[u].used, n = 0;
        const char *w = used ? "USER " : "EMPTY ";
        while (*w)
            buf[n++] = *w++;
        buf[n++] = (char)('0' + (u + 1u) / 10u);
        buf[n++] = (char)('0' + (u + 1u) % 10u);
        buf[n] = 0;
        return buf;
    }
}

static const char *alt_row_about(uint32_t t, uint32_t i)
{
    if (i == 0)
        return t ? "NO HITS ON THE THREE CIRCLES" : "A BLANK START. LOAD ONLY";
    if (i <= ALT_NFACT)
        return t ? ALT_FPAT[i - 1u].about : ALT_SCENE[i - 1u].about;
    return (t ? alt_bank_p.p[i - 1u - ALT_NFACT].used : alt_bank_s.s[i - 1u - ALT_NFACT].used) ? "YOURS" : "HOLD SAVE: STORE HERE";
}

/* SAVE tapped: load row i of tab t; 0 = nothing there */
static int alt_row_load(uint32_t t, uint32_t i)
{
    uint32_t c;
    if (t == 0) {
        if (i == 0)
            alt_new_scene();
        else if (i <= ALT_NFACT)
            alt_load_scene(i - 1u);
        else if (alt_bank_s.s[i - 1u - ALT_NFACT].used) {
            uint32_t u = i - 1u - ALT_NFACT, k;
            alt_scene_put(&alt_bank_s.s[u], alt_bank_r.r[u]);
            for (c = 0; c < AS_NCIRC; c++)       /* its envelope modes' settings (or the defaults: stored before them) */
                for (k = 0; k < AE_N; k++)
                    alt_envp[c][k] = alt_bank_p.envok[u] ? alt_bank_p.env[u][c][k] : AE_DEF[k];
            if (alt_bank_r.fxok[u])              /* its effects (stored before them: as they are) */
                alt_fx_put(alt_bank_r.fx[u]);
            for (c = 0; c < AS_NCIRC; c++)       /* its RATCH lanes */
                alt_rat_put(c, alt_bank_r.rat[u][c]);
            if (alt_bank_f.used[u])              /* its MASTER FILTER, or none */
                afl_put(alt_bank_f.r[u]);
            else
                afl_off();
            for (c = 0; c < AS_NCIRC; c++) {     /* its added steps, its REC'd fills */
                alt_add_set(c, alt_bank_f.add[u][c]);
                alt_fxr_put(c, alt_bank_f.fx[u][c]);
            }
        } else
            return 0;
        return 1;
    }
    for (c = 0; c < AS_NCIRC; c++) {
        alt_rhy_t r;
        if (i == 0)
            alt_rhy_plain(&r, 16, 0, 0, 127);
        else if (i <= ALT_NFACT)
            alt_rhy_plain(&r, ALT_FPAT[i - 1u].c[c][0], ALT_FPAT[i - 1u].c[c][1], ALT_FPAT[i - 1u].c[c][2],
                          ALT_FPAT[i - 1u].c[c][3]);
        else if (alt_bank_p.p[i - 1u - ALT_NFACT].used)
            r = alt_bank_p.p[i - 1u - ALT_NFACT].c[c];
        else
            return 0;
        alt_add_set(c, 0);
        alt_fxr_put(c, 0);                       /* (a pattern: the added steps and the REC'd fills go) */
        alt_rhy_put(c, &r);
        if (i > ALT_NFACT)
            alt_rat_put(c, alt_bank_p.prat[i - 1u - ALT_NFACT][c]);
    }
    return 1;
}

/* SAVE held: store into row i of tab t; 1 saved, 0 protected (NEW, factory), -1 the flash failed */
static int alt_row_save(uint32_t t, uint32_t i)
{
    uint32_t u, c;
    if (i <= ALT_NFACT)
        return 0;
    u = i - 1u - ALT_NFACT;
    if (t == 0) {
        uint32_t k;
        for (c = 0; c < AS_NCIRC; c++)           /* a circle's alignment gain folded into its LEVEL (a scene keeps */
            if (ac_tg[c] != AC_UNITY) {          /* LEVEL only): it reloads as loud as it plays */
                int32_t want = (int32_t)(((int64_t)LEVEL_Q12[trk[c].p[P_LEVEL] & 127] * ac_tg[c]) >> 15), best = 1, j;
                for (j = 1; j < 128; j++)
                    if ((LEVEL_Q12[j] > want ? LEVEL_Q12[j] - want : want - LEVEL_Q12[j]) <
                        (LEVEL_Q12[best] > want ? LEVEL_Q12[best] - want : want - LEVEL_Q12[best]))
                        best = j;
                fm1_irq_off();
                trk[c].p[P_LEVEL] = (int16_t)best;
                fm1_irq_on();
                alt_cal_to(c, AC_UNITY);
            }
        alt_scene_get(&alt_bank_s.s[u], alt_bank_r.r[u]);
        alt_bank_p.envok[u] = 1;
        alt_bank_r.fxok[u] = 1;
        alt_fx_get(alt_bank_r.fx[u]);
        alt_bank_f.used[u] = (uint8_t)afl_active();   /* the MASTER FILTER: kept when one is on */
        for (c = 0; c < AS_NCIRC; c++) {
            uint32_t k;
            alt_bank_f.add[u][c] = alt_seq.c[c].add;
            for (k = 0; k < AS_MAXLEN; k++)
                alt_bank_f.fx[u][c][k] = alt_fxr[c][k];
        }
        if (alt_bank_f.used[u])
            afl_get(alt_bank_f.r[u]);
        for (c = 0; c < AS_NCIRC; c++)
            alt_rat_get(c, alt_bank_r.rat[u][c]);
        for (c = 0; c < AS_NCIRC; c++)
            for (k = 0; k < AE_N; k++)
                alt_bank_p.env[u][c][k] = alt_envp[c][k];
        return st_save(ALT_OBJ_SCENES, &alt_bank_s, sizeof alt_bank_s) == 0 &&
               st_save(ALT_OBJ_SRHY, &alt_bank_r, sizeof alt_bank_r) == 0 &&
               st_save(ALT_OBJ_PATS, &alt_bank_p, sizeof alt_bank_p) == 0 &&
               st_save(ALT_OBJ_FILT, &alt_bank_f, sizeof alt_bank_f) == 0 ? 1 : -1;
    }
    alt_bank_p.p[u].used = 1;
    for (c = 0; c < AS_NCIRC; c++) {
        alt_rhy_get(c, &alt_bank_p.p[u].c[c]);
        alt_rat_get(c, alt_bank_p.prat[u][c]);
    }
    return st_save(ALT_OBJ_PATS, &alt_bank_p, sizeof alt_bank_p) == 0 ? 1 : -1;
}
