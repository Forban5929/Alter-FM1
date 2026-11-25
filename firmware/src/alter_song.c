/* SPDX-License-Identifier: GPL-3.0-only
 * SONG (Romain 05/11, after the Analog Rytm's song mode): three slots A B C, each the rhythms of the three circles as
 * they played when stored (their steps, lanes, RANGEs, the die's draw, the steps TRIGGER added, the REC'd fills, REC's
 * moves, the mutes; NOT the sounds, nor the root, chord and dose of the SCALE page), and a chain of up to 16 links (a
 * slot, 1 2 4 or 8 bars) that loops.
 *
 * The audio side (included by alter_play.c after alter_rec.c): at a bar line (alt_gstep % 16 == 0, the PLAY grid, before
 * the step plays) the slot asked for, or the chain's next link, replaces the three circles at once, each from its
 * step 1. The UI (alter_ui.c alt_sg_*) writes the slots (irq off), the chain, and asks; it reads sg_done_cnt back.
 * v1.36 (Romain 25/11, "une chaîne est juste un pattern différent et nombre de triggers, même chance, mêmes settings"):
 * a slot is now light: for each circle its HITS, ROT and LEN only (sounds, CHANCE, lanes, REC, the rest: the scene's);
 * the slots and the chain belong to the SCENE (alter_store.c: the last 48 bytes of a scene, SG_REC). A slot's key tapped
 * when it is empty captures what plays (alter_ui.c alt_sg_*). The slots of before v1.36 (whole rhythms, on the flash,
 * objects 10..13) are read once at the start as HITS ROT LEN (sg_old_t) for the scenes stored without slots. */
#define SG_NSLOT 3u
#define SG_MAXL 16u
#define SG_MAGIC (0x47530000u | (uint32_t)(sizeof(as_circ_t) & 0xFFFFu))   /* (the old slots' mark) */
typedef struct {
    uint8_t used;
    uint8_t hits[AS_NCIRC], rot[AS_NCIRC], len[AS_NCIRC];
} sg_slot_t;
static sg_slot_t sg_slot[SG_NSLOT];
/* a scene's SONG record (alter_store.c, the end of a scene, a BLOB, the crash copy): [0] 1 = stored, [1] the used slots
 * (bits), [2..28] the slots (HITS ROT LEN a circle), [29] the chain's links, [30..45] the chain, 2 spare */
#define SG_REC 48u
/* UI -> here */
static uint8_t sg_chain[SG_MAXL];                /* a link: its slot (0..2) | its bars' code << 2 (0..3: 1 2 4 8) */
static volatile uint8_t sg_n;                    /* the chain's links */
static volatile uint8_t sg_on;                   /* the chain plays */
static volatile uint8_t sg_ask;                  /* a slot asked for (+ 1) .. */
static volatile uint32_t sg_ask_cnt, sg_start_cnt;   /* .. (counted); the chain from its first link */
static uint32_t sg_ask_seen, sg_start_seen;
static uint8_t sg_restart;                       /* (PLAY: the chain from its first link) */
static volatile uint8_t sg_loaded;               /* (v1.36: a scene brought slots and a chain: the UI's copy follows) */
/* here -> UI */
static volatile uint8_t sg_play;                 /* the slot playing + 1 (0: none taken yet) */
static volatile uint8_t sg_link;                 /* the chain's link playing .. */
static volatile uint8_t sg_left;                 /* .. its bars left, this one counted */
static volatile uint32_t sg_done_cnt;            /* a slot taken */

static uint32_t sg_bars(uint32_t code) { return 1u << (code & 3u); }
static volatile uint8_t sg_detach;               /* UI: a scene / a pattern loaded: the circles are no slot's now */

/* a slot from the circles' settings as asked (cfg: au.cfg on the UI side, the circles' next here) */
static void sg_fill(sg_slot_t *S, const as_cfg_t *cfg0, uint32_t stride)
{
    uint32_t c;
    for (c = 0; c < AS_NCIRC; c++) {
        const as_cfg_t *f = (const as_cfg_t *)((const uint8_t *)cfg0 + c * stride);
        S->hits[c] = f->hits;
        S->rot[c] = f->rot;
        S->len[c] = f->len ? f->len : 16u;
    }
    S->used = 1;
}
/* (v1.36: a slot is a snapshot, as captured: an edit while it plays no longer goes into it (before: it did, as on the
 * Rytm); to keep an edit, hold the slot's key 1 s: replaced) */

/* slot s on the three circles from this step (each from its step 1): their HITS ROT LEN, nothing else */
static void sg_take(uint32_t s)
{
    uint32_t c;
    const sg_slot_t *S = &sg_slot[s % SG_NSLOT];
    for (c = 0; c < AS_NCIRC; c++) {
        as_circ_t *cc = &alt_seq.c[c];
        uint8_t len = (uint8_t)(S->len[c] ? (S->len[c] > AS_PLAYLEN ? AS_PLAYLEN : S->len[c]) : 16u);
        uint8_t hits = (uint8_t)(S->hits[c] > len ? len : S->hits[c]);
        cc->cfg.len = cc->next.len = len;
        cc->cfg.hits = cc->next.hits = hits;
        cc->cfg.rot = cc->next.rot = (uint8_t)(S->rot[c] % len);
        cc->cfg.frot %= len;
        cc->next.frot %= len;
        cc->pos = 0;
        as_repat(cc);
    }
    alt_post = 0;                                /* (what the UI asked before: the slot's instead; it follows, alt_sg_sync) */
    alt_post_now = 0;
}

/* a scene's SONG record from the slots and the chain, and back (b[0] != 1: stored before v1.36, nothing changed) */
static void sg_rec_get(uint8_t *b)
{
    uint32_t s, c, i;
    for (i = 0; i < SG_REC; i++)
        b[i] = 0;
    b[0] = 1;
    for (s = 0; s < SG_NSLOT; s++) {
        b[1] |= (uint8_t)(sg_slot[s].used << s);
        for (c = 0; c < AS_NCIRC; c++) {
            b[2u + (s * AS_NCIRC + c) * 3u] = sg_slot[s].hits[c];
            b[3u + (s * AS_NCIRC + c) * 3u] = sg_slot[s].rot[c];
            b[4u + (s * AS_NCIRC + c) * 3u] = sg_slot[s].len[c];
        }
    }
    b[29] = sg_n;
    for (i = 0; i < SG_MAXL; i++)
        b[30u + i] = sg_chain[i];
}
static void sg_rec_put_raw(const uint8_t *b)   /* (irq off: alter_store.c sg_rec_put) */
{
    uint32_t s, c, i;
    if (b[0] != 1u)
        return;
    for (s = 0; s < SG_NSLOT; s++) {
        sg_slot[s].used = (uint8_t)((b[1] >> s) & 1u);
        for (c = 0; c < AS_NCIRC; c++) {
            uint32_t len = b[4u + (s * AS_NCIRC + c) * 3u];
            len = len < 1u || len > AS_PLAYLEN ? 16u : len;
            sg_slot[s].len[c] = (uint8_t)len;
            sg_slot[s].hits[c] = (uint8_t)(b[2u + (s * AS_NCIRC + c) * 3u] > len ? len : b[2u + (s * AS_NCIRC + c) * 3u]);
            sg_slot[s].rot[c] = (uint8_t)(b[3u + (s * AS_NCIRC + c) * 3u] % len);
        }
    }
    sg_n = (uint8_t)(b[29] > SG_MAXL ? SG_MAXL : b[29]);
    for (i = 0; i < SG_MAXL; i++)
        sg_chain[i] = (uint8_t)((b[30u + i] & 3u) % SG_NSLOT | (b[30u + i] & 12u));
    sg_on = 0;
    sg_play = 0;
    sg_loaded = 1;
}

/* alter_step, before the step: at a bar line, what plays next */
static void sg_bar(void)
{
    uint32_t want = 0, n = sg_n;
    if (sg_detach) {                             /* (a scene loaded: nothing to keep in a slot) */
        sg_detach = 0;
        sg_play = 0;
    }
    if (alt_gstep % 16u)
        return;
    if (sg_restart || sg_start_cnt != sg_start_seen) {   /* the chain from its first link: as if its last ended */
        sg_start_seen = sg_start_cnt;
        sg_restart = 0;
        sg_link = (uint8_t)(n ? n - 1u : 0u);
        sg_left = 0;
    }
    if (sg_on && n) {
        if (sg_left > 1u)
            sg_left--;
        else {
            sg_link = (uint8_t)((sg_link + 1u) % n);
            sg_left = (uint8_t)sg_bars(sg_chain[sg_link] >> 2);
            want = (sg_chain[sg_link] & 3u) + 1u;
        }
    }
    if (sg_ask_cnt != sg_ask_seen) {             /* a slot's key (the UI has stopped the chain) */
        sg_ask_seen = sg_ask_cnt;
        want = sg_ask;
    }
    if (want && want <= SG_NSLOT && sg_slot[want - 1u].used) {
        sg_take(want - 1u);
        sg_play = (uint8_t)want;
        sg_done_cnt++;
    }
}
