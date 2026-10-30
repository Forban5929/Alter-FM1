/* SPDX-License-Identifier: GPL-3.0-only
 * ALTER's sequencer (alter_seq.c) driving part 1 (ALTER) on Felucca's transport: PLAY starts it, the tempo and an
 * external MIDI clock are Felucca's. Included at the top of seq.c, called once per block from its audio-ISR loop
 * (alter_tick, after click_tick). Felucca's own patterns are empty at power-on: ALTER's circles are what plays.
 *
 * Each sixteenth: the step's note on part 1 (the last one released first: a hit is one step long, the ADSR
 * shapes it), its velocity (HITS 96, GHOST 40, ACCENT ~124), a glide when the die drew one (the voice glides
 * only then: GLIDE is set for that note alone), and the die's timbre offsets on MRPH SPRD HARM AMNT. The offsets
 * are undone exactly before the next ones (alt_applied): what the knobs set stays the sound's base. */

#include "alter_seq.c"

#define ALT_PART 0u                              /* the part ALTER plays (part 1) */
static as_seq_t alt_seq;
static uint32_t alt_pos;                         /* samples into the sixteenth */
static uint8_t alt_run, alt_ready, alt_note;     /* transport was running; alt_seq set up; the sounding note (0 = none) */
static int16_t alt_applied[AS_NTIMB];            /* the offsets in the part's knobs now */
static const uint8_t ALT_TIMB_P[AS_NTIMB] = {P_E0, P_E1, P_E2, P_E4};   /* MRPH SPRD HARM AMNT */

static void alter_step(track_t *t)
{
    as_event_t ev;
    uint32_t j;
    as_step(&alt_seq, &ev);
    if (alt_note) {
        trk_note_off(t, alt_note);
        alt_note = 0;
    }
    for (j = 0; j < AS_NTIMB; j++) {             /* back to the base, then this step's offset (none without a hit) */
        int32_t p = ALT_TIMB_P[j], base = t->p[p] - alt_applied[j], v = ev.on ? base + ev.timb[j] : base;
        v = clamp(v, 0, 127);
        t->p[p] = (int16_t)v;
        alt_applied[j] = (int16_t)(v - base);
    }
    if (ev.on) {
        int16_t g = t->p[P_GLIDE];
        t->p[P_GLIDE] = (int16_t)(ev.glide ? (g ? g : 40) : 0);
        trk_note_on(t, ev.note, ev.vel);
        t->p[P_GLIDE] = g;
        alt_note = ev.note;
    }
}

/* n samples on in this block (seq_n: Felucca's, so an external clock moves it too) */
static void alter_tick(uint32_t n)
{
    track_t *t = &trk[ALT_PART];
    uint32_t p = div_samples(2);                 /* a sixteenth */
    if (!alt_ready) {
        as_init(&alt_seq, 0xA17E5u);
        alt_ready = 1;
    }
    if (!song.playing) {
        if (alt_run && alt_note) {
            trk_note_off(t, alt_note);
            alt_note = 0;
        }
        alt_run = 0;
        return;
    }
    if (!alt_run) {                              /* PLAY: every circle from its first step, at once */
        uint32_t i;
        alt_run = 1;
        alt_pos = 0;
        for (i = 0; i < AS_NCIRC; i++)
            alt_seq.c[i].pos = 0;
        alter_step(t);
        return;
    }
    alt_pos += n;
    while (alt_pos >= p) {
        alt_pos -= p;
        alter_step(t);
    }
}
