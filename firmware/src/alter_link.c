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
#define LK_VERSION "ALTER LINK 1"
enum { LKC_INFO = 1, LKC_STATE, LKC_DESC, LKC_SET, LKC_SOUND, LKC_SCENE, LKC_WATCH, LKC_CHANGED, LKC_RELOAD, LKC_PING };

/* the sound's parameters, in this order (STATE): the engine's eight, then what a USER scene keeps (ALT_SP) */
static const uint8_t LK_IDS[] = {P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7, P_ATK, P_DEC, P_SUS, P_REL,
                                 P_DIST, P_CHOR, P_DLY, P_REV, P_ITYPE, P_IA, P_IMIX, P_LEVEL, P_PAN, P_GLIDE};
#define LK_NID (sizeof LK_IDS)

static uint8_t lk_out[1400];
static uint32_t lk_n;
static struct {
    uint8_t on;
    uint32_t last_ms, pass_ms;
    int16_t sh[AS_NCIRC][LK_NID];    /* what the page knows */
    uint8_t eng[AS_NCIRC], pre[AS_NCIRC];
    uint32_t pcnt[AS_NCIRC];
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
    case LKC_SCENE: {                                 /* op 0: the USER slots used; 1 slot: store here (as SAVE held);
                                                      * 2 slot: load. Reply op, slot, rc (1 ok, 0 empty / protected,
                                                      * 2 the flash failed), then for op 0 one used byte per slot */
        int rc = 0;
        if (na < 1u || a[0] > 2u || (a[0] && (na != 2u || a[1] >= ALT_NUSER)))
            return;
        if (a[0] == 1u)
            rc = alt_row_save(0, 1u + ALT_NFACT + a[1]);
        else if (a[0] == 2u)
            rc = alt_row_load(0, 1u + ALT_NFACT + a[1]);
        lk_begin(LKC_SCENE);
        lk_b(a[0]);
        lk_b(na > 1u ? a[1] : 0u);
        lk_b(rc < 0 ? 2u : (uint32_t)rc);
        if (a[0] == 0u)
            for (k = 0; k < ALT_NUSER; k++)
                lk_b(alt_bank_s.s[k].used ? 1u : 0u);
        lk_send();
        if (a[0] == 2u && rc > 0)
            for (c = 0; c < AS_NCIRC; c++)
                lk_shadow(c);                        /* (the page re-reads STATE after a load) */
        return;
    }
    case LKC_WATCH:                                   /* on 0/1 */
        if (na != 1u)
            return;
        lk.on = (uint8_t)(a[0] & 1u);
        for (c = 0; c < AS_NCIRC; c++)
            lk_shadow(c);
        lk_begin(LKC_WATCH);
        lk_b(lk.on);
        lk_send();
        return;
    case LKC_PING:
        lk_begin(LKC_PING);
        lk_b(0);
        lk_send();
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
