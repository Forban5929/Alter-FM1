/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's SAVE / LOAD (included by alter_ui.c after alter_sounds.c). Two tabs, each a list:
 *   NEW (load only: a blank scene, or an empty pattern), 4 factory ones (protected), 16 USER slots.
 * A SCENE is everything: per circle its sound (engine, E1..E8, ADSR, sends, insert, level, pan, glide, LFO), envelope
 * mode, locks and rhythm (LEN HITS ROT PROB FROT, DOSE, ROOT, CHORD, each step's force and chance); and the scale,
 * tempo and swing. A PATTERN is the rhythm only (per circle LEN HITS ROT PROB FROT, force and chance per step):
 * loading one leaves the sounds, notes and everything else as they are.
 * The USER slots live in RAM (alt_bank_s, alt_bank_p) and on the flash, in Felucca's project objects 3 and 4
 * (storage.c OBJ_PROJECT0 + 2 / + 3; an own magic tells ours from a Felucca project saved there). Packed small: a
 * step's force in 2 bits, its chance in 4 (16 levels). */

#define ALT_NUSER 16
#define ALT_NFACT 4
#define ALT_NLIST (1 + ALT_NFACT + ALT_NUSER)   /* NEW, the factory ones, the USER slots */
#define ALT_OBJ_SCENES 3u                        /* storage.c OBJ_PROJECT0 + 2 (OBJ_PROJECT0 = 1) */
#define ALT_OBJ_PATS 4u                          /* OBJ_PROJECT0 + 3 */
#define ALT_MAGIC_S 0x534C5441u                  /* "ATLS" */
#define ALT_MAGIC_P 0x504C5441u                  /* "ATLP" */
static int st_load(uint32_t obj, void *dst, uint32_t max);   /* (storage.c, included later) */
static int st_save(uint32_t obj, const void *src, uint32_t len);

/* the part's parameters a scene keeps besides E1..E8 (each fits a signed byte) */
static const uint8_t ALT_SP[] = {P_ATK, P_DEC, P_SUS, P_REL, P_DIST, P_CHOR, P_DLY, P_REV, P_ITYPE, P_IA, P_IMIX,
                                 P_LEVEL, P_PAN, P_GLIDE, P_LRATE, P_LD_PIT, P_LD_FLT, P_LD_AMP};
#define ALT_NSP (sizeof ALT_SP)

typedef struct {                 /* a circle's rhythm */
    uint8_t len, hits, rot, prob, frot;
    uint8_t force[AS_MAXLEN / 4];                /* 2 bits a step */
    uint8_t chance[AS_MAXLEN / 2];               /* 4 bits a step (x 127 / 15) */
} alt_rhy_t;

typedef struct {                 /* a circle in a scene */
    uint8_t eng, pre, mode, locks, dose, root, chord, rsv;
    int8_t e[8];
    int8_t p[ALT_NSP];
    alt_rhy_t r;
} alt_scirc_t;

typedef struct {
    uint8_t used, bpm, scale, swing;
    alt_scirc_t c[AS_NCIRC];
} alt_scene_rec_t;

typedef struct {
    uint8_t used, rsv[3];
    alt_rhy_t c[AS_NCIRC];
} alt_pat_rec_t;

static struct { uint32_t magic; alt_scene_rec_t s[ALT_NUSER]; } alt_bank_s;
static struct { uint32_t magic; alt_pat_rec_t p[ALT_NUSER]; } alt_bank_p;
_Static_assert(sizeof alt_bank_s <= 3840u && sizeof alt_bank_p <= 3840u, "ALTER's banks fit a storage object");

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
static void alt_rhy_get(uint32_t c, alt_rhy_t *r)
{
    const as_cfg_t *cf = &au.cfg[c];
    const as_circ_t *sc = &alt_seq.c[c];
    uint32_t k;
    r->len = cf->len, r->hits = cf->hits, r->rot = cf->rot, r->prob = cf->prob, r->frot = cf->frot;
    for (k = 0; k < AS_MAXLEN / 4u; k++)
        r->force[k] = 0;
    for (k = 0; k < AS_MAXLEN / 2u; k++)
        r->chance[k] = 0;
    for (k = 0; k < AS_MAXLEN; k++) {
        r->force[k >> 2] |= (uint8_t)((sc->force[k] & 3u) << ((k & 3u) * 2u));
        r->chance[k >> 1] |= (uint8_t)(((sc->chance[k] * 15u + 63u) / 127u & 15u) << ((k & 1u) * 4u));
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
    for (k = 0; k < AS_MAXLEN; k++) {
        uint32_t f = (r->force[k >> 2] >> ((k & 3u) * 2u)) & 3u;
        sc->force[k] = (uint8_t)(f > AS_ACCENT ? AS_NORMAL : f);
        sc->chance[k] = (uint8_t)(((r->chance[k >> 1] >> ((k & 1u) * 4u)) & 15u) * 127u / 15u);
    }
    alt_post_now |= (uint8_t)(1u << c);
    alt_post_circle(c);
    alt_clear_cnt[c]++;
}

/* a plain rhythm: LEN HITS ROT PROB, every step normal and sure */
static void alt_rhy_plain(alt_rhy_t *r, uint32_t len, uint32_t hits, uint32_t rot, uint32_t prob)
{
    uint32_t k;
    r->len = (uint8_t)len, r->hits = (uint8_t)hits, r->rot = (uint8_t)rot, r->prob = (uint8_t)prob, r->frot = 0;
    for (k = 0; k < AS_MAXLEN / 4u; k++)
        r->force[k] = 0x55;                      /* AS_NORMAL (1) in each 2 bits */
    for (k = 0; k < AS_MAXLEN / 2u; k++)
        r->chance[k] = 0xFF;
}

/* ---- the scene, in and out ----------------------------------------------------------------------------------- */
static int32_t alt_base(uint32_t c, uint32_t p)  /* a parameter without the die's offset */
{
    int32_t sl = alt_slot(p);
    return trk[c].p[p] - (sl >= 0 ? alt_applied[c][sl] : 0);
}

static void alt_scene_get(alt_scene_rec_t *s)
{
    uint32_t c, k;
    s->used = 1;
    s->bpm = (uint8_t)clamp(song.g[G_BPM], 40, 240);
    s->scale = alt_seq.scale;
    s->swing = (uint8_t)song.g[G_SWING];
    for (c = 0; c < AS_NCIRC; c++) {
        alt_scirc_t *o = &s->c[c];
        const track_t *t = &trk[c];
        o->eng = (uint8_t)(t->eng_req == ALT_ENGS[1] ? 1 : t->eng_req == ALT_ENGS[2] ? 2 : 0);
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
        alt_rhy_get(c, &o->r);
    }
}

static void alt_scene_put(const alt_scene_rec_t *s)
{
    uint32_t c, k;
    song.g[G_BPM] = s->bpm;
    song.g[G_SWING] = s->swing;
    alt_seq.scale = (uint8_t)(s->scale % AS_NSCALE);
    for (c = 0; c < AS_NCIRC; c++) {
        const alt_scirc_t *o = &s->c[c];
        track_t *t = &trk[c];
        alt_load_sound(c, o->pre);               /* the engine and everything else at a known place .. */
        if (t->eng_req != ALT_ENGS[o->eng % AS_NCIRC])
            set_engine_of(t, ALT_ENGS[o->eng % AS_NCIRC]);
        fm1_irq_off();                           /* .. then the scene's own values */
        for (k = 0; k < 8u; k++)
            t->p[P_E0 + k] = o->e[k];
        for (k = 0; k < ALT_NSP; k++)
            t->p[ALT_SP[k]] = o->p[k];
        for (k = 0; k < ALT_NSLOT; k++)
            alt_applied[c][k] = 0;
        fm1_irq_on();
        alt_emode[c] = (uint8_t)(o->mode % AM_N);
        alt_seq.c[c].locks = o->locks;
        au.cfg[c].dose = o->dose;
        au.cfg[c].root = o->root;
        au.cfg[c].chord = (uint8_t)(o->chord % AS_NCHORD);
        au.cfg[c].mute = 0;
        alt_rhy_put(c, &o->r);
    }
}

/* factory scene n (alter_sounds.c ALT_SCENE) */
static void alt_load_scene(uint32_t n)
{
    const alt_scene_t *sc = &ALT_SCENE[n % ALT_NSCENE];
    uint32_t c;
    song.g[G_BPM] = sc->bpm;
    song.g[G_SWING] = sc->swing;
    alt_seq.scale = sc->scale;
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
        alt_rhy_plain(&r, sc->c[c].len, sc->c[c].hits, sc->c[c].rot, sc->c[c].prob);
        alt_rhy_put(c, &r);
    }
}

/* NEW SCENE: the three engines' first sounds, no hits, FREE, 120 BPM, minor */
static void alt_new_scene(void)
{
    static const char *const PRE[AS_NCIRC] = {"THUD", "TOMS", "HAT"};
    static const uint8_t ROOT[AS_NCIRC] = {36, 48, 60};
    uint32_t c;
    song.g[G_BPM] = 120;
    song.g[G_SWING] = 0;
    alt_seq.scale = AS_SC_MIN;
    for (c = 0; c < AS_NCIRC; c++) {
        alt_rhy_t r;
        alt_load_sound(c, alt_pre_find(PRE[c]));
        alt_emode[c] = AM_FREE;
        au.cfg[c].root = ROOT[c];
        au.cfg[c].chord = AS_CH_OFF;
        au.cfg[c].mute = 0;
        au.cfg[c].dose = AS_DOSE_DEF;
        alt_seq.c[c].locks = 0;
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
    if (st_load(ALT_OBJ_PATS, &alt_bank_p, sizeof alt_bank_p) != (int)sizeof alt_bank_p || alt_bank_p.magic != ALT_MAGIC_P) {
        uint32_t i;
        for (i = 0; i < sizeof alt_bank_p; i++)
            ((uint8_t *)&alt_bank_p)[i] = 0;
        alt_bank_p.magic = ALT_MAGIC_P;
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
        else if (alt_bank_s.s[i - 1u - ALT_NFACT].used)
            alt_scene_put(&alt_bank_s.s[i - 1u - ALT_NFACT]);
        else
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
        alt_rhy_put(c, &r);
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
        alt_scene_get(&alt_bank_s.s[u]);
        return st_save(ALT_OBJ_SCENES, &alt_bank_s, sizeof alt_bank_s) == 0 ? 1 : -1;
    }
    alt_bank_p.p[u].used = 1;
    for (c = 0; c < AS_NCIRC; c++)
        alt_rhy_get(c, &alt_bank_p.p[u].c[c]);
    return st_save(ALT_OBJ_PATS, &alt_bank_p, sizeof alt_bank_p) == 0 ? 1 : -1;
}
