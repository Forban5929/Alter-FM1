/* SPDX-License-Identifier: GPL-3.0-only
 * Tests of alter_seq.c (three independent circles, force and chance per step, the die at once).
 *   cc -std=c99 -O2 -Wall -o build/alter/alter_seq_test tools/alter/alter_seq_test.c && build/alter/alter_seq_test */
#include <stdint.h>
#include <stdio.h>
#include "../../firmware/src/alter_seq.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("%-64s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
}

/* n steps: which steps circle i played (bit s), and its events */
static uint64_t run(as_seq_t *q, uint32_t i, uint32_t n, as_event_t *keep)
{
    uint64_t m = 0;
    uint32_t s;
    for (s = 0; s < n; s++) {
        as_event_t ev[AS_NCIRC];
        as_step(q, ev);
        if (ev[i].on)
            m |= 1ull << s;
        if (keep)
            keep[s] = ev[i];
    }
    return m;
}

static uint32_t pat16(uint64_t m) { return (uint32_t)(m & 0xFFFF); }

int main(void)
{
    as_seq_t q;
    as_event_t e[64];
    uint32_t i, s;

    /* 1. the three circles are three euclidean patterns, each on its own */
    as_init(&q, 1);
    {
        as_seq_t a = q, b = q, c = q;
        check("circle 1 = 4/16", pat16(run(&a, 0, 16, 0)) == as_euclid(16, 4, 0));
        check("circle 2 = 3/16 turned by 2", pat16(run(&b, 1, 16, 0)) == as_euclid(16, 3, 2));
        check("circle 3 = 5/16 turned by 1", pat16(run(&c, 2, 16, 0)) == as_euclid(16, 5, 1));
    }
    /* 2. each circle plays its own root, normal force */
    as_init(&q, 1);
    {
        as_event_t ev[AS_NCIRC];
        as_step(&q, ev);
        check("step 1: circle 1 on its root (C2), normal force", ev[0].on && ev[0].note == 36 && ev[0].vel == 96);
    }
    /* 3. a change waits for the next cycle */
    as_init(&q, 1);
    run(&q, 0, 5, 0);
    {
        as_cfg_t c = q.c[0].next;
        c.hits = 8;
        as_set(&q, 0, c);
    }
    check("HITS 8 asked at step 5: the bar ends on 4/16",
          (run(&q, 0, 11, 0) << 5 | (as_euclid(16, 4, 0) & 31)) == as_euclid(16, 4, 0));
    check("HITS 8 from the next cycle", pat16(run(&q, 0, 16, 0)) == as_euclid(16, 8, 0));
    /* 4. mute at once, on that circle alone */
    as_init(&q, 1);
    {
        as_cfg_t c = q.c[1].next;
        as_seq_t q2;
        c.mute = 1;
        as_set(&q, 1, c);
        q2 = q;
        check("MUTE circle 2: silent at once", run(&q, 1, 16, 0) == 0);
        check("circle 1 untouched", pat16(run(&q2, 0, 16, 0)) == as_euclid(16, 4, 0));
    }
    /* 5. force per step and its own rotation */
    as_init(&q, 1);
    q.c[0].svel[0] = AS_VEL[AS_ACCENT];
    q.c[0].svel[4] = AS_VEL[AS_GHOST];
    run(&q, 0, 16, e);
    check("force: step 1 accent, step 5 ghost, step 9 normal", e[0].vel == 124 && e[4].vel == 40 && e[8].vel == 96);
    q.c[0].next.frot = 4;
    run(&q, 0, 16, e);
    check("FROT 4: the accent moves to step 5, hits stay", e[4].vel == 124 && e[0].vel == 96 && e[0].on && e[4].on);
    /* 6. chance per step */
    as_init(&q, 1);
    q.c[0].chance[0] = 0;
    {
        uint64_t m = run(&q, 0, 64, 0);
        check("CHANCE 0 on step 1: never plays, the others always",
              !(m & 0x0001000100010001ull) && (m & 0x0010001000100010ull) == 0x0010001000100010ull);
    }
    /* 7. the die: at once, kept, cleared */
    as_init(&q, 7);
    q.c[0].next.dose = 127;
    run(&q, 0, 3, 0);
    as_roll(&q, 0);
    run(&q, 0, 13, e);                                  /* steps 4..16: the draw plays already */
    {
        int moved = 0;
        for (s = 0; s < 13; s++)
            if (e[s].on && (e[s].note != 36 || e[s].vel != 96))
                moved = 1;
        check("DICE: the draw plays at once (same cycle)", moved);
    }
    {
        as_event_t a[16], b[16];
        int same = 1;
        run(&q, 0, 16, a);
        run(&q, 0, 16, b);
        for (s = 0; s < 16; s++)                       /* (skipping steps differ: compare where both play) */
            if (a[s].on && b[s].on)
                same &= a[s].note == b[s].note && a[s].vel == b[s].vel;
        check("DICE: the draw is kept, cycle after cycle", same);
    }
    as_clear(&q, 0);
    run(&q, 0, 16, e);
    {
        int clean = 1;
        for (s = 0; s < 16; s++)
            if (e[s].on)
                clean &= e[s].note == 36 && e[s].vel == 96 && !e[s].glide && !e[s].timb[0];
        check("DICE held: the clean pattern again", clean);
    }
    /* 8. a roll on one circle leaves the others alone */
    as_init(&q, 3);
    q.c[0].next.dose = q.c[1].next.dose = 127;
    as_roll(&q, 1);
    run(&q, 0, 16, e);
    {
        int clean = 1;
        for (s = 0; s < 16; s++)
            if (e[s].on)
                clean &= e[s].note == 36 && e[s].vel == 96;
        check("DICE circle 2: circle 1 unchanged", clean);
    }
    /* 9. low DOSE stays near the pattern: notes mostly on the root, forces mostly as set */
    as_init(&q, 11);
    q.c[0].next.dose = 10;
    q.c[0].next.hits = 16;
    run(&q, 0, 16, 0);
    as_roll(&q, 0);
    run(&q, 0, 16, e);
    {
        uint32_t root = 0, n = 0;
        for (s = 0; s < 16; s++)
            if (e[s].on)
                n++, root += e[s].note == 36;
        check("DOSE 10: most notes on the root", n >= 12 && root >= n * 3 / 4);
    }
    /* 10. locks */
    as_init(&q, 5);
    q.c[0].locks = AS_LK_PITCH | AS_LK_FORCE | AS_LK_RHYTHM;
    q.c[0].next.dose = 127;
    q.c[0].next.hits = 16;
    run(&q, 0, 16, 0);
    as_roll(&q, 0);
    run(&q, 0, 16, e);
    {
        int ok = 1;
        for (s = 0; s < 16; s++)
            ok &= e[s].note == 36 && e[s].force == AS_NORMAL;
        check("locks PITCH + FORCE + RHYTHM: 16 hits, root, normal force", ok);
    }
    /* 11. SHIFT turns the pattern at once and keeps it */
    as_init(&q, 1);
    as_turn(&q, 0, 1);
    check("SHIFT +1: 4/16 turned by 1 at once", pat16(run(&q, 0, 16, 0)) == as_euclid(16, 4, 1));
    check("SHIFT +1: kept next cycle", pat16(run(&q, 0, 16, 0)) == as_euclid(16, 4, 1));
    /* 12. polyrhythm: LEN 5 against 16 */
    as_init(&q, 1);
    {
        as_cfg_t c = q.c[2].next;
        uint64_t m;
        c.len = 5, c.hits = 2, c.rot = 0;
        as_set(&q, 2, c);
        run(&q, 2, 16, 0);                              /* the bar it was asked in */
        for (i = 0; i < 16; i++)                        /* (wait for circle 3's cycle start) */
            if (q.c[2].pos == 0)
                break;
            else
                run(&q, 2, 1, 0);
        m = run(&q, 2, 20, 0);
        check("LEN 5 HITS 2: repeats every 5 steps", (m & 31) == as_euclid(5, 2, 0) && (m >> 5 & 31) == (m & 31) &&
                                                         (m >> 15 & 31) == (m & 31));
    }
    /* 13. the default die is drastic: over a few rolls the rhythm moves, notes leave the root, lengths vary */
    {
        uint32_t moved = 0, off_root = 0, hits = 0, long_ = 0, burst = 0, r;
        as_init(&q, 21);
        for (r = 0; r < 8; r++) {
            uint32_t m;
            as_roll(&q, 0);
            m = pat16(run(&q, 0, 16, e));
            moved += q.c[0].pat != as_euclid(16, 4, 0);
            for (s = 0; s < 16; s++)
                if (e[s].on) {
                    hits++;
                    off_root += e[s].note != 36;
                    long_ += e[s].gate > 1;
                    burst += e[s].rat || e[s].flam;
                }
            (void)m;
        }
        printf("  8 rolls: rhythm moved %u, hits %u, off the root %u, long %u, bursts %u\n", moved, hits, off_root, long_, burst);
        check("default DICE: the rhythm moves in most rolls", moved >= 5);
        check("default DICE: most notes leave the root", off_root * 2 > hits);
        check("default DICE: some long notes and some bursts", long_ > 0 && burst > 0);
        as_clear(&q, 0);
        check("held: the clean 4/16 again", pat16(run(&q, 0, 16, 0)) == as_euclid(16, 4, 0));
    }
    /* 14. DENSITY: +1 at once and kept, -1 back */
    as_init(&q, 1);
    run(&q, 0, 3, 0);
    as_density(&q, 0, 1);
    {
        uint64_t m = run(&q, 0, 13, 0) << 3;
        check("DENSITY +1: 5/16 from the next step", ((m | (as_euclid(16, 5, 0) & 7)) & 0xFFFF) == as_euclid(16, 5, 0));
        check("DENSITY +1: kept next cycle", pat16(run(&q, 0, 16, 0)) == as_euclid(16, 5, 0));
        as_density(&q, 0, -1);
        check("DENSITY -1: 4/16 again", pat16(run(&q, 0, 16, 0)) == as_euclid(16, 4, 0));
    }
    /* 15. CHORD: a triad of the scale over each hit, at once */
    as_init(&q, 1);
    {
        as_cfg_t c = q.c[0].next;
        as_event_t ev[AS_NCIRC];
        c.chord = AS_CH_TRI;
        as_set(&q, 0, c);                               /* (MIN: C D Eb F G Ab Bb) */
        as_step(&q, ev);
        check("CHORD TRIAD on C2: C2 + Eb2 + G2", ev[0].on && ev[0].note == 36 && ev[0].chord[0] == 39 &&
                                                    ev[0].chord[1] == 43);
        {
            static const struct { uint8_t ch, n, a, b; const char *what; } T[] = {
                {AS_CH_OCT, 36, 48, 0, "CHORD OCT on C2: C2 + C3"},
                {AS_CH_5TH, 36, 43, 48, "CHORD 5TH on C2: C2 + G2 + C3"},
                {AS_CH_7TH, 36, 39, 46, "CHORD 7TH on C2 (MIN): C2 + Eb2 + Bb2"},
                {AS_CH_9TH, 39, 46, 50, "CHORD 9TH on C2 (MIN), rootless: Eb2 + Bb2 + D3"},
            };
            uint32_t t;
            for (t = 0; t < 4; t++) {
                as_init(&q, 1);
                c = q.c[0].next;
                c.chord = T[t].ch;
                as_set(&q, 0, c);
                as_step(&q, ev);
                check(T[t].what, ev[0].on && ev[0].note == T[t].n && ev[0].chord[0] == T[t].a && ev[0].chord[1] == T[t].b);
            }
            q.scale = AS_SC_MPEN;
            as_init(&q, 1);
            q.scale = AS_SC_MPEN;
            c = q.c[0].next;
            c.chord = AS_CH_7TH;
            as_set(&q, 0, c);
            as_step(&q, ev);
            check("CHORD 7TH in MPEN (5 notes): C2 + Eb2 + Bb2", ev[0].note == 36 && ev[0].chord[0] == 39 && ev[0].chord[1] == 46);
            check("CHORD knob order OFF OCT 5TH TRIAD 7TH 9TH", as_ch_step(AS_CH_OFF, 1) == AS_CH_OCT &&
                  as_ch_step(AS_CH_OCT, 1) == AS_CH_5TH && as_ch_step(AS_CH_5TH, 1) == AS_CH_TRI &&
                  as_ch_step(AS_CH_TRI, 1) == AS_CH_7TH && as_ch_step(AS_CH_9TH, 1) == AS_CH_9TH &&
                  as_ch_step(AS_CH_OFF, -1) == AS_CH_OFF);
        }
    }
    /* lanes, TURING: LOCK 127 keeps its pattern, LOCK 0 renews it, the LOCK key freezes it; NEXT / PREV */
    {
        as_seq_t r;
        as_event_t k8[16];
        as_cfg_t c;
        uint32_t a, b2, n, same = 1, x;
        as_init(&r, 7);
        r.c[0].sdly[0] = 100;
        r.c[0].spitch[0] = 2;
        run(&r, 0, 16, 0);
        run(&r, 0, 16, k8);
        check("lanes: step 1 throws DELAY 100, plays 2 degrees up (Eb2 in minor)", k8[0].on && k8[0].dly == 100 &&
              k8[0].note == 39);
        c = r.c[0].next;
        c.type = AS_T_TURING;
        c.tlock = 127;
        c.tdens = 64;
        as_set(&r, 0, c);
        run(&r, 0, 16, 0);
        a = (uint32_t)run(&r, 0, 16, 0);
        b2 = (uint32_t)run(&r, 0, 16, 0);
        check("TURING LOCK 127: the same pattern cycle after cycle", a == b2 && a);
        c.tlock = 0;
        as_set(&r, 0, c);
        run(&r, 0, 16, 0);
        for (n = 0; n < 4u && same; n++)
            same = (uint32_t)run(&r, 0, 16, 0) == a;
        check("TURING LOCK 0: the pattern changes", !same);
        r.c[0].tfreeze = 1;
        a = (uint32_t)run(&r, 0, 16, 0);
        check("TURING, the LOCK key: frozen even at LOCK 0", (uint32_t)run(&r, 0, 16, 0) == a &&
              (uint32_t)run(&r, 0, 16, 0) == a);
        x = r.c[0].treg;
        as_tnext(&r, 0);
        b2 = r.c[0].treg;
        as_tprev(&r, 0);
        check("TURING NEXT: a new pattern; PREV: the one before again", b2 != x && r.c[0].treg == x);
        as_tnext(&r, 0);
        check("TURING NEXT after PREV: forward to the one undone", r.c[0].treg == b2);
    }

    printf(fails ? "%d FAILED\n" : "all ok\n", fails);
    return fails != 0;
}
