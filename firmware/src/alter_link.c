/* SPDX-License-Identifier: GPL-3.0-only */
/* ALTER LINK: the companion page's SysEx over USB-MIDI (docs/alter/LINK.md). Included after alter_ui.c / alter_store.c
 * (and, on the FM-1, after ota.c: frames come from ota_frame_get, replies leave through ota_wire_send).
 *   F0 7D 41 4C cmd args.. F7       (7D = non-commercial ID, "AL"; Felucca's old editor said "FL": not ours)
 * Values: 2 bytes, LSB first, + 8192 (-8192..8191). Every valid request gets one reply, same cmd. While the page
 * watches (WATCH, kept by any request within 3 s), the device pushes CHANGED and RELOAD.
 * A circle's sound = its part's parameters (cercle c -> partie c): the values here are the BASE ones (alter_store.c
 * alt_base: without the die's offset), the ones a USER scene stores. */

#define LK_H0 0x7D
#define LK_H1 0x41
#define LK_H2 0x4C
#define LK_VERSION "ALTER LINK 4"
enum { LKC_INFO = 1, LKC_STATE, LKC_DESC, LKC_SET, LKC_SOUND, LKC_SCENE, LKC_WATCH, LKC_CHANGED, LKC_RELOAD, LKC_PING,
       /* v2 (04/11): the other settings (LK_V), the lanes, the actions, the circles as they play */
       LKC_VLIST, LKC_VDESC, LKC_VALL, LKC_VSET, LKC_LANE, LKC_LSET, LKC_ACT, LKC_PLAY, LKC_VCHANGED, LKC_LCHANGED,
       /* v3 (05/11): the USER patterns (as SCENE; gone 10/11, no reply), SONG */
       LKC_PAT, LKC_SONG,
       /* 10/11: a whole scene to and from the page's file (lk_blob) */
       LKC_BLOB };

/* the sound's parameters, in this order (STATE): the engine's eight, then what a USER scene keeps (ALT_SP) */
static const uint8_t LK_IDS[] = {P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7, P_ATK, P_DEC, P_SUS, P_REL,
                                 P_DIST, P_CHOR, P_DLY, P_REV, P_ITYPE, P_IA, P_IMIX, P_LEVEL, P_PAN, P_GLIDE};
#define LK_NID (sizeof LK_IDS)

static uint8_t lk_out[1400];
static uint32_t lk_n;
static struct {
    uint8_t on, was;                 /* watched now; asked for by the page (a PING after a silence: on again) */
    uint32_t last_ms, pass_ms;
    int16_t sh[AS_NCIRC][LK_NID];    /* what the page knows */
    uint8_t eng[AS_NCIRC], pre[AS_NCIRC];
    uint32_t pcnt[AS_NCIRC];
    uint8_t v2;                      /* v2: the page asked for the settings (VLIST): their pushes too */
    int16_t vs[AS_NCIRC + 1][48];    /* .. what it knows of them (a circle's; [3] the shared ones) .. */
    uint8_t ls[AS_NCIRC][6][16];     /* .. of the lanes */
    uint32_t play_ms, play_sig;      /* the last PLAY pushed */
} lk;

static void lk_begin(uint32_t cmd)
{
    lk_out[0] = 0xF0;
    lk_out[1] = LK_H0;
    lk_out[2] = LK_H1;
    lk_out[3] = LK_H2;
    lk_out[4] = (uint8_t)cmd;
    lk_n = 5;
}
static void lk_b(uint32_t v)
{
    if (lk_n < sizeof lk_out - 1u)
        lk_out[lk_n++] = (uint8_t)(v & 0x7Fu);
}
static void lk_v(int32_t v)
{
    uint32_t u = (uint32_t)(clamp(v, -8192, 8191) + 8192);
    lk_b(u);
    lk_b(u >> 7);
}
static void lk_str(const char *s)
{
    uint32_t i;
    for (i = 0; s && s[i] && i < 24u; i++)
        lk_b((uint8_t)s[i]);
    lk_b(0);
}
static int32_t lk_rv(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 7) - 8192; }

#ifdef FELUCCA_WEB
static uint8_t lk_web[4096];            /* the replies, for web_link_out (felucca_web.c) */
static uint32_t lk_web_n;
static void lk_send(void)
{
    uint32_t i;
    lk_out[lk_n++] = 0xF7;
    for (i = 0; i < lk_n && lk_web_n < sizeof lk_web; i++)
        lk_web[lk_web_n++] = lk_out[i];
}
static int lk_room(void) { return lk_web_n + 64u < sizeof lk_web; }
#else
static void lk_send(void)
{
    lk_out[lk_n++] = 0xF7;
    ota_wire_send(lk_out, lk_n);
}
static int lk_room(void) { return so_w - so_r + 8u <= SXQ / 2u; }
#endif

static uint32_t lk_engi(uint32_t c)              /* the circle's engine as ALT_ENGS' index */
{
    uint32_t k;
    for (k = 0; k < ALT_NENG; k++)
        if (trk[c].eng_req == ALT_ENGS[k])
            return k;
    return 0;
}
static const param_desc_t *lk_desc(uint32_t c, uint32_t id) { return param_desc_of(eng_idx(trk[c].eng_req), id); }
static int lk_known(uint32_t id)
{
    uint32_t k;
    for (k = 0; k < LK_NID; k++)
        if (LK_IDS[k] == id)
            return 1;
    return 0;
}

static void lk_shadow(uint32_t c)                /* the page is in sync with circle c */
{
    uint32_t k;
    for (k = 0; k < LK_NID; k++)
        lk.sh[c][k] = (int16_t)alt_base(c, LK_IDS[k]);
    lk.eng[c] = (uint8_t)trk[c].eng_req;
    lk.pre[c] = au.pre[c];
    lk.pcnt[c] = alt_preset_cnt[c];
}

static void lk_state(uint32_t c)
{
    uint32_t k;
    lk_begin(LKC_STATE);
    lk_b(c);
    lk_b(lk_engi(c));
    lk_b(au.pre[c]);
    lk_b(alt_emode[c]);
    lk_b(LK_NID);
    for (k = 0; k < LK_NID; k++)
        lk_v(alt_base(c, LK_IDS[k]));
    lk_send();
    lk_shadow(c);
}

/* a sound parameter of circle c to v (its base; the die's offset stays on top); the value kept */
static int32_t lk_set(uint32_t c, uint32_t id, int32_t v)
{
    const param_desc_t *d = lk_desc(c, id);
    int32_t sl = alt_slot(id), off = sl >= 0 ? alt_applied[c][sl] : 0, lo = d ? d->min : 0, hi = d ? d->max : 127;
    v = clamp(v, lo, hi);
    fm1_irq_off();
    trk[c].p[id] = (int16_t)clamp(v + off, lo, hi);
    fm1_irq_on();
    return v;
}


/* ---- v2: the other settings, by number (LK_V): a circle's (scope 1: VALL / VSET with the circle) or shared ------- */
enum { LV_CFG, LV_MODE, LV_LFO, LV_MTX, LV_SCALE, LV_GL, LV_G, LV_AMX, LV_AFL, LV_MORPH, LV_CONS, LV_PAR, LV_RMB, LV_VOL };
/* (LV_CONS: the CONSOLE's acs_v[c][ref]; LV_PAR: the sound's parameter ref, its base, as SET: the CONSOLE's DRIVE,
 * DELAY, REVERB, LEVEL; LINK 4, 07/11; LV_RMB: the circle's RUMBLE alt_rmb, 08/11; LV_VOL: the MIXER's MASTER gain,
 * alt_mgain, 100 = 0 dB, 10/11) */
enum { LF_TYPE, LF_LEN, LF_HITS, LF_ROT, LF_PROB, LF_FROT, LF_TLOCK, LF_TDENS, LF_TRANGE, LF_MUTE, LF_ROOT, LF_CHORD,
       LF_DOSE };
static const char *const LK_GRP[] = {"RHYTHM", "SCALE", "ENVELOPE", "LFO 1", "LFO 2", "LFO 3", "MATRIX",
                                     "LFO GLOBAL", "TEMPO", "DELAY", "REVERB", "MASTER", "FILTER A", "FILTER B",
                                     "MORPH", "CONSOLE", "RUMBLE", "MIXER"};
enum { GR_RYT, GR_GAM, GR_ENV, GR_L1, GR_L2, GR_L3, GR_MTX, GR_LGL, GR_TMP, GR_DLY, GR_REV, GR_MST, GR_FA, GR_FB, GR_MRP,
       GR_CONS, GR_RMB, GR_MIX };
static const char *const LK_OFFON[2] = {"OFF", "ON"};
static const char *const LK_SLOPE[2] = {"12 DB", "24 DB"};
typedef struct {
    uint8_t kind, ref, grp, scope;       /* LV_*, its field / slot / id, LK_GRP's, 1: a circle's */
    const char *name;
    int16_t lo, hi;                      /* (LV_MTX: the part's own description) */
    const char *const *names;            /* a list: its words, lo .. hi */
} lk_var_t;
#define LKV_C(k, r, g, n, lo, hi, nm) {k, r, g, 1, n, lo, hi, nm}
#define LKV_G(k, r, g, n, lo, hi, nm) {k, r, g, 0, n, lo, hi, nm}
#define LKV_LFO(n, g) LKV_C(LV_LFO, n * 4 + 0, g, "WAVE", 0, GW_N - 1, GL_WAVE_NAME), \
                      LKV_C(LV_LFO, n * 4 + 1, g, "RATE", 0, GL_NRATE - 1, GL_RATE_NAME), \
                      LKV_C(LV_LFO, n * 4 + 2, g, "DEPTH", -64, 63, 0), \
                      LKV_C(LV_LFO, n * 4 + 3, g, "DEST", 0, GD_N - 1, GL_DST_NAME)
#define LKV_MTX(n) LKV_C(LV_MTX, n * 3 + 0, GR_MTX, 0, 0, 0, 0), LKV_C(LV_MTX, n * 3 + 1, GR_MTX, 0, 0, 0, 0), \
                   LKV_C(LV_MTX, n * 3 + 2, GR_MTX, 0, 0, 0, 0)
#define LKV_AFL(sn, g) LKV_G(LV_AFL, sn * 16 + AFL_FREQ, g, "FREQ", 0, 127, 0), \
                       LKV_G(LV_AFL, sn * 16 + AFL_RES, g, "RES", 0, 127, 0), \
                       LKV_G(LV_AFL, sn * 16 + AFL_TYPE, g, "TYPE", 0, 127, 0), \
                       LKV_G(LV_AFL, sn * 16 + AFL_DRIVE, g, "DRIVE", 0, 127, 0), \
                       LKV_G(LV_AFL, sn * 16 + AFL_MOD, g, "MOD", 0, 127, 0), \
                       LKV_G(LV_AFL, sn * 16 + AFL_SRC, g, "SRC", 0, AFS_N - 1, AFS_NAME), \
                       LKV_G(LV_AFL, sn * 16 + AFL_FM, g, "FM", 0, 127, 0), \
                       LKV_G(LV_AFL, sn * 16 + AFL_MIX, g, "MIX", 0, 127, 0), \
                       LKV_G(LV_AFL, sn * 16 + AFL_SLOPE, g, "SLOPE", 0, 1, LK_SLOPE), \
                       LKV_G(LV_AFL, sn * 16 + AFL_DET, g, "DETUNE", 0, 127, 0), \
                       LKV_G(LV_AFL, sn * 16 + AFL_MTYP, g, "MODTYP", 0, 127, 0)
static const lk_var_t LK_V[] = {
    LKV_C(LV_CFG, LF_TYPE, GR_RYT, "TYPE", 0, AS_NTYPE - 1, AS_TYPE_NAME),
    LKV_C(LV_CFG, LF_LEN, GR_RYT, "LEN", 1, AS_PLAYLEN, 0),
    LKV_C(LV_CFG, LF_HITS, GR_RYT, "HITS", 0, AS_PLAYLEN, 0),
    LKV_C(LV_CFG, LF_ROT, GR_RYT, "ROT", 0, AS_PLAYLEN - 1, 0),
    LKV_C(LV_CFG, LF_PROB, GR_RYT, "PROB", 0, 127, 0),
    LKV_C(LV_CFG, LF_FROT, GR_RYT, "FORCE ROT", 0, AS_PLAYLEN - 1, 0),
    LKV_C(LV_CFG, LF_TLOCK, GR_RYT, "TURING LOCK", 0, 127, 0),
    LKV_C(LV_CFG, LF_TDENS, GR_RYT, "TURING DENS", 0, 127, 0),
    LKV_C(LV_CFG, LF_TRANGE, GR_RYT, "TURING RANGE", 0, 14, 0),
    LKV_C(LV_CFG, LF_MUTE, GR_RYT, "MUTE", 0, 1, LK_OFFON),
    LKV_C(LV_CFG, LF_ROOT, GR_GAM, "NOTE", 12, 96, 0),
    LKV_C(LV_CFG, LF_CHORD, GR_GAM, "CHORD", 0, AS_NCHORD - 1, AS_CHORD_NAME),
    LKV_C(LV_CFG, LF_DOSE, GR_GAM, "DICE DOSE", 0, 127, 0),
    LKV_C(LV_MODE, 0, GR_ENV, "TYPE", 0, AM_NSEL - 1, ALT_MODE_NAME),
    LKV_LFO(0, GR_L1), LKV_LFO(1, GR_L2), LKV_LFO(2, GR_L3),
    LKV_MTX(0), LKV_MTX(1), LKV_MTX(2), LKV_MTX(3),
    LKV_G(LV_SCALE, 0, GR_GAM, "SCALE", 0, AS_NSCALE - 1, AS_SCALE_NAME),
    LKV_G(LV_GL, 0, GR_LGL, "WAVE", 0, GW_N - 1, GL_WAVE_NAME),
    LKV_G(LV_GL, 1, GR_LGL, "RATE", 0, GL_NRATE - 1, GL_RATE_NAME),
    LKV_G(LV_GL, 2, GR_LGL, "DEPTH", -64, 63, 0),
    LKV_G(LV_GL, 3, GR_LGL, "DEST", 0, GD_N - 1, GL_DST_NAME),
    LKV_G(LV_G, G_BPM, GR_TMP, "TEMPO", 40, 240, 0),
    LKV_G(LV_G, G_SWING, GR_TMP, "SWING", 0, 100, 0),
    LKV_G(LV_G, G_DTIME, GR_DLY, "TIME", 0, (int16_t)(sizeof N_DTIME / sizeof N_DTIME[0]) - 1, N_DTIME),
    LKV_G(LV_G, G_DFDBK, GR_DLY, "FEEDBACK", 0, 120, 0),
    LKV_G(LV_G, G_DCOLOR, GR_DLY, "COLOR", 0, 127, 0),
    LKV_G(LV_G, G_RTYPE, GR_REV, "TYPE", 0, 3, N_RTYPE),
    LKV_G(LV_G, G_RSIZE, GR_REV, "SIZE", 0, 127, 0),
    LKV_G(LV_G, G_RDAMP, GR_REV, "DAMP", 0, 127, 0),
    LKV_G(LV_AMX, AMX_LOW, GR_MST, "LOW", 0, 127, 0), LKV_G(LV_AMX, AMX_MID, GR_MST, "MID", 0, 127, 0),
    LKV_G(LV_AMX, AMX_HIGH, GR_MST, "HIGH", 0, 127, 0), LKV_G(LV_AMX, AMX_COMP, GR_MST, "COMP", 0, 127, 0),
    LKV_G(LV_AMX, AMX_DUCK, GR_MST, "DUCK", 0, 127, 0), LKV_G(LV_AMX, AMX_SRC, GR_MST, "SRC", 0, AMS_N - 1, AMS_NAME),
    LKV_G(LV_AMX, AMX_SHAPE, GR_MST, "SHAPE", 0, 127, 0), LKV_G(LV_AMX, AMX_CLIP, GR_MST, "CLIP", 0, 127, 0),
    LKV_AFL(0, GR_FA), LKV_AFL(1, GR_FB),
    LKV_G(LV_MORPH, 0, GR_MRP, "MORPH A-B", 0, 127, 0),
    /* LINK 4 (07/11): each circle's CONSOLE, as the MIXER's tab I II III (the device's order); appended: the indexes
     * before stay */
    LKV_C(LV_PAR, P_DIST, GR_CONS, "DRIVE", 0, 127, 0),
    LKV_C(LV_CONS, ACS_LOW, GR_CONS, "LOW", 0, 127, 0), LKV_C(LV_CONS, ACS_MID, GR_CONS, "MID", 0, 127, 0),
    LKV_C(LV_CONS, ACS_HIGH, GR_CONS, "HIGH", 0, 127, 0), LKV_C(LV_CONS, ACS_COMP, GR_CONS, "COMP", 0, 127, 0),
    LKV_C(LV_PAR, P_DLY, GR_CONS, "DELAY", 0, 127, 0), LKV_C(LV_PAR, P_REV, GR_CONS, "REVERB", 0, 127, 0),
    LKV_C(LV_PAR, P_LEVEL, GR_CONS, "LEVEL", 0, 127, 0),
    /* 08/11: the RUMBLE (the KICK's knob 4; the page shows it for the sounds INFO marks) */
    LKV_C(LV_RMB, 0, GR_RMB, "RUMBLE", 0, 127, 0),
    /* 10/11: the MIXER's MASTER gain (its knob 4; the page's MIXER tab) */
    LKV_G(LV_VOL, 0, GR_MIX, "MASTER", 0, 127, 0),
};
#define LK_NV (sizeof LK_V / sizeof LK_V[0])
_Static_assert(LK_NV <= 2u * 48u, "LK_V: lk.vs holds 48 a scope");

static uint32_t lk_vn(uint32_t scope)            /* how many of a scope; their order: LK_V's */
{
    uint32_t k, n = 0;
    for (k = 0; k < LK_NV; k++)
        n += LK_V[k].scope == scope;
    return n;
}
static const lk_var_t *lk_vat(uint32_t scope, uint32_t i)   /* scope's i-th */
{
    uint32_t k;
    for (k = 0; k < LK_NV; k++)
        if (LK_V[k].scope == scope && i-- == 0u)
            return &LK_V[k];
    return 0;
}
static void lk_vlim(const lk_var_t *v, uint32_t c, int32_t *lo, int32_t *hi)
{
    if (v->kind == LV_MTX) {
        const param_desc_t *d = param_desc_of(eng_idx(trk[c].eng_req), P_M1SRC + v->ref);
        *lo = d ? d->min : 0;
        *hi = d ? d->max : 0;
        return;
    }
    *lo = v->lo;
    *hi = v->hi;
}
static int32_t lk_vget(const lk_var_t *v, uint32_t c)
{
    const as_cfg_t *f = &au.cfg[c % AS_NCIRC];
    switch (v->kind) {
    case LV_CFG:
        switch (v->ref) {
        case LF_TYPE: return f->type;
        case LF_LEN: return f->len;
        case LF_HITS: return f->hits;
        case LF_ROT: return f->rot;
        case LF_PROB: return f->prob;
        case LF_FROT: return f->frot;
        case LF_TLOCK: return f->tlock;
        case LF_TDENS: return f->tdens;
        case LF_TRANGE: return f->trange;
        case LF_MUTE: return f->mute;
        case LF_ROOT: return f->root;
        case LF_CHORD: return f->chord;
        default: return f->dose;
        }
    case LV_MODE: return alt_emode[c % AS_NCIRC];
    case LV_LFO: {
        const volatile alt_cl_t *l = &alt_cl[c % AS_NCIRC][(v->ref >> 2) % CL_N];
        uint32_t i = v->ref & 3u;
        return i == 0 ? l->wave : i == 1 ? l->rate : i == 2 ? l->depth : l->dst;
    }
    case LV_MTX: return alt_base(c % AS_NCIRC, P_M1SRC + v->ref);
    case LV_SCALE: return alt_seq.scale;
    case LV_GL: return v->ref == 0 ? alt_gl.wave : v->ref == 1 ? alt_gl.rate : v->ref == 2 ? alt_gl.amt[0] : alt_gl.dst[0];
    case LV_G: return song.g[v->ref];
    case LV_AMX: return amx[v->ref % AMX_N];
    case LV_AFL: return afl[(v->ref >> 4) & 1u][(v->ref & 15u) % AFL_N];
    case LV_CONS: return acs_v[c % AS_NCIRC][v->ref % ACS_N];
    case LV_PAR: return clamp(alt_base(c % AS_NCIRC, v->ref), 0, 127);
    case LV_RMB: return alt_rmb[c % AS_NCIRC];
    case LV_VOL: return alt_mgain;
    default: return afl_morph;
    }
}
/* set, in range (as the device's own knobs would: the rhythm from the circle's next cycle); the value kept back */
static int32_t lk_vset(const lk_var_t *v, uint32_t c, int32_t x)
{
    int32_t lo, hi;
    c %= AS_NCIRC;
    lk_vlim(v, c, &lo, &hi);
    x = clamp(x, lo, hi);
    switch (v->kind) {
    case LV_CFG: {
        as_cfg_t *f = &au.cfg[c];
        switch (v->ref) {
        case LF_TYPE: f->type = (uint8_t)x; break;
        case LF_LEN:
            f->len = (uint8_t)x;
            if (f->hits > f->len)
                f->hits = f->len;
            f->rot %= f->len;
            f->frot %= f->len;
            break;
        case LF_HITS: f->hits = (uint8_t)(x > f->len ? f->len : x); break;
        case LF_ROT: f->rot = (uint8_t)(x % (f->len ? f->len : 1u)); break;
        case LF_PROB: f->prob = (uint8_t)x; break;
        case LF_FROT: f->frot = (uint8_t)(x % (f->len ? f->len : 1u)); break;
        case LF_TLOCK: f->tlock = (uint8_t)x; break;
        case LF_TDENS: f->tdens = (uint8_t)x; break;
        case LF_TRANGE: f->trange = (uint8_t)x; break;
        case LF_MUTE: f->mute = (uint8_t)x; break;
        case LF_ROOT: f->root = (uint8_t)x; break;
        case LF_CHORD: f->chord = (uint8_t)x; break;
        default: f->dose = (uint8_t)x; break;
        }
        alt_post_circle(c);
        break;
    }
    case LV_MODE: alt_emode[c] = am_ok((uint32_t)x); break;
    case LV_LFO: {
        volatile alt_cl_t *l = &alt_cl[c][(v->ref >> 2) % CL_N];
        uint32_t i = v->ref & 3u;
        if (i == 0) l->wave = (uint8_t)x;
        else if (i == 1) l->rate = (uint8_t)x;
        else if (i == 2) l->depth = (int8_t)x;
        else l->dst = (uint8_t)x;
        break;
    }
    case LV_MTX: lk_set(c, P_M1SRC + v->ref, x); break;
    case LV_SCALE: alt_seq.scale = (uint8_t)x; break;
    case LV_GL:
        if (v->ref == 0) alt_gl.wave = (uint8_t)x;
        else if (v->ref == 1) alt_gl.rate = (uint8_t)x;
        else if (v->ref == 2) alt_gl.amt[0] = (int8_t)x;
        else alt_gl.dst[0] = (uint8_t)x;
        break;
    case LV_G:
        song.g[v->ref] = (int16_t)x;
        afx_edit |= (uint8_t)(v->grp == GR_DLY ? 1u << AFX_S_DLY : v->grp == GR_REV ? 1u << AFX_S_REV : 0u);
        break;
    case LV_AMX:
        amx[v->ref % AMX_N] = (uint8_t)x;
        afx_edit |= 1u << AFX_S_MST;
        break;
    case LV_AFL:
        afl[(v->ref >> 4) & 1u][(v->ref & 15u) % AFL_N] = (uint8_t)x;
        afl_edit |= (uint8_t)(1u << ((v->ref >> 4) & 1u));
        break;
    case LV_CONS: acs_v[c][v->ref % ACS_N] = (uint8_t)x; break;
    case LV_PAR: (void)lk_set(c, v->ref, x); break;
    case LV_RMB: alt_rmb[c] = (uint8_t)x; break;
    case LV_VOL:
        alt_mgain = (uint8_t)x;
        alt_sys[5] = (uint8_t)(x + 1);           /* (kept: SYSTEM's settings, as the MIXER's knob 4) */
        break;
    default: afl_morph = (uint8_t)x; break;
    }
    return lk_vget(v, c);
}

/* ---- the lanes: circle c's lane ln (VELO CHANCE DLY REV PITCH RATCH) at its 16 steps ----------------------- */
static int32_t lk_lget(uint32_t c, uint32_t ln, uint32_t k) { return alt_lane_val(c % AS_NCIRC, ln % LN_N, k % AS_NLANE); }
static int32_t lk_lset(uint32_t c, uint32_t ln, uint32_t k, int32_t v)
{
    as_circ_t *sc = &alt_seq.c[c % AS_NCIRC];
    int32_t lo, hi, st;
    ln %= LN_N;
    k %= AS_NLANE;
    alt_lane_lim(ln, &lo, &hi, &st);
    v = clamp(v, lo, hi);
    if (ln == LN_VELO)
        sc->svel[k] = (uint8_t)v;
    else if (ln == LN_CHANCE) {
        sc->chance[k] = (uint8_t)v;
        if (k + AS_NLANE < AS_MAXLEN)
            sc->chance[k + AS_NLANE] = (uint8_t)v;
    } else if (ln == LN_DLY)
        sc->sdly[k] = (uint8_t)v;
    else if (ln == LN_REV)
        sc->srev[k] = (uint8_t)v;
    else if (ln == LN_RATCH)
        sc->srat[k] = (uint8_t)v;
    else
        sc->spitch[k] = (int8_t)v;
    sc->rmask[ln % AS_NLR] &= (uint16_t)~(1u << k);   /* (a value set: no RANGE on that step any more) */
    return lk_lget(c, ln, k);
}
static void lk_lane(uint32_t cmd, uint32_t c, uint32_t ln)
{
    uint32_t k;
    lk_begin(cmd);
    lk_b(c);
    lk_b(ln);
    for (k = 0; k < AS_NLANE; k++) {
        int32_t v = lk_lget(c, ln, k);
        lk_v(v);
        lk.ls[c][ln][k] = (uint8_t)v;
    }
    lk_send();
}

/* ---- the actions: what the device's own keys do (alter_ui.c, the live keys), for circle c ----------------- */
enum { LA_MUTE, LA_DIE, LA_CLEAN, LA_DENSM, LA_DENSP, LA_SHL, LA_SHR, LA_TRIG, LA_RANDOM, LA_LANES, LA_FLATALL,
       LA_SHUF0, LA_FLAT0 = LA_SHUF0 + LN_N, LA_CHAOS = LA_FLAT0 + LN_N, LA_TOTAL, LA_SELECT,
       LA_PLAY, LA_STOP, LA_REC, LA_N };   /* (04/11: the transport, as PLAY/STOP and REC tapped) */
static int lk_act(uint32_t c, uint32_t a)
{
    uint8_t sel = au.sel, lane = au.lane;
    uint32_t ln;
    int tur;
    if (c >= AS_NCIRC || a >= LA_N)
        return 0;
    tur = au.cfg[c].type == AS_T_TURING;
    switch (a) {
    case LA_MUTE:
        au.cfg[c].mute ^= 1u;
        alt_post_circle(c);
        return 1;
    case LA_DIE:                                     /* (as a DICE key tapped: a new draw, at once, no chord) */
        alt_roll_cnt[c]++;
        if (au.cfg[c].chord != AS_CH_OFF) {
            au.cfg[c].chord = AS_CH_OFF;
            alt_post_circle(c);
        }
        alt_cal(c);
        alt_say("NEW DRAW");
        return 1;
    case LA_CLEAN:
        alt_clear_cnt[c]++;
        alt_cal(c);
        alt_say("CLEAN");
        return 1;
    case LA_DENSM: case LA_DENSP: {
        int32_t d = a == LA_DENSP ? 1 : -1;
        alt_dens_tot[c] += d;
        au.cfg[c].hits = (uint8_t)clamp((int32_t)au.cfg[c].hits + d, 0, au.cfg[c].len);
        return 1;
    }
    case LA_SHL: case LA_SHR: {
        int32_t d = a == LA_SHR ? 1 : -1;
        alt_shift_tot[c] += d;
        if (!tur)
            au.cfg[c].rot = as_wrap((int32_t)au.cfg[c].rot + d, au.cfg[c].len);
        return 1;
    }
    case LA_TRIG: alt_trig_tot[c]++; return 1;
    case LA_CHAOS: alt_chaos(); alt_say("CHAOS"); return 1;
    case LA_TOTAL: alt_total_chaos(); return 1;      /* (again within 3 s: undone, as on the device) */
    case LA_SELECT:
        if (au.sel != c)
            alt_select(c);
        return 1;
    case LA_PLAY: if (!song.playing) transport_req = 1u; return 1;
    case LA_STOP: if (song.playing || seq_counting()) transport_req = 2u; return 1;
    case LA_REC: ar_on ^= 1u; alt_say(ar_on ? "REC ON" : "REC OFF"); return 1;   /* (REC tapped: overdub on / off) */
    default: break;
    }
    au.sel = (uint8_t)c;                             /* (these act on the chosen circle: c for a moment) */
    if (a == LA_RANDOM)                              /* (as MUTE 1+2+3: LEN, HITS and VELO CHANCE PITCH drawn) */
        alt_lanes_random();
    else if (a == LA_LANES)
        for (ln = 0; ln < LN_N; ln++) {
            au.lane = (uint8_t)ln;
            alt_lane_shuffle();
        }
    else if (a == LA_FLATALL)
        for (ln = 0; ln < LN_N; ln++)
            alt_lane_flat(ln);
    else if (a < LA_FLAT0) {
        au.lane = (uint8_t)(a - LA_SHUF0);
        alt_lane_shuffle();
    } else
        alt_lane_flat(a - LA_FLAT0);
    au.sel = sel;
    au.lane = lane;
    return 1;
}

/* ---- PLAY: the circles as they play (playing, the chosen one; each: step, LEN, mute, pattern, notes played) --- */
static uint32_t lk_play_sig(void)
{
    uint32_t c, h = (uint32_t)song.playing * 7u + au.sel + (ar_on ? 131u : 0u);
    for (c = 0; c < AS_NCIRC; c++) {
        const as_circ_t *sc = &alt_seq.c[c];
        h = h * 31u + sc->pos;
        h = h * 31u + sc->pat;
        h = h * 31u + sc->cfg.len + 64u * au.cfg[c].mute + 128u * sc->die;
        h = h * 31u + alt_hits[c];
    }
    return h;
}
static void lk_play(void)
{
    uint32_t c;
    lk_begin(LKC_PLAY);
    lk_b((song.playing ? 1u : 0u) | (ar_on ? 2u : 0u));   /* (bit 1: REC lit) */
    lk_b(au.sel);
    for (c = 0; c < AS_NCIRC; c++) {
        const as_circ_t *sc = &alt_seq.c[c];
        uint32_t len = sc->cfg.len ? sc->cfg.len : 1u, pat = sc->pat;
        lk_b((sc->pos + len - 1u) % len);            /* (the step playing now: as the screen draws it) */
        lk_b(len);
        lk_b(au.cfg[c].mute | (uint32_t)sc->die << 1);
        lk_b(pat);
        lk_b(pat >> 7);
        lk_b(pat >> 14);
        lk_b(alt_hits[c]);                           /* (7 bits: the page flashes when it moves) */
    }
    lk_send();
    lk.play_sig = lk_play_sig();
}

/* the page in sync with every setting and lane (WATCH on, VLIST) */
static void lk_vshadow(void)
{
    uint32_t k, c, i[2] = {0, 0}, ln;
    for (k = 0; k < LK_NV; k++) {
        const lk_var_t *v = &LK_V[k];
        if (v->scope)
            for (c = 0; c < AS_NCIRC; c++)
                lk.vs[c][i[1]] = (int16_t)lk_vget(v, c);
        else
            lk.vs[AS_NCIRC][i[0]] = (int16_t)lk_vget(v, 0);
        i[v->scope]++;
    }
    for (c = 0; c < AS_NCIRC; c++)
        for (ln = 0; ln < LN_N; ln++)
            for (k = 0; k < AS_NLANE; k++)
                lk.ls[c][ln][k] = (uint8_t)lk_lget(c, ln, k);
    lk.play_sig = 0;
}

/* SCENE / PAT (t 0 scenes, 1 patterns): op 0 the USER slots used; 1 slot [c0 c1 c2]: store here (as SAVE held; with
 * a NAME: that one, else the one it had); 2 slot: load; 3: the slots used and their NAMEs (v3). Reply op, slot, rc (1 ok,
 * 0 empty / protected, 2 the flash failed), then for op 0 one used byte per slot, for op 3 used + 3 characters (0: no
 * NAME) per slot */
static void lk_user(uint32_t t, uint32_t cmd, const uint8_t *a, uint32_t na)
{
    int rc = 0;
    uint32_t k, c;
    if (na < 1u || a[0] > 3u || ((a[0] == 1u || a[0] == 2u) && (na < 2u || a[1] >= ALT_NUSER)) ||
        (a[0] == 1u && na != 2u && na != 5u) || (a[0] == 2u && na != 2u))
        return;
    if (a[0] == 1u) {
        if (na == 5u) {                              /* its NAME (the page's), in ALT_NM_CH's characters */
            for (k = 0; k < 3u; k++)
                alt_nm_new[k] = ALT_NM_CH[alt_nm_idx((char)a[2u + k])];
            if (alt_nm_new[0] == ' ' && alt_nm_new[1] == ' ' && alt_nm_new[2] == ' ')
                alt_nm_new[0] = 0;
            alt_nm_set = 1;
        }
        rc = alt_row_save(t, 1u + ALT_NFACT + a[1]);
        alt_nm_set = 0;
    } else if (a[0] == 2u)
        rc = alt_row_load(t, 1u + ALT_NFACT + a[1]);
    lk_begin(cmd);
    lk_b(a[0]);
    lk_b(na > 1u ? a[1] : 0u);
    lk_b(rc < 0 ? 2u : (uint32_t)rc);
    if (a[0] == 0u || a[0] == 3u)
        for (k = 0; k < ALT_NUSER; k++) {
            uint32_t u = alt_used_s[k];
            const char *nm = alt_nm_s[k];
            lk_b(u ? 1u : 0u);
            if (a[0] == 3u)
                for (c = 0; c < 3u; c++)
                    lk_b(u && nm[0] ? (uint32_t)(uint8_t)nm[c] : 0u);
        }
    lk_send();
    if (a[0] == 2u && rc > 0)
        for (c = 0; c < AS_NCIRC; c++)
            lk_shadow(c);                            /* (the page re-reads STATE after a load) */
}

/* BLOB (10/11, Romain: the page's patch file = the whole scene): alt_blob (alter_store.c), 8-bit bytes sent 7 at a time in 8
 * (a byte of their top bits, then their 7 low bits each). A value in a request: its 14 bits as they are (lo, hi); in a
 * reply: as lk_v (+ 8192, as everywhere).
 *   0             what plays now into alt_blob (as SAVE held would store it)  -> 0, size (value), sum (value: of the bytes,
 *                 13 bits)
 *   1 off         its bytes from off (value), up to LK_BLOB_OUT             -> 1, off, n (value), n bytes packed
 *   2 off, data   bytes packed from the page into alt_blob at off           -> 2, off, n written (value)
 *   3 size, sum   alt_blob played (as a USER scene loads), if size and sum match and it is ours  -> 3, rc (1 played, 0 refused) */
#define LK_BLOB_OUT 256u
_Static_assert(sizeof(alt_blob_t) < 8192u, "BLOB: its size and offsets in a value");
static uint32_t lk_blob_sum(void)
{
    uint32_t k, sm = 0;
    for (k = 0; k < sizeof alt_blob; k++)
        sm = (sm * 31u + ((const uint8_t *)&alt_blob)[k]) & 0x1FFFu;
    return sm;
}
static void lk_blob(const uint8_t *a, uint32_t na)
{
    uint8_t *b = (uint8_t *)&alt_blob;
    uint32_t off, n, k, j;
    if (na < 1u || a[0] > 3u)
        return;
    if (a[0] == 0u) {
        for (k = 0; k < sizeof alt_blob; k++)
            b[k] = 0;
        alt_us_snap(&alt_blob.r, alt_blob.cs, alt_blob.dr);
        lk_begin(LKC_BLOB);
        lk_b(0);
        lk_v((int32_t)sizeof alt_blob);
        lk_v((int32_t)lk_blob_sum());
        lk_send();
        return;
    }
    if (na < 3u)
        return;
    off = (uint32_t)(lk_rv(a + 1) + 8192);
    if (a[0] == 1u) {
        if (off > sizeof alt_blob)
            return;
        n = sizeof alt_blob - off;
        n = n > LK_BLOB_OUT ? LK_BLOB_OUT : n;
        lk_begin(LKC_BLOB);
        lk_b(1);
        lk_v((int32_t)off);
        lk_v((int32_t)n);
        for (k = 0; k < n; k += 7u) {
            uint32_t top = 0;
            for (j = 0; j < 7u && k + j < n; j++)
                top |= (uint32_t)(b[off + k + j] >> 7) << j;
            lk_b(top);
            for (j = 0; j < 7u && k + j < n; j++)
                lk_b(b[off + k + j]);
        }
        lk_send();
        return;
    }
    if (a[0] == 2u) {
        uint32_t i = 3u;
        n = 0;
        while (i < na) {                             /* (8 bytes: 7 of ours; the last group may be shorter) */
            uint32_t top = a[i++];
            for (j = 0; j < 7u && i < na; j++, i++)
                if (off + n < sizeof alt_blob)
                    b[off + n++] = (uint8_t)((a[i] & 0x7Fu) | (((top >> j) & 1u) << 7));
        }
        lk_begin(LKC_BLOB);
        lk_b(2);
        lk_v((int32_t)off);
        lk_v((int32_t)n);
        lk_send();
        return;
    }
    {
        uint32_t sz = off, sm = na >= 5u ? (uint32_t)(lk_rv(a + 3) + 8192) : 0xFFFFFFFFu, ok;   /* (no sum: never matches) */
        ok = sz == sizeof alt_blob && sm == lk_blob_sum() && alt_blob.r.used == 1u;
        if (ok) {
            sg_detach = 1;
            alt_us_apply(&alt_blob.r, alt_blob.cs, alt_blob.dr);
        }
        lk_begin(LKC_BLOB);
        lk_b(3);
        lk_b(ok);
        lk_send();
        if (ok)
            for (k = 0; k < AS_NCIRC; k++)
                lk_shadow(k);                        /* (the page re-reads STATE after a load) */
    }
}

/* SONG (v3, alter_ui.c alt_sg_*): op 0 the state; 1 slot: store what plays there (as SAVE held + A B C); 2 slot: it plays
 * from the next bar (as its key); 3 n, n links (slot | bars' code << 2): the chain (stored); 4 on: the chain on / off
 * (as ARP). Reply op, rc (1 done, 0 refused, 2 the flash failed), then the state: the full slots (bits), the slot
 * playing + 1, the one asked for + 1, the chain on, its link playing, that link's bars left, n, the n links */
static void lk_song(const uint8_t *a, uint32_t na)
{
    uint32_t rc = 1, k;
    if (na < 1u || a[0] > 4u || ((a[0] == 1u || a[0] == 2u) && (na != 2u || a[1] >= SG_NSLOT)) ||
        (a[0] == 3u && (na < 2u || a[1] > SG_MAXL || na != 2u + a[1])) || (a[0] == 4u && na != 2u))
        return;
    if (a[0] == 1u)
        rc = alt_sg_store(a[1]) ? 1u : 2u;
    else if (a[0] == 2u) {
        if (sg_slot[a[1]].used)
            alt_sg_ask(a[1]);
        else
            rc = 0;
    } else if (a[0] == 3u) {
        for (k = 0; k < SG_MAXL; k++)
            asg.chain[k] = k < a[1] ? (uint8_t)((a[2u + k] & 3u) % SG_NSLOT | (a[2u + k] & 12u)) : 0u;
        asg.n = a[1];
        if (asg.row >= asg.n)
            asg.row = 0;
        alt_sg_push();
        alt_sg_chain_save();
    } else if (a[0] == 4u) {
        if (a[1] && !asg.n)
            rc = 0;
        else
            alt_sg_on(a[1] != 0);
    }
    lk_begin(LKC_SONG);
    lk_b(a[0]);
    lk_b(rc);
    lk_b((uint32_t)sg_slot[0].used | (uint32_t)sg_slot[1].used << 1 | (uint32_t)sg_slot[2].used << 2);
    lk_b(sg_play);
    lk_b(asg.q);
    lk_b(sg_on);
    lk_b(sg_link);
    lk_b(sg_left);
    lk_b(asg.n);
    for (k = 0; k < asg.n; k++)
        lk_b(asg.chain[k]);
    lk_send();
}

enum { LKC_DIAG = 30, LKC_TEST, LKC_PEEK };
#define LK_BLK100 (CTL * 544u / 100u)            /* 1 % of a block in TIMER4 ticks */
static uint32_t lk_r32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 7 | (uint32_t)p[2] << 14 | (uint32_t)p[3] << 21 | (uint32_t)p[4] << 28;
}
static void lk_handle(const uint8_t *f, uint32_t n)   /* f: the bytes between F0 and F7 */
{
    const uint8_t *a = f + 4;
    uint32_t na = n - 4u, c, k, cmd = f[3];
    switch (cmd) {
    case LKC_INFO:                                    /* version, circles, engines (names), sounds (engine, name),
                                                      * USER scene slots, the parameter ids of STATE */
        lk_begin(LKC_INFO);
        lk_str(LK_VERSION);
        lk_b(AS_NCIRC);
        lk_b(ALT_NENG);
        for (k = 0; k < ALT_NENG; k++)
            lk_str(ENGINES[eng_idx(ALT_ENGS[k])]->name);
        lk_b(ALT_NPRE);
        for (k = 0; k < ALT_NPRE; k++) {
            lk_b(ALT_PRE[k].eng);
            lk_str(ALT_PRE[k].name);
        }
        lk_b(ALT_NUSER);
        lk_b(LK_NID);
        for (k = 0; k < LK_NID; k++)
            lk_b(LK_IDS[k]);
        for (k = 0; k < ALT_NPRE; k++)               /* (08/11, appended: each sound has a RUMBLE, 1, or not) */
            lk_b(ALT_PRE[k].rumble != 0);
        lk_send();
        return;
    case LKC_STATE:                                   /* circle */
        if (na != 1u || a[0] >= AS_NCIRC)
            return;
        lk_state(a[0]);
        return;
    case LKC_DESC: {                                  /* circle, id [, engine (ALT_ENGS index): that engine's, whatever
                                                      * the circle plays]: fmt, min, max, def, label, unit, enum names */
        const param_desc_t *d;
        if ((na != 2u && na != 3u) || a[0] >= AS_NCIRC || !lk_known(a[1]) || (na == 3u && a[2] >= ALT_NENG))
            return;
        d = na == 3u ? param_desc_of(eng_idx(ALT_ENGS[a[2]]), a[1]) : lk_desc(a[0], a[1]);
        if (!d)
            return;
        lk_begin(LKC_DESC);
        lk_b(a[0]);
        lk_b(a[1]);
        lk_b(d->fmt);
        lk_v(d->min);
        lk_v(d->max);
        lk_v(d->def);
        lk_str(d->label);
        lk_str(d->unit);
        if (d->fmt == F_ENUM && d->names)
            for (k = 0; (int32_t)k <= d->max - d->min && k < 32u; k++)
                lk_str(d->names[k]);
        lk_send();
        return;
    }
    case LKC_SET: {                                   /* circle, id, v14: the value kept */
        int32_t v;
        if (na != 4u || a[0] >= AS_NCIRC || !lk_known(a[1]))
            return;
        c = a[0];
        v = lk_set(c, a[1], lk_rv(a + 2));
        for (k = 0; k < LK_NID; k++)                 /* (the page's own write: no push back) */
            if (LK_IDS[k] == a[1])
                lk.sh[c][k] = (int16_t)alt_base(c, a[1]);
        lk_begin(LKC_SET);
        lk_b(c);
        lk_b(a[1]);
        lk_v(v);
        lk_send();
        return;
    }
    case LKC_SOUND:                                   /* circle, sound (ALT_PRE): loaded as PRESETS does; STATE back */
        if (na != 2u || a[0] >= AS_NCIRC || a[1] >= ALT_NPRE)
            return;
        alt_load_sound(a[0], a[1]);
        lk_state(a[0]);
        return;
    case LKC_SCENE:                                   /* the USER scenes (lk_user) */
        lk_user(0u, LKC_SCENE, a, na);
        return;
    case LKC_BLOB:                                    /* a whole scene, to and from a file (10/11): lk_blob */
        lk_blob(a, na);
        return;
    case LKC_SONG:                                    /* SONG (v3, 05/11): lk_song */
        lk_song(a, na);
        return;
    case LKC_WATCH:                                   /* on 0/1 */
        if (na != 1u)
            return;
        lk.on = lk.was = (uint8_t)(a[0] & 1u);
        if (lk.v2)
            lk_vshadow();
        for (c = 0; c < AS_NCIRC; c++)
            lk_shadow(c);
        lk_begin(LKC_WATCH);
        lk_b(lk.on);
        lk_send();
        return;
    case LKC_VLIST:                                  /* the groups' names, how many settings a circle has, shared ones,
                                                      * the lanes' names */
        lk.v2 = 1;
        lk_vshadow();
        lk_begin(LKC_VLIST);
        lk_b(sizeof LK_GRP / sizeof LK_GRP[0]);
        for (k = 0; k < sizeof LK_GRP / sizeof LK_GRP[0]; k++)
            lk_str(LK_GRP[k]);
        lk_b(lk_vn(1));
        lk_b(lk_vn(0));
        lk_b(LN_N);
        for (k = 0; k < LN_N; k++)
            lk_str(ALT_LANE_NAME[k]);
        lk_b(LA_N);
        lk_send();
        return;
    case LKC_VDESC: {                                /* scope, i [, circle]: group, min, max, name, words */
        const lk_var_t *v;
        int32_t lo, hi;
        uint32_t cc = na > 2u ? a[2] % AS_NCIRC : 0u;
        if (na < 2u || !(v = lk_vat(a[0] & 1u, a[1])))
            return;
        lk_vlim(v, cc, &lo, &hi);
        lk_begin(LKC_VDESC);
        lk_b(a[0] & 1u);
        lk_b(a[1]);
        lk_b(v->grp);
        lk_v(lo);
        lk_v(hi);
        if (v->kind == LV_MTX) {                     /* (the matrix: "SLOT n" and the part's own words) */
            const param_desc_t *d = param_desc_of(eng_idx(trk[cc].eng_req), P_M1SRC + v->ref);
            static const char *const W[3] = {"SRC", "DEST", "AMT"};
            char nm[12] = "SLOT 1 ";
            uint32_t j = 7;
            const char *w = W[v->ref % 3u];
            nm[5] = (char)('1' + v->ref / 3u);
            while (*w && j < 11u)
                nm[j++] = *w++;
            nm[j] = 0;
            lk_str(nm);
            if (d && d->fmt == F_ENUM && d->names)
                for (k = 0; (int32_t)k <= hi - lo && k < 40u; k++)
                    lk_str(d->names[k]);
        } else {
            lk_str(v->name);
            if (v->names)
                for (k = 0; (int32_t)k <= hi - lo && k < 40u; k++)
                    lk_str(v->names[k]);
        }
        lk_send();
        return;
    }
    case LKC_VALL: {                                 /* circle (3: the shared ones): every value, in LK_V's order */
        uint32_t sc, cc, i, n;
        if (na != 1u || a[0] > AS_NCIRC)
            return;
        sc = a[0] < AS_NCIRC ? 1u : 0u;
        cc = a[0] % AS_NCIRC;
        n = lk_vn(sc);
        lk_begin(LKC_VALL);
        lk_b(a[0]);
        lk_b(n);
        for (i = 0; i < n; i++) {
            int32_t x = lk_vget(lk_vat(sc, i), cc);
            lk_v(x);
            lk.vs[sc ? cc : AS_NCIRC][i] = (int16_t)x;
        }
        lk_send();
        return;
    }
    case LKC_VSET: {                                 /* circle (3: shared), i, v14: the value kept */
        uint32_t sc, cc;
        const lk_var_t *v;
        int32_t x;
        if (na != 4u || a[0] > AS_NCIRC)
            return;
        sc = a[0] < AS_NCIRC ? 1u : 0u;
        cc = a[0] % AS_NCIRC;
        if (!(v = lk_vat(sc, a[1])))
            return;
        x = lk_vset(v, cc, lk_rv(a + 2));
        lk.vs[sc ? cc : AS_NCIRC][a[1]] = (int16_t)x;
        lk_begin(LKC_VSET);
        lk_b(a[0]);
        lk_b(a[1]);
        lk_v(x);
        lk_send();
        return;
    }
    case LKC_LANE:                                   /* circle, lane: its 16 values */
        if (na != 2u || a[0] >= AS_NCIRC || a[1] >= LN_N)
            return;
        lk_lane(LKC_LANE, a[0], a[1]);
        return;
    case LKC_LSET: {                                 /* circle, lane, step, v14: the value kept */
        int32_t x;
        if (na != 5u || a[0] >= AS_NCIRC || a[1] >= LN_N || a[2] >= AS_NLANE)
            return;
        x = lk_lset(a[0], a[1], a[2], lk_rv(a + 3));
        lk.ls[a[0]][a[1]][a[2]] = (uint8_t)x;
        lk_begin(LKC_LSET);
        lk_b(a[0]);
        lk_b(a[1]);
        lk_b(a[2]);
        lk_v(x);
        lk_send();
        return;
    }
    case LKC_ACT:                                    /* circle, action (LA_*): 1 done */
        if (na != 2u)
            return;
        k = (uint32_t)lk_act(a[0], a[1]);
        lk_begin(LKC_ACT);
        lk_b(a[0]);
        lk_b(a[1]);
        lk_b(k);
        lk_send();
        return;
    case LKC_PLAY:
        lk_play();
        return;
    case LKC_PING:                                   /* (after a silence of > 3 s, watching again: what changed
                                                      * meanwhile is pushed, the page's copy being older) */
        if (lk.was)
            lk.on = 1;
        lk_begin(LKC_PING);
        lk_b(0);
        lk_send();
        return;
    case LKC_DIAG:                                   /* (v0.55, the test page web/alter/test.html: Romain's FM-1
                                                      * measured from his browser) the CPU lines' numbers */
        lk_begin(LKC_DIAG);
        lk_str(FELUCCA_VERSION);
#if ALTER_CPU_SHOW
        lk_v((int32_t)alt_cpu_avg), lk_v((int32_t)alt_cpu_pk), lk_v((int32_t)alt_fps), lk_v((int32_t)alt_draw_ms);
        lk_v((int32_t)(shed_count > 8000u ? 8000u : shed_count));
        lk_v((int32_t)(alt_cpu_late > 8000u ? 8000u : alt_cpu_late));
        lk_b(alt_cpu_c1);
#else
        for (k = 0; k < 6u; k++)
            lk_v(0);
        lk_b(0);
#endif
        for (k = 0; k < APF_N; k++)
            lk_b(apf_pct[k]);
        lk_b(alt_t5_pct > 99u ? 99u : alt_t5_pct);
        lk_b(split_last);
        for (k = 0; k < NPART; k++)                  /* each part's and each core's fixed work, % of a block */
            lk_b(part_tk[k] / LK_BLK100 > 127u ? 127u : part_tk[k] / LK_BLK100);
        lk_b(fix_tk[0] / LK_BLK100 > 127u ? 127u : fix_tk[0] / LK_BLK100);
        lk_b(fix_tk[1] / LK_BLK100 > 127u ? 127u : fix_tk[1] / LK_BLK100);
        for (k = 0; k < AS_NCIRC; k++)
            lk_b(trk[k].engine);
        lk_b(alt_gov);                               /* (v0.62: the safety net's level, times up, notes ended sooner) */
        lk_v((int32_t)(alt_gov_ups > 8000u ? 8000u : alt_gov_ups));
        lk_v((int32_t)(alt_gov_tails > 8000u ? 8000u : alt_gov_tails));
        lk_v((int32_t)(alt_dr_w10 > 8000u ? 8000u : alt_dr_w10));   /* (v0.63: a frame's drawing, 0.1 ms: all, audio in it, */
        lk_v((int32_t)(alt_dr_i10 > 8000u ? 8000u : alt_dr_i10));   /* the screen's transfers; pixels / 100; loop turns /s) */
        lk_v((int32_t)(alt_dr_l10 > 8000u ? 8000u : alt_dr_l10));
        lk_v((int32_t)(alt_dr_kpx > 8000u ? 8000u : alt_dr_kpx));
        lk_v((int32_t)(alt_loops > 8000u ? 8000u : alt_loops));
        lk_send();
        return;
    case LKC_TEST:                                   /* op, a, b (5 x 7 bits each): alter_ui.c alt_web_cmd, the tests'
                                                      * own ops, on the real machine */
        if (na < 11u)
            return;
        alt_web_cmd(a[0], lk_r32(a + 1), lk_r32(a + 6));
        lk_begin(LKC_TEST);
        lk_b(a[0]);
        lk_send();
        return;
    case LKC_PEEK:                                   /* c, w (5 x 7 bits each): alt_peek's value, 5 x 7 bits */
        if (na < 10u)
            return;
        {
            uint32_t v = (uint32_t)alt_peek(lk_r32(a), lk_r32(a + 5));
            lk_begin(LKC_PEEK);
            for (k = 0; k < 5u; k++)
                lk_b(v >> (7u * k));
            lk_send();
        }
        return;
    default:
        return;                                      /* (unknown: no reply) */
    }
}

/* main loop: while watched, what changed on the device. A sound or a scene loaded (or many values at once):
 * RELOAD circle (the page re-reads STATE); else CHANGED circle, id, v14, a few per pass */
static void lk_sync(void)
{
    uint32_t c, k, sent = 0;
    if (!lk.on)
        return;
    if (fm1_ms - lk.last_ms > 3000u) {               /* the page went away */
        lk.on = 0;
        return;
    }
    if (fm1_ms - lk.pass_ms < 20u)
        return;
    lk.pass_ms = fm1_ms;
    for (c = 0; c < AS_NCIRC && sent < 6u; c++) {
        uint32_t diff = 0;
        if (!lk_room())
            return;
        for (k = 0; k < LK_NID; k++)
            diff += lk.sh[c][k] != alt_base(c, LK_IDS[k]);
        if (lk.eng[c] != trk[c].eng_req || lk.pre[c] != au.pre[c] || lk.pcnt[c] != alt_preset_cnt[c] || diff > 4u) {
            lk_begin(LKC_RELOAD);
            lk_b(c);
            lk_send();
            lk_shadow(c);
            sent++;
            continue;
        }
        for (k = 0; k < LK_NID && sent < 6u; k++) {
            int32_t v = alt_base(c, LK_IDS[k]);
            if (v == lk.sh[c][k])
                continue;
            lk_begin(LKC_CHANGED);
            lk_b(c);
            lk_b(LK_IDS[k]);
            lk_v(v);
            lk_send();
            lk.sh[c][k] = (int16_t)v;
            sent++;
        }
    }
    if (!lk.v2)
        return;
    /* v2: the circles as they move (~30 a second), then each setting that moved (VCHANGED circle (3: shared), i,
     * v14), then a lane that moved, whole (LCHANGED) */
    if (fm1_ms - lk.play_ms >= 33u && lk_room() && lk_play_sig() != lk.play_sig) {
        lk.play_ms = fm1_ms;
        lk_play();
        sent++;
    }
    {
        uint32_t i[2] = {0, 0};
        for (k = 0; k < LK_NV && sent < 10u; k++) {
            const lk_var_t *v = &LK_V[k];
            uint32_t sc = v->scope, c0 = sc ? 0u : AS_NCIRC, c1 = sc ? AS_NCIRC : AS_NCIRC + 1u, j = i[sc]++;
            for (c = c0; c < c1 && sent < 10u; c++) {
                int32_t x = lk_vget(v, c % AS_NCIRC);
                if (x == lk.vs[c][j])
                    continue;
                if (!lk_room())
                    return;
                lk_begin(LKC_VCHANGED);
                lk_b(c);
                lk_b(j);
                lk_v(x);
                lk_send();
                lk.vs[c][j] = (int16_t)x;
                sent++;
            }
        }
    }
    for (c = 0; c < AS_NCIRC && sent < 10u; c++) {
        uint32_t ln;
        for (ln = 0; ln < LN_N && sent < 10u; ln++) {
            for (k = 0; k < AS_NLANE; k++)
                if ((uint8_t)lk_lget(c, ln, k) != lk.ls[c][ln][k])
                    break;
            if (k == AS_NLANE)
                continue;
            if (!lk_room())
                return;
            lk_lane(LKC_LCHANGED, c, ln);
            sent++;
        }
    }
}

static void lk_frame(const uint8_t *p, uint32_t n)   /* a SysEx frame's bytes between F0 and F7 */
{
    uint8_t f[64];
    uint32_t i;
    if (n < 4u || n > sizeof f || p[0] != LK_H0 || p[1] != LK_H1 || p[2] != LK_H2)
        return;
    for (i = 0; i < n; i++)
        f[i] = p[i];
    lk.last_ms = fm1_ms;                             /* any request keeps WATCH alive */
    lk_handle(f, n);
}

#ifndef FELUCCA_WEB
/* main loop, the one place that reads a received SysEx frame: ALTER LINK's are answered here, any other goes to
 * ota_service() (M-UPGRADE: untouched). (Before 04/11 ota_service ran after lk_service and could take an ALTER LINK
 * frame that arrived in between: it threw it away, the page got no reply and saw the link as cut.) The frame stays
 * put between the look and the take: the USB side keeps no new one while one waits (usb.c sx_ready) */
static void lk_usb_service(int ota)                /* ota 0: leave another's frame where it is */
{
    const uint8_t *p;
    uint32_t n;
    lk_sync();
    if (!ota_frame_get(&p, &n))
        return;
    if (n >= 4u && p[0] == LK_H0 && p[1] == LK_H1 && p[2] == LK_H2) {
        uint8_t f[64];
        uint32_t k = n > sizeof f ? sizeof f : n, i;
        for (i = 0; i < k; i++)
            f[i] = p[i];
        ota_frame_done();                            /* free the frame before replying */
        lk_frame(f, k);
        return;
    }
    if (ota)
        ota_service();                               /* (not ours: the update's handshake) */
}
#endif
