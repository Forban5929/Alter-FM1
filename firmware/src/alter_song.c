/* SPDX-License-Identifier: GPL-3.0-only
 * SONG (Romain 05/11, after the Analog Rytm's song mode): three slots A B C, each the rhythms of the three circles as
 * they played when stored (their steps, lanes, RANGEs, the die's draw, the steps TRIGGER added, the REC'd fills, REC's
 * moves, the mutes; NOT the sounds, nor the root, chord and dose of the SCALE page), and a chain of up to 16 links (a
 * slot, 1 2 4 or 8 bars) that loops.
 *
 * The audio side (included by alter_play.c after alter_rec.c): at a bar line (alt_gstep % 16 == 0, the PLAY grid, before
 * the step plays) the slot asked for, or the chain's next link, replaces the three circles at once, each from its
 * step 1. The UI (alter_ui.c alt_sg_*) writes the slots (irq off), the chain, and asks; it reads sg_done_cnt back.
 * The slots are on the flash (alter_ui.c), not in the scenes: the same song whatever scene plays. */
#define SG_NSLOT 3u
#define SG_MAXL 16u
#define SG_MAGIC (0x47530000u | (uint32_t)(sizeof(as_circ_t) & 0xFFFFu))   /* "SG" + the circle's size (a firmware
                                                  * whose circles differ reads no slot) */
typedef struct {
    uint32_t magic;
    uint8_t used, rsv[3];
    as_circ_t c[AS_NCIRC];
    uint8_t fxr[AS_NCIRC][AS_MAXLEN];
    uint8_t rec[AS_NCIRC][AR_PACK];
} sg_slot_t;
static sg_slot_t sg_slot[SG_NSLOT] __attribute__((section(".pool")));   /* (cleared or read from the flash at the
                                                  * UI's start: alter_ui.c alt_sg_boot) */
/* UI -> here */
static uint8_t sg_chain[SG_MAXL];                /* a link: its slot (0..2) | its bars' code << 2 (0..3: 1 2 4 8) */
static volatile uint8_t sg_n;                    /* the chain's links */
static volatile uint8_t sg_on;                   /* the chain plays */
static volatile uint8_t sg_ask;                  /* a slot asked for (+ 1) .. */
static volatile uint32_t sg_ask_cnt, sg_start_cnt;   /* .. (counted); the chain from its first link */
static uint32_t sg_ask_seen, sg_start_seen;
static uint8_t sg_restart;                       /* (PLAY: the chain from its first link) */
/* here -> UI */
static volatile uint8_t sg_play;                 /* the slot playing + 1 (0: none taken yet) */
static volatile uint8_t sg_link;                 /* the chain's link playing .. */
static volatile uint8_t sg_left;                 /* .. its bars left, this one counted */
static volatile uint32_t sg_done_cnt;            /* a slot taken */

static uint32_t sg_bars(uint32_t code) { return 1u << (code & 3u); }

/* slot s on the three circles, from this step (each from its step 1); the SCALE page's root, chord, dose kept */
static void sg_take(uint32_t s)
{
    uint32_t c, k;
    const sg_slot_t *S = &sg_slot[s % SG_NSLOT];
    for (c = 0; c < AS_NCIRC; c++) {
        as_circ_t *cc = &alt_seq.c[c];
        uint8_t dose = cc->next.dose, root = cc->next.root, chord = cc->next.chord;
        *cc = S->c[c];
        cc->cfg.dose = cc->next.dose = dose;
        cc->cfg.root = cc->next.root = root;
        cc->cfg.chord = cc->next.chord = chord;
        if (!cc->cfg.len)
            cc->cfg.len = cc->next.len = 16;
        cc->pos = 0;
        as_repat(cc);
        for (k = 0; k < AS_MAXLEN; k++)
            alt_fxr[c][k] = S->fxr[c][k];
        ar_unpack(c, S->rec[c]);
    }
    alt_post = 0;                                /* (what the UI asked before: the slot's instead; it follows, alt_sg_sync) */
    alt_post_now = 0;
}

/* alter_step, before the step: at a bar line, what plays next */
static void sg_bar(void)
{
    uint32_t want = 0, n = sg_n;
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
