/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* I2S output (ALNK0 -> external codec) and the
 * audio ISR: per half buffer, blocks of CTL samples: mix_block (fx.c: events ->
 * each synth part -> dist -> level / pan -> sends -> buses -> master) -> USB audio (uac_tap) -> the metronome's
 * click (click.c, the DAC only) -> 24-bit stereo. */
/* registers: bsp/boards/fm1/fm1_audio.h */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define DAC_TICKS 544u            /* TIMER4 ticks per I2S frame (24 MHz / 44,117.6 Hz) */
#define CPU_AVG (4096u / HALF_FRAMES)   /* halves the load meter averages: ~93 ms whatever the half */
#define OUT_SHIFT 7               /* Q15 -> 24-bit, -6 dBFS ceiling (M0f ran clean at -18 dBFS) */

static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));

/* diagnostics, kept across resets and UBOOT entry: read with `fm1t memr` */
#define DBG_MAGIC 0x44424731u                       /* "DBG1" */
struct felucca_dbg {
    uint32_t magic, halves, max_us, nested, in_audio, late, timer_irqs, ui_frames;
    uint32_t last_us, cpu_q8, boots;
    uint32_t stage, page, home;           /* where the main loop is (breadcrumbs) */
    uint32_t prev_stage, prev_page, prev_home, prev_rst, prev_frames;   /* as found at boot */
} felucca_dbg __attribute__((section(".noinit")));
static volatile uint32_t audio_halves, audio_max_us;
/* ALTER (version test) : le pic de charge de la seconde passée (1/256) et le nombre de demi-blocs en retard (= trous dans
 * le son), lus par l'écran PLAY (alter_ui.c alt_cpu_line) */
static volatile uint32_t alt_cpu_pk_q8, alt_cpu_late;
static volatile uint32_t alt_t5_pct;              /* ALTER (v0.53): TIMER5 nested in the audio, % of its time, ~1 s */
static uint32_t alt_t5_acc;
static uint32_t alt_cpu_win, alt_cpu_n;
static volatile uint32_t t5_nested_ticks;              /* TIMER4 ticks TIMER5 spent nested in this ISR (main.c) */
static uint32_t audio_cpu_rem;                         /* keep the fractional IIR step: no low-load bias */

#if FELUCCA_UAC
/* bsp/chip/usb.c uac_tap takes the samples as the I2S does (24-bit); here they are still Q15: up for it and back
 * (exact), only while a computer records (out of line: the audio ISR's own loops stay as they were) */
static __attribute__((noinline)) void uac_tap_q15(int32_t *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < 2u * n; i++)
        out[i] *= 256;
    uac_tap(out, n);
    for (i = 0; i < 2u * n; i++)
        out[i] >>= 8;
}
#endif

static void audio_block(int32_t *out, uint32_t n)       /* mix (fx.c), then Q15 -> 24 bit */
{
    uint32_t i;
    mix_block(out, n);
#if FELUCCA_UAC
    if (uac.feed)                                       /* the USB audio input: the same master output */
        uac_tap_q15(out, n);
#endif
    if (fx_usb_fixed)                                   /* USB LEVEL FIXED: USB took the full level, the DAC */
        usb_fixed_dac(out, n);                          /* (speaker, headphones) gets MASTER's (fx.c) */
    if (!scope_mix)                                     /* the HOME scope (SCOPE OUT): the music, before the click */
        for (i = 1; i < n; i += 2u)                     /* (MIX: fx.c mix_block took it) */
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)out[2u * i];
    click_render(out, n);                               /* the metronome (click.c): the DAC only, not USB */
    for (i = 0; i < n; i++) {
        out[2u * i] *= 1 << OUT_SHIFT;
        out[2u * i + 1u] *= 1 << OUT_SHIFT;
    }
}

/* Overload: two halves in a row above 85 % (the render plus TIMER5 nested in it), fade one voice over the
 * next block, and one more each half while it lasts. Keep each part's
 * lowest POLY note and MONO / LEGATO / UNISON lead, as the shared budget does. */
static volatile uint8_t shed_req;
static uint8_t shed_over;                              /* bit k: the half k halves ago was over 85 % */
/* ALTER (v0.53, Romain 11/11: CUT 999 at P39 X12 F8 M8): 95 %, not Felucca's 85 %. ALTER has a voice or two a circle:
 * a voice faded out is heard at once, and the 15 % kept free was killing them at a load the DMA still had room for.
 * The real misses (late halves) are counted apart (the CPU line: L) */
#define SHED_TICKS ((HALF_FRAMES * 1000000u / FS) * 95u / 100u * FM1_TICKS_PER_US)   /* 95 % of a half */
static uint32_t shed_count;

/* end of a half: all = its ticks, TIMER5 nested in it included -> the render alone in us. The deadline sees
 * both; shed after 2 overloaded halves in a row (out of line: keeps the ISR's render loops as they were) */
/* ALTER (v0.62, REFONTE 3, Romain: "l'écran puis les traînes"): the safety net, before a voice is cut or a half is late.
 * Each half's load (the render and TIMER5 in it): above 80 % / 88 % / 93 % two halves in a row, one level up (fx.c alt_gov:
 * 1 the screen slower, 2 the notes fading out end sooner, 3 the MASTER FILTER plain); a level kept 2 s, then one down
 * if the load allows it. Felucca's shed (95 %) stays behind it, the last resort */
#define GOV_HALF ((HALF_FRAMES * 1000000u / FS) * FM1_TICKS_PER_US)   /* a half, TIMER4 ticks */
#define GOV_HOLD (2u * FS / HALF_FRAMES)                                /* 2 s of halves */
static uint8_t gov_over;
static uint16_t gov_hold, gov_tail;
static __attribute__((noinline)) void alt_gov_half(uint32_t all)
{
    uint32_t want = all > GOV_HALF / 100u * 93u ? 3u : all > GOV_HALF / 100u * 88u ? 2u : all > GOV_HALF / 100u * 80u ? 1u : 0u;
    uint32_t g = alt_gov;
    if (want > g) {
        if (++gov_over >= 2u) {
            alt_gov = (uint8_t)(g + 1u);
            alt_gov_ups++;
            gov_over = 0;
            gov_hold = GOV_HOLD;
        }
        return;
    }
    gov_over = 0;
    if (want == g)
        gov_hold = GOV_HOLD;
    else if (gov_hold)
        gov_hold--;
    else {
        alt_gov = (uint8_t)(g - 1u);
        gov_hold = GOV_HOLD;
    }
}
/* level 2: one note already released (its key up, fading out) ends over the next block, the quietest; one every 4 halves */
static __attribute__((noinline)) void alt_gov_tail(void)
{
    uint32_t p, i;
    voice_t *best = 0;
    if (++gov_tail < 4u)
        return;
    gov_tail = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && !v->gate && v->stage != 4u && (!best || v->env < best->env))
                best = v;
        }
    if (best) {
        voice_kill(best);
        alt_gov_tails++;
    }
}

static __attribute__((noinline)) uint32_t shed_check(uint32_t all)
{
    alt_gov_half(all);
    shed_over = (uint8_t)(shed_over << 1 | (all > SHED_TICKS));
    if ((shed_over & 3u) == 3u)
        shed_req = 1;
    return (all - t5_nested_ticks) / FM1_TICKS_PER_US;
}

static void shed_voice(void)
{
    uint32_t p, i;
    voice_t *best = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && !v->gate && v->stage != 4u && (!best || v->env < best->env))
                best = v;
        }
    if (best) {
        voice_kill(best);
        shed_count++;
        return;
    }
    for (p = 0; p < NPART; p++) {
        const track_t *t = &trk[p];
        uint32_t mode = trk_vmode(t), low = mode == V_POLY ? lowest_held(t) : 0u;
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && v->gate && v->stage != 4u &&
                ((mode == V_POLY && i != low) || (mode != V_POLY && i > 0u))) {
                if (!best || v->age < best->age)
                    best = v;
            }
        }
    }
    if (best) {
        voice_kill(best);
        shed_count++;
    }
}

void fm1_alnk0_irq(void)                       /* via isr_alnk0 (bsp/chip/fm1_isr.S) */
{
    uint8_t p;
    uint32_t t0;
    felucca_dbg.in_audio = 1;                   /* first: TIMER5 nests from here on (main.c) */
    p = fm1_audio_pending();
    t0 = fm1_ticks();
    t5_nested_ticks = 0;
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half(), b, us;
        int32_t *o = &abuf[half * HALF_WORDS];
        if (shed_req) {
            shed_req = 0;
            shed_voice();
        }
        else if (alt_gov >= 2u)
            alt_gov_tail();                     /* (v0.62: the safety net's level 2) */
#if FELUCCA_UAC
        uac_render_start();
#endif
        for (b = 0; b < HALF_FRAMES; b += CTL) {
#ifdef FM1_INPUT_LAT
            kb_out_tick = t0 + (HALF_FRAMES + b) * DAC_TICKS;   /* when this block plays (seq.c kb_lat) */
#endif
            audio_block(o + 2u * b, CTL);
        }
        fm1_audio_ack_half();
        audio_halves++;
        us = shed_check(fm1_ticks() - t0);
        if (us > audio_max_us)
            audio_max_us = us;
        {
            uint32_t load = song.cpu_q8 * (CPU_AVG - 1u) + (us * 256u) / (HALF_FRAMES * 1000000u / FS) + audio_cpu_rem;
            song.cpu_q8 = load / CPU_AVG;
            audio_cpu_rem = load % CPU_AVG;
        }
        {                                       /* ALTER : pic par fenêtre d'environ 1 s */
            uint32_t alt_load = (us * 256u) / (HALF_FRAMES * 1000000u / FS);
            if (alt_load > alt_cpu_win)
                alt_cpu_win = alt_load;
            alt_t5_acc += t5_nested_ticks;
            if (++alt_cpu_n >= FS / HALF_FRAMES) {
                alt_t5_pct = alt_t5_acc / (alt_cpu_n * HALF_FRAMES / 100u * (FM1_TICKS_PER_US * 1000000u / FS));
                alt_t5_acc = 0;
                alt_cpu_pk_q8 = alt_cpu_win;
                alt_cpu_win = 0;
                alt_cpu_n = 0;
            }
        }
        if (fm1_audio_free_half() != half) {
            felucca_dbg.late++;                         /* the DMA moved on while we rendered */
            alt_cpu_late++;
        }
        felucca_dbg.halves++;
        felucca_dbg.last_us = us;
        if (us > felucca_dbg.max_us)
            felucca_dbg.max_us = us;
        felucca_dbg.cpu_q8 = song.cpu_q8;
    }
    felucca_dbg.in_audio = 0;
}
extern void isr_alnk0(void);

static void audio_init(void)                   /* codec and ALNK0 bring-up (fm1_audio.h) */
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
}
