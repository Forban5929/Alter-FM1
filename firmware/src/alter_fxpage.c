/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's FX page (included by alter_ui.c after alter_store.c): FX held 0.5 s opens it, it stays open (FX tapped,
 * HOME or another page button leaves; a tap on FX is the quick page as before). The keys play on (mutes, dice,
 * fills: DUB DELAY into the delay shown). Four tabs (ALGORITHM):
 *   DIST    the chosen circle's (SELECT the circle): PRESETS its preset, KNOB DRIVE TYPE AMOUNT MIX
 *   DELAY   shared: PRESETS the preset, KNOB TIME FEEDBK COLOR, and SEND: the chosen circle's (SELECT)
 *   REVERB  shared: PRESETS the preset, KNOB TYPE SIZE DAMP, and SEND: the chosen circle's (SELECT)
 *   MASTER  PRESETS the preset; two rows of four (SELECT the row): LOW MID HIGH COMP | DUCK SRC SHAPE CLIP; the
 *           compressor's and the duck's meters below
 * The presets and the master: alter_fx.c. A knob moved after a preset: its name with a * */

/* (its state, afp: alter_fx.c, so alter_ui_input sees it) */
enum { AFP_DIST, AFP_DLY, AFP_REV, AFP_MST, AFP_N };
static const char *const AFP_TAB[AFP_N] = {"DIST", "DELAY", "REVERB", "MASTER"};
static const char *const AFP_RTYPE[3] = {"ROOM", "SPRING", "HALL"};
/* the delay's times, short to long (params.c N_DTIME's ids) */
static const uint8_t AFP_DT[] = {3, 5, 2, 11, 4, 1, 10, 0, 12, 6, 7};
#define AFP_NDT sizeof AFP_DT

static uint32_t afp_dt_pos(void)
{
    uint32_t i;
    for (i = 0; i < AFP_NDT; i++)
        if (AFP_DT[i] == (uint32_t)song.g[G_DTIME])
            return i;
    return 5;
}

/* ---- the presets on ---------------------------------------------------------------------------------------- */
static void afp_dist_apply(uint32_t c, uint32_t i)
{
    const afx_dist_t *d = &AFX_DIST[i % AFX_NDIST];
    track_t *t = &trk[c % AS_NCIRC];
    afx_sel[c] = (uint8_t)(i % AFX_NDIST);
    afx_edit &= (uint8_t)~(1u << c);
    fm1_irq_off();
    if (!afx_sel[c]) {                           /* SOUND: its own DIST, no insert */
        t->p[P_DIST] = ALT_PRE[au.pre[c % AS_NCIRC]].fx[0];
        t->p[P_ITYPE] = 0;
        t->p[P_IMIX] = 0;
    } else {
        t->p[P_DIST] = d->dist;
        t->p[P_ITYPE] = d->type;
        t->p[P_IA] = d->a;
        t->p[P_IB] = d->b;
        t->p[P_IC] = d->c;
        t->p[P_IMIX] = d->mix;
    }
    fm1_irq_on();
}
static void afp_dly_apply(uint32_t i)
{
    const afx_dly_t *d = &AFX_DLY[i % AFX_NDLY];
    afx_sel[AFX_S_DLY] = (uint8_t)(i % AFX_NDLY);
    afx_edit &= (uint8_t)~(1u << AFX_S_DLY);
    song.g[G_DTIME] = d->time;
    song.g[G_DFDBK] = d->fb;
    song.g[G_DCOLOR] = d->col;
    song.g[G_DMIX] = d->mix;
}
static void afp_rev_apply(uint32_t i)
{
    const afx_rev_t *d = &AFX_REV[i % AFX_NREV];
    afx_sel[AFX_S_REV] = (uint8_t)(i % AFX_NREV);
    afx_edit &= (uint8_t)~(1u << AFX_S_REV);
    song.g[G_RTYPE] = d->type;
    song.g[G_RSIZE] = d->size;
    song.g[G_RDAMP] = d->damp;
}
static void afp_mst_apply(uint32_t i)
{
    uint32_t k;
    afx_sel[AFX_S_MST] = (uint8_t)(i % AFX_NMST);
    afx_edit &= (uint8_t)~(1u << AFX_S_MST);
    for (k = 0; k < AMX_N; k++)
        amx[k] = AFX_MST[i % AFX_NMST].v[k];
}
static uint32_t afp_step(uint32_t cur, uint32_t n, int32_t s)
{
    return (uint32_t)(((int32_t)cur + (s > 0 ? 1 : -1) + (int32_t)n) % (int32_t)n);
}

/* ---- the knobs --------------------------------------------------------------------------------------------- */
static void afp_part(uint32_t id, int32_t s)    /* a parameter of the chosen circle's part, through the queue */
{
    const param_desc_t *d = param_desc_of(eng_idx(trk[au.sel].eng_req), id);
    alt_qpush(au.sel, id, d->fmt == F_ENUM ? s : s * 2);
}
static void afp_g(uint32_t g, int32_t s, int32_t lo, int32_t hi)
{
    song.g[g] = (int16_t)clamp(song.g[g] + s * 2, lo, hi);
}
static void afp_turn(uint32_t i, int32_t s)
{
    uint32_t c = au.sel % AS_NCIRC;
    switch (afp.tab) {
    case AFP_DIST: {
        static const uint8_t ID[4] = {P_DIST, P_ITYPE, P_IA, P_IMIX};
        if (i == 1u) {                           /* TYPE: OFF .. CRUSH (the swept ones are not distortions) */
            int32_t v = clamp(trk[c].p[P_ITYPE] + (s > 0 ? 1 : -1), IT_OFF, IT_CRUSH);
            alt_qpush(c, P_ITYPE, v - trk[c].p[P_ITYPE]);
        } else
            afp_part(ID[i], s);
        afx_edit |= (uint8_t)(1u << c);
        return;
    }
    case AFP_DLY:
        if (i == 0u)
            song.g[G_DTIME] = AFP_DT[clamp((int32_t)afp_dt_pos() + (s > 0 ? 1 : -1), 0, (int32_t)AFP_NDT - 1)];
        else if (i == 1u)
            afp_g(G_DFDBK, s, 0, 120);
        else if (i == 2u)
            afp_g(G_DCOLOR, s, 0, 127);
        else {
            afp_part(P_DLY, s);
            return;
        }
        afx_edit |= 1u << AFX_S_DLY;
        return;
    case AFP_REV:
        if (i == 0u)
            song.g[G_RTYPE] = (int16_t)clamp(song.g[G_RTYPE] + (s > 0 ? 1 : -1), 0, 2);
        else if (i == 1u)
            afp_g(G_RSIZE, s, 0, 127);
        else if (i == 2u)
            afp_g(G_RDAMP, s, 0, 127);
        else {
            afp_part(P_REV, s);
            return;
        }
        afx_edit |= 1u << AFX_S_REV;
        return;
    default: {
        uint32_t k = (afp.row & 1u) * 4u + i;
        if (k == AMX_SRC)
            amx[k] = (uint8_t)clamp((int32_t)amx[k] + (s > 0 ? 1 : -1), 0, AMS_N - 1);
        else
            amx[k] = (uint8_t)clamp((int32_t)amx[k] + s * 2, 0, 127);
        afx_edit |= 1u << AFX_S_MST;
        return;
    }
    }
}

/* the FX page has this frame */
static void afp_input(void)
{
    uint32_t i;
    int32_t s;
    if ((s = panel_enc(EN_ALGO)) != 0)
        afp.tab = (uint8_t)clamp((int32_t)afp.tab + (s > 0 ? 1 : -1), 0, AFP_N - 1);
    if ((s = panel_enc(EN_SELECT)) != 0) {
        if (afp.tab == AFP_MST)
            afp.row = (uint8_t)(s > 0 ? 1 : 0);
        else
            alt_select((uint32_t)((int32_t)au.sel + s + 3 * 16));
    }
    if ((s = panel_enc(EN_PRESET)) != 0) {
        uint32_t c = au.sel % AS_NCIRC;
        switch (afp.tab) {
        case AFP_DIST: afp_dist_apply(c, afp_step(afx_sel[c], AFX_NDIST, s)); alt_say(AFX_DIST[afx_sel[c]].name); break;
        case AFP_DLY: afp_dly_apply(afp_step(afx_sel[AFX_S_DLY], AFX_NDLY, s)); alt_say(AFX_DLY[afx_sel[AFX_S_DLY]].name); break;
        case AFP_REV: afp_rev_apply(afp_step(afx_sel[AFX_S_REV], AFX_NREV, s)); alt_say(AFX_REV[afx_sel[AFX_S_REV]].name); break;
        default: afp_mst_apply(afp_step(afx_sel[AFX_S_MST], AFX_NMST, s)); alt_say(AFX_MST[afx_sel[AFX_S_MST]].name); break;
        }
    }
    for (i = 0; i < 4u; i++)
        if ((s = panel_enc(EN_K1 + i)) != 0)
            afp_turn(i, s);
}

/* ---- the screen -------------------------------------------------------------------------------------------- */
static uint16_t afp_dim(uint16_t c, int32_t pct)   /* c towards the background (in colour whatever the theme) */
{
    return alt_mixc(ALT_BG, c, pct);
}
/* knob value k of the tab: its name, its text, a fill of 50 px (-1: text only) */
static void afp_kv(uint32_t k, const char **name, char *txt, int32_t *fill)
{
    static const char *const KN[3][4] = {{"DRIVE", "TYPE", "AMOUNT", "MIX"}, {"TIME", "FEEDBK", "COLOR", "SEND"},
                                         {"TYPE", "SIZE", "DAMP", "SEND"}};
    uint32_t c = au.sel % AS_NCIRC;
    const track_t *t = &trk[c];
    int32_t v = 0, max = 127;
    txt[0] = 0;
    *fill = -1;
    if (afp.tab == AFP_MST) {
        *name = AMX_NAME[k % AMX_N];
        v = amx[k % AMX_N];
        if (k == AMX_SRC) {
            const char *s = AMS_NAME[v % AMS_N];
            uint32_t n = 0;
            while (s[n] && n < 10u)
                txt[n] = s[n], n++;
            txt[n] = 0;
            return;
        }
        if (k <= AMX_HIGH) {                     /* the bands: flat at 64, - towards gone, + towards +6 dB */
            if (v == 64)
                txt[0] = '0', txt[1] = 0;
            else if (v == 0) {
                const char *s = "KILL";
                uint32_t n = 0;
                while (s[n])
                    txt[n] = s[n], n++;
                txt[n] = 0;
            } else
                alt_num(txt, v < 64 ? -(64 - v) * 100 / 64 : (v - 64) * 100 / 63, 1);
            return;
        }
        *fill = v * 50 / 127;
        return;
    }
    *name = KN[afp.tab][k & 3u];
    switch (afp.tab * 4u + k) {
    case 0: v = t->p[P_DIST]; break;
    case 1: {
        const char *s = N_ITYPE[clamp(t->p[P_ITYPE], 0, IT_N - 1)];
        uint32_t n = 0;
        while (s[n])
            txt[n] = s[n], n++;
        txt[n] = 0;
        return;
    }
    case 2: v = t->p[P_IA]; break;
    case 3: v = t->p[P_IMIX]; break;
    case 4: {
        const char *s = N_DTIME[clamp(song.g[G_DTIME], 0, 12)];
        uint32_t n = 0;
        while (s[n])
            txt[n] = s[n], n++;
        txt[n] = 0;
        return;
    }
    case 5: v = song.g[G_DFDBK]; max = 120; break;
    case 6: v = song.g[G_DCOLOR]; break;
    case 7: v = t->p[P_DLY]; break;
    case 8: {
        const char *s = AFP_RTYPE[clamp(song.g[G_RTYPE], 0, 2)];
        uint32_t n = 0;
        while (s[n])
            txt[n] = s[n], n++;
        txt[n] = 0;
        return;
    }
    case 9: v = song.g[G_RSIZE]; break;
    case 10: v = song.g[G_RDAMP]; break;
    default: v = t->p[P_REV]; break;
    }
    *fill = clamp(v, 0, max) * 50 / max;
}

static void afp_knob(int32_t x, int32_t y, uint32_t k, uint16_t col, int lit)
{
    const char *name;
    char txt[12];
    int32_t fill;
    afp_kv(k, &name, txt, &fill);
    cv_text(x, y, &AF_S, name, lit ? ALT_DIM : ALT_GREY);
    if (fill >= 0) {
        cv_rect(x, y + 21, 50, 5, ALT_DARK);
        cv_rect(x, y + 21, fill, 5, lit ? col : ALT_GREY);
    } else
        cv_text(x, y + 16, &AF_S, txt, lit ? ALT_INK : ALT_GREY);
}

/* the preset's name, big, with its place in the list (and a * once a knob moved it) */
static void afp_head(const char *name, uint32_t i, uint32_t n, int edited, uint16_t col)
{
    char b[8];
    int32_t w;
    cv_text(8, 36, &AF_S, "PRESET", ALT_DIM);
    b[0] = (char)('0' + (i + 1u) / 10u);
    b[1] = (char)('0' + (i + 1u) % 10u);
    b[2] = '/';
    b[3] = (char)('0' + n / 10u);
    b[4] = (char)('0' + n % 10u);
    b[5] = 0;
    cv_text_r(232, 36, &AF_S, b[0] == '0' ? b + 1 : b, ALT_GREY, ALT_BG);
    w = cv_text(8, 52, &AF_L, name, col);
    if (edited)
        cv_text(8 + text_w(&AF_L, name) + 4, 52, &AF_M, "*", ALT_DIM);
    (void)w;
}

/* the three circles' sends of a bus (P_DLY / P_REV), the chosen one lit */
static void afp_sends(uint32_t id, int32_t y)
{
    uint32_t c;
    cv_text(8, y, &AF_S, "SENDS", ALT_DIM);
    for (c = 0; c < AS_NCIRC; c++) {
        int32_t x = 64 + (int32_t)c * 58, v = clamp(trk[c].p[id], 0, 127) * 50 / 127;
        cv_rect(x, y + 6, 50, 5, ALT_DARK);
        cv_rect(x, y + 6, v, 5, c == au.sel ? ALT_COL[c] : afp_dim(ALT_COL[c], 45));
    }
}

static void afp_dist_pic(int32_t y0, int32_t h, uint16_t col)   /* one cycle of a sine through the distortion */
{
    const track_t *t = &trk[au.sel % AS_NCIRC];
    int32_t x, py = 0, ty = t->p[P_ITYPE], mix = t->p[P_IMIX];
    int32_t g = 4096 + t->p[P_DIST] * t->p[P_DIST] * 2, ga = 4096 + t->p[P_IA] * t->p[P_IA] * 2;
    cv_rect(8, y0 + h / 2, 224, 1, ALT_DARK);
    for (x = 0; x < 224; x++) {
        int32_t s = sine_i((uint32_t)x * (0xFFFFFFFFu / 224u)), v = s, w, y;
        v = softclip((int32_t)(((int64_t)v * g) >> 12));
        w = v;
        if (ty == IT_SOFT || ty == IT_FUZZ)
            w = softclip((int32_t)(((int64_t)v * ga) >> 12));
        else if (ty == IT_HARD)
            w = clamp((int32_t)(((int64_t)v * ga) >> 12), -16384, 16384) * 2;
        else if (ty == IT_FOLD) {
            int32_t f = (int32_t)(((int64_t)v * ga) >> 13), p = (f + 16384) & 65535;
            w = (p < 32768 ? p - 16384 : 49151 - p) * 2;
        } else if (ty == IT_CRUSH) {
            int32_t sh = 15 - (t->p[P_IA] >> 3);
            w = (v >> sh) << sh;
        }
        if (ty == IT_FUZZ && w < 0)
            w /= 2;
        if (ty != IT_OFF)
            v = v + (((w - v) * clamp(mix, 0, 127)) >> 7);
        y = y0 + h / 2 - (clamp(v, -32767, 32767) * (h / 2 - 2)) / 32767;
        if (x)
            cv_line(8 + x - 1, py, 8 + x, y, col);
        py = y;
    }
}

static void afp_dly_pic(int32_t y0, int32_t h, uint16_t col)    /* the echoes, the time to scale, the feedback's fall */
{
    static const uint16_t TQ[13] = {48, 24, 12, 6, 16, 8, 96, 192, 384, 768, 36, 18, 72};   /* in 1/48 of a beat .. */
    int32_t sp = TQ[clamp(song.g[G_DTIME], 0, 12)] * 2, a = 32767, fb = song.g[G_DFDBK] * 230, x = 8, k;
    if (sp < 8)
        sp = 8;
    cv_rect(8, y0 + h - 1, 224, 1, ALT_DARK);
    cv_rect(x, y0, 4, h, ALT_INK);               /* the hit */
    for (k = 1; k < 24 && (x += sp) < 228 && a > 400; k++) {
        int32_t bh = (h * (a >> 7)) >> 8;
        cv_rect(x, y0 + h - bh, 4, bh ? bh : 1, afp_dim(col, 40 + 60 * a / 32767));
        a = mulq15(a, fb);
    }
}

static void afp_rev_pic(int32_t y0, int32_t h, uint16_t col)    /* the tail's fall: SIZE its length, DAMP its colour */
{
    int32_t len = 40 + song.g[G_RSIZE] * 14 / 10, x, d = song.g[G_RDAMP];
    for (x = 0; x < 224; x++) {
        int32_t r = x < len ? (len - x) * 256 / len : 0, bh = (h * r * r) >> 16;
        if (bh)
            cv_rect(8 + x, y0 + h - bh, 1, bh, afp_dim(col, 100 - d * 50 / 127 - x * 40 / 224));
    }
    cv_rect(8, y0 + h - 1, 224, 1, ALT_DARK);
}

static void afp_meter(int32_t x, int32_t y, const char *name, uint32_t pct, uint16_t col)
{
    char b[8];
    cv_text(x, y, &AF_S, name, ALT_DIM);
    cv_rect(x, y + 20, 100, 6, ALT_DARK);
    cv_rect(x, y + 20, (int32_t)(pct > 100u ? 100u : pct), 6, col);
    b[0] = '-';
    alt_num(b + 1, (int32_t)pct, 1);
    cv_text_r(x + 100, y, &AF_S, pct ? b : "0", ALT_INK, ALT_BG);
}

static void afp_scene(void)
{
    uint32_t i, c = au.sel % AS_NCIRC;
    uint16_t cc = ALT_COL[c];
    int32_t x = 8;
    for (i = 0; i < AFP_N; i++) {                /* the tabs: DIST in the chosen circle's colour */
        int32_t w = text_w(&AF_S, AFP_TAB[i]);
        uint16_t tc = i == AFP_DIST ? cc : ALT_INK;
        cv_text(x, 4, &AF_S, AFP_TAB[i], i == afp.tab ? tc : ALT_GREY);
        if (i == afp.tab)
            cv_rect(x, 20, w, 2, tc);
        x += w + 18;
    }
    switch (afp.tab) {
    case AFP_DIST:
        afp_head(AFX_DIST[afx_sel[c] % AFX_NDIST].name, afx_sel[c], AFX_NDIST, (afx_edit >> c) & 1u, cc);
        alt_disc(231, 12, 5, cc);                /* (whose: the chosen circle) */
        afp_dist_pic(92, 92, cc);
        break;
    case AFP_DLY:
        afp_head(AFX_DLY[afx_sel[AFX_S_DLY] % AFX_NDLY].name, afx_sel[AFX_S_DLY], AFX_NDLY, (afx_edit >> AFX_S_DLY) & 1u, ALT_INK);
        afp_dly_pic(94, 64, cc);
        afp_sends(P_DLY, 168);
        break;
    case AFP_REV:
        afp_head(AFX_REV[afx_sel[AFX_S_REV] % AFX_NREV].name, afx_sel[AFX_S_REV], AFX_NREV, (afx_edit >> AFX_S_REV) & 1u, ALT_INK);
        afp_rev_pic(94, 64, cc);
        afp_sends(P_REV, 168);
        break;
    default: {
        uint32_t r;
        afp_head(AFX_MST[afx_sel[AFX_S_MST] % AFX_NMST].name, afx_sel[AFX_S_MST], AFX_NMST, (afx_edit >> AFX_S_MST) & 1u, ALT_INK);
        for (r = 0; r < 2u; r++) {               /* the two rows; the one the knobs move lit, a bar at its left */
            int32_t y = 96 + (int32_t)r * 46;
            if (r == afp.row)
                cv_rect(0, y, 3, 34, ALT_INK);
            for (i = 0; i < 4u; i++)
                afp_knob(8 + (int32_t)i * 58, y, r * 4u + i, ALT_INK, r == afp.row);
        }
        afp_meter(8, 196, "COMP", am_gr, ALT_COL[2]);
        afp_meter(128, 196, amx[AMX_DUCK] ? "DUCK" : "DUCK OFF", am_dk, ALT_COL[1]);
        return;
    }
    }
    for (i = 0; i < 4u; i++)                     /* the four knobs (DIST: AMOUNT and MIX grey with no insert) */
        afp_knob(8 + (int32_t)i * 58, 196, i, i == 3u || afp.tab == AFP_DIST ? cc : ALT_INK,
                 !(afp.tab == AFP_DIST && i >= 2u && !trk[c].p[P_ITYPE]));
}

/* what the FX page's picture depends on */
static uint32_t afp_sig(void)
{
    uint32_t sg = afp.tab * 7u + afp.row * 11u + au.sel * 13u + afx_edit * 17u, k;
    for (k = 0; k < AFX_NS; k++)
        sg = sg * 31u + afx_sel[k];
    for (k = 0; k < AMX_N; k++)
        sg = sg * 31u + amx[k];
    for (k = 0; k < AS_NCIRC; k++)
        sg = sg * 31u + (uint32_t)(trk[k].p[P_DIST] + trk[k].p[P_ITYPE] * 3 + trk[k].p[P_IA] * 5 + trk[k].p[P_IMIX] * 7 +
                                   trk[k].p[P_DLY] * 11 + trk[k].p[P_REV] * 13);
    sg = sg * 31u + (uint32_t)(song.g[G_DTIME] + song.g[G_DFDBK] * 3 + song.g[G_DCOLOR] * 5 + song.g[G_RTYPE] * 7 +
                               song.g[G_RSIZE] * 11 + song.g[G_RDAMP] * 13);
    if (afp.tab == AFP_MST)
        sg = sg * 31u + am_gr / 4u * 7u + am_dk / 4u * 977u;
    return sg;
}
