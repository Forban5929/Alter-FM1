/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca 1.2: the SEQ TOOLS layer's actions (ui_layer.c: SEQ held, on every page). Included by ui_layer.c. The white
 * keys from F3 (TL_*):
 *   F3 CLEAR  G3 REVERSE  A3 SHIFT <  B3 SHIFT >       the whole sequence
 *   C4 RANDOM D4 COOK     E4 BEAT (DRUM)  F4 EUCLID      new / a little changed (EUCLID: 1.5.5, below)
 *   G4 CLEAR  A4 REVERSE  B4 FILL  C5 RANDOM             the selected lane (DRUM: ui.lane, black keys 1..8 pick it)
 *   D5 DOUBLE E5 HALF                                    LEN x2 / half (1.5.5, below)
 * Each works on the selected track's steps 1..LEN (the steps past LEN stay) and is one undo: SAVE held swaps back
 * the copy taken just before it (ui.c undo, UNDO_PAT; unlike a load the track's automation stays), so after a few
 * COOKs SAVE held takes back the last one only; the layer's OCT- puts back the track as the layer opened (tl_open).
 * A step that held something and is empty after loses its locks (as EDIT's step clear); REVERSE and SHIFT move the
 * steps' automation and locks with them. RANDOM and COOK draw from libc.c rng(): no seed is kept, each press
 * differs. A sequence of ties reversed keeps its note first: N T T -> N T T at the mirrored place */
enum { TL_CLEAR, TL_REV, TL_LEFT, TL_RIGHT, TL_RANDOM, TL_COOK, TL_BEAT, TL_EUCLID, TL_LCLR, TL_LREV, TL_LFILL, TL_LRND,
       TL_DOUBLE, TL_HALF, TL_N };

/* the track as the layer opened (OCT-): in the pool (RAM's .bss is tight), taken by layer_opened */
static struct { step_t step[NSTEP]; motion_store_t mo; } tl_open __attribute__((section(".pool")));

/* place p's action is there for this track (the lane row and BEAT: DRUM only) */
static int tl_cell(const track_t *t, uint32_t p)
{
    return p < TL_N && (p < TL_BEAT || p == TL_EUCLID || p >= TL_DOUBLE || drum_track(t));
}






/* ------------------------------------------------------------- drums --- */





/* ------------------------------------------------------------ EUCLID --- */
/* 1.5.5 (#186): F4 EUCLID: HITS spread as evenly as they go over a cycle of STEPS, turned by ROT, repeated through
 * 1..LEN. F4 opens it on the selected track: the undo copy is taken (one undo for all of it) and the pattern is written
 * at once (heard while playing); KNOB 1..3 HITS (0..STEPS) STEPS (1..LEN) ROT (0..STEPS - 1) rewrite it from that copy.
 * OCT+ (or F4 again, another tool, the layer let go) applies it, OCT- cancels it (the steps as they were; the layer's
 * own OCT- after that puts back the track as the layer opened). DRUM: the selected lane's hits (black keys 1..8 move it
 * to another lane), the other lanes and the step's own fields stay. Melodic: a hit keeps the step's own note (a NOTE
 * step as it is: its chord, accent, CHANCE, RATCH ..; a TIE: the note it holds), else the root (ROOT, near C4); the
 * other steps are emptied, but a TIE that holds a note on (its note's ties: the note keeps its length); the emptied
 * steps lose their locks when it is applied. The values stay for the next one (until the power goes). The track's steps
 * as it opened are its own copy (eu_from; the undo copy is the next load's, a MIDI Program Change or the editor's on
 * another track meanwhile), and it stays on the track it opened on */
static struct { uint8_t on, hits, steps, rot, trk; } eu;
static step_t eu_from[NSTEP] __attribute__((section(".pool")));   /* the steps as EUCLID opened (the pool: .bss is tight) */

/* ------------------------------------------------------- DOUBLE, HALF --- */
/* 1.5.5 (#85): D5 DOUBLE: steps 1..LEN once more after them (LEN + 1 .. 2 x LEN, at most the 64 steps: from LEN 33 on
 * only the first 64 - LEN steps fit) and LEN x2, the steps whole (notes, ties, hits, accents, CHANCE / conditions, RATCH,
 * NUDGE, VEL) with their automation and locks (a copy each; what was stored past LEN where the copy goes is replaced).
 * A note tied over the loop's end ties into the copy as it tied into step 1. E5 HALF: LEN / 2 (rounded down), the steps
 * of the second half cleared with their automation and locks (a note tied into it ends at the new LEN). Both one undo
 * (SAVE held: the steps, LEN and the automation back). DOUBLE at 64 steps and HALF at 1 do nothing (said why); DOUBLE
 * with too few of the 128 records left for the copies: AUTOMATION FULL, nothing done */
static motion_store_t tl_mo __attribute__((section(".pool")));   /* the track's records as they will be */

/* ------------------------------------------------------------ actions --- */
