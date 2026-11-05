/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MIDI LEARN (1.5, Discussion #170): a controller's CC set to any knob's parameter, on the device. GLO held + D4
 * (LEARN, ui_layer.c) turns it on and off. On: a knob turned on any page (HOME, the sound's pages, MIXER: the
 * parameter of a track it edits) picks its parameter on the selected track, as it edits it; the next CC that comes
 * (midi_control.c midi_learned: any channel ROUT hears) is set to it, and stays until something else is learned on it.
 * Then turn the next knob. OCT- clears the picked parameter's CC, OCT+ (or GLO + D4 again) is done. The footer's first
 * row says what is picked and its CC (ml_line). The map: midi_control.c (ML_N entries in favorites.factory[14], kept
 * with the settings, saved as each one changes); one CC sets one parameter, a parameter has one CC: learning one again
 * replaces both. The editor reads and edits it (editor_learn.c). Its state: ui.ml (ui.c) */
typedef char ml_row_free[FAV_ENGINES <= 14 ? 1 : -1];   /* (factory[14]: no engine's favourites; favorites.c) */
static uint8_t *const ml_tab = favorites.factory[14];
static volatile uint8_t *const ml_io = ui.ml.io;         /* (midi_control.c ml_arm, ml_heard) */

static uint32_t ml_get(uint32_t i) { return ml_tab[2u * i] | (uint32_t)ml_tab[2u * i + 1u] << 8; }
static uint32_t ml_count(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < ML_N; i++)
        n += (ml_get(i) >> 9) != 0u;
    return n;
}
/* takes away cc's entry and parameter id of track k's (either: ML_N / P_COUNT for none); 1 = one went */
#define ML_NOCC 0x80u                                   /* ml_drop: no CC to match (a learned CC is 0..127) */

