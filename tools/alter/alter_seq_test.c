/* SPDX-License-Identifier: GPL-3.0-only
 * Tests of ALTER's sequencer (firmware/src/alter_seq.c), on the PC:
 *   cc -std=c99 -O2 -Wall -o build/alter/alter_seq_test tools/alter/alter_seq_test.c && build/alter/alter_seq_test */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../firmware/src/alter_seq.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t popcount(uint32_t x) { uint32_t n = 0; while (x) { n += x & 1u; x >>= 1; } return n; }

/* the steps circle 0 plays over n sixteenths, as a bit string from the current position */
static void run(as_seq_t *q, uint32_t n, as_event_t *ev)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        as_step(q, &ev[i]);
}

static as_cfg_t cfg(uint8_t len, uint8_t hits, uint8_t rot, uint8_t prob)
{
    as_cfg_t c = {len, hits, rot, prob, 0, 0, 0};
    return c;
}

static void solo(as_seq_t *q, uint32_t keep)           /* only circle keep plays (others muted, now) */
{
    uint32_t i;
    for (i = 0; i < AS_NCIRC; i++) {
        q->c[i].cfg.mute = q->c[i].next.mute = (uint8_t)(i != keep);
    }
}

int main(void)
{
    as_seq_t q, q2;
    as_event_t ev[256], ev2[256];
    uint32_t i, n, k;

    /* 1. euclidean patterns: E(3,8) is x..x..x., every E(k,n) has k hits, the rotation turns it */
    CHECK(as_euclid(8, 3, 0) == ((1u << 0) | (1u << 3) | (1u << 6)), "E(3,8) = %x", as_euclid(8, 3, 0));
    CHECK(as_euclid(16, 4, 0) == 0x1111u, "E(4,16) = %x", as_euclid(16, 4, 0));
    CHECK(as_euclid(8, 3, 1) == ((1u << 1) | (1u << 4) | (1u << 7)), "E(3,8) rot 1");
    for (n = 1; n <= 32; n++)
        for (k = 0; k <= n; k++)
            CHECK(popcount(as_euclid(n, k, k % n)) == k, "E(%u,%u) has %u hits", k, n, popcount(as_euclid(n, k, k % n)));

    /* 2. a change waits for the next cycle */
    as_init(&q, 1);
    solo(&q, AS_HITS);
    as_set(&q, AS_HITS, cfg(16, 4, 0, 127));
    run(&q, 5, ev);                                    /* steps 0..4 of the first cycle */
    as_set(&q, AS_HITS, cfg(16, 16, 0, 127));          /* every step, asked at step 5 */
    run(&q, 11, ev);                                   /* steps 5..15: still E(4,16): hits at 8 and 12 */
    for (i = 0; i < 11; i++)
        CHECK(ev[i].on == ((i + 5) % 4 == 0), "step %u before the cycle: on %u", i + 5, ev[i].on);
    run(&q, 16, ev);                                   /* the next cycle: every step */
    for (i = 0; i < 16; i++)
        CHECK(ev[i].on, "step %u of the new cycle silent", i);

    /* 3. a roll waits for the next cycle, then stays the same cycle after cycle */
    as_init(&q, 7);
    solo(&q, AS_HITS);
    q.scale = AS_SC_CHR;
    as_set(&q, AS_HITS, (as_cfg_t){8, 8, 0, 127, 1, 127, 0});
    run(&q, 8, ev);                                    /* the cycle that takes the new settings (die on) */
    run(&q, 3, ev);
    {
        uint8_t before[8], after[8];
        int changed = 0;
        for (i = 0; i < 8; i++)
            before[i] = q.c[AS_HITS].pitch[i];
        as_roll(&q, AS_HITS);
        for (i = 0; i < 8; i++)
            CHECK(q.c[AS_HITS].pitch[i] == before[i], "the roll changed step %u before the cycle ended", i);
        run(&q, 5, ev);                                /* the end of this cycle */
        run(&q, 8, ev);                                /* the new draw plays */
        for (i = 0; i < 8; i++) {
            after[i] = q.c[AS_HITS].pitch[i];
            changed |= after[i] != before[i];
        }
        CHECK(changed, "a roll at DOSE 127 changed no pitch");
        run(&q, 8, ev2);                               /* the next cycle: the same notes */
        for (i = 0; i < 8; i++)
            CHECK(ev[i].note == ev2[i].note && ev[i].vel == ev2[i].vel, "step %u differs between cycles", i);
    }

    /* 4. locks keep a parameter out of the draw */
    as_init(&q, 3);
    q.locks = AS_LK_PITCH | AS_LK_MRPH;
    as_set(&q, AS_HITS, (as_cfg_t){16, 16, 0, 127, 1, 127, 0});
    as_roll(&q, AS_HITS);
    run(&q, 16, ev);
    for (i = 0; i < AS_MAXLEN; i++) {
        CHECK(q.c[AS_HITS].pitch[i] == 0, "pitch locked but drawn at step %u", i);
        CHECK(q.c[AS_HITS].timb[i][AS_MRPH] == 0, "MRPH locked but drawn at step %u", i);
    }
    {
        int moved = 0;
        for (i = 0; i < AS_MAXLEN; i++)
            moved |= q.c[AS_HITS].timb[i][AS_SPRD] != 0;
        CHECK(moved, "SPRD not locked but never drawn");
    }

    /* 5. DOSE 0: the die changes nothing */
    as_init(&q, 5);
    as_set(&q, AS_HITS, (as_cfg_t){16, 16, 0, 127, 1, 0, 0});
    as_roll(&q, AS_HITS);
    run(&q, 32, ev);
    for (i = 16; i < 32; i++)
        CHECK(ev[i].note == q.root && !ev[i].glide && ev[i].timb[0] == 0, "DOSE 0 moved step %u", i);

    /* 6. ACCENT alone plays nothing; on a hit it makes it loud; GHOST plays only where HITS does not */
    as_init(&q, 9);
    as_set(&q, AS_HITS, cfg(8, 1, 0, 127));            /* step 0 */
    as_set(&q, AS_GHOST, cfg(8, 2, 0, 127));           /* steps 0, 4 */
    as_set(&q, AS_ACCENT, cfg(8, 4, 0, 127));          /* steps 0, 2, 4, 6 */
    run(&q, 8, ev);
    run(&q, 8, ev);
    CHECK(ev[0].on && !ev[0].ghost && ev[0].accent && ev[0].vel >= 120, "step 0: an accented hit (vel %u)", ev[0].vel);
    CHECK(ev[4].on && ev[4].ghost && ev[4].accent, "step 4: an accented ghost");
    CHECK(ev[4].vel < ev[0].vel, "an accented ghost stays under an accented hit");
    CHECK(!ev[2].on && !ev[6].on, "an accent alone played");

    /* 7. PROB: 0 never plays, 127 always, 64 about half */
    as_init(&q, 11);
    solo(&q, AS_HITS);
    as_set(&q, AS_HITS, cfg(16, 16, 0, 0));
    run(&q, 16, ev);
    run(&q, 64, ev);
    for (i = 0, n = 0; i < 64; i++)
        n += ev[i].on;
    CHECK(n == 0, "PROB 0 played %u", n);
    as_set(&q, AS_HITS, cfg(16, 16, 0, 64));
    run(&q, 16, ev);
    run(&q, 256, ev);
    for (i = 0, n = 0; i < 256; i++)
        n += ev[i].on;
    CHECK(n > 90 && n < 170, "PROB 64 played %u of 256", n);

    /* 8. the same seed plays the same */
    as_init(&q, 42);
    as_init(&q2, 42);
    for (k = 0; k < AS_NCIRC; k++) {
        as_set(&q, k, (as_cfg_t){(uint8_t)(13 + k), 5, 1, 100, 1, 80, 0});
        as_set(&q2, k, (as_cfg_t){(uint8_t)(13 + k), 5, 1, 100, 1, 80, 0});
        as_roll(&q, k);
        as_roll(&q2, k);
    }
    run(&q, 200, ev);
    run(&q2, 200, ev2);
    CHECK(!memcmp(ev, ev2, 200 * sizeof ev[0]), "two runs from the same seed differ");

    /* 9. odd lengths turn against each other: the whole pattern repeats only after LCM(len) steps */
    as_init(&q, 1);
    as_set(&q, AS_HITS, cfg(16, 5, 0, 127));
    as_set(&q, AS_GHOST, cfg(12, 7, 0, 127));
    as_set(&q, AS_ACCENT, cfg(5, 2, 0, 127));
    run(&q, 240, ev);                                  /* LCM(16, 12, 5) = 240 */
    run(&q, 240, ev2);
    CHECK(!memcmp(ev, ev2, 240 * sizeof ev[0]), "the 16/12/5 polyrhythm does not repeat after 240 steps");

    printf(fails ? "alter_seq: %d FAILED\n" : "alter_seq: all tests passed\n", fails);
    return fails != 0;
}
