/*
 * picocalc-doom — upstream's I2S API over the PicoCalc's PWM audio
 * (design.md §5.3, §8), in place of upstream's src/pico/audio.c, which is
 * not compiled.
 *
 * Upstream's sound module (i_picosound.c), unchanged, mixes in the game
 * loop into a ring of four slots of 1,421 stereo frames, and hands one
 * over at a time through the fill callback it gives
 * i2s_set_fill_callback() (§8.2). Here that callback is called from the
 * PWM's refill interrupt, which only converts and re-arms (D11): PWM on
 * GP26/27, both channels of one slice, one compare word a stereo frame,
 * each frame written twice; two DMA channels chained to each other
 * ping-pong a two-half ring of 512 frames a half, paced by the slice's
 * wrap, with the hardware read wrap (hardware-notes.md §5.1-5.4). From
 * picocalc-quest's audio_shim.c (D8), less its mixing in the interrupt.
 *
 * The rate is the OPL emulator's own, 49,716 Hz, which upstream's mixer
 * runs at: TOP = clk_sys / (2 x 49,716) - 1, rounded, 3,016 at 300 MHz,
 * so 49,718 Hz and a 99.4 kHz carrier (§8.1).
 *
 * Everything on the refill path is in SRAM (§6.2): this file's interrupt
 * code, audio_stage.c's fill, and upstream's callback, i2s_fill_audio(),
 * whose section is renamed after it is compiled (cmake/sram_sections.cmake)
 * and whose memcpy() and memset() are redirected, in i_picosound.c's
 * object only, to the two below. The memset is how an underrun is
 * counted: upstream's callback memsets the slot when its ring is empty.
 * The build checks the path (cmake/check_sram.cmake).
 *
 * Nothing masks the interrupt: upstream's ring has one writer a side
 * (§8.2).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "hardware/timer.h"

#include "audio.h"
#include "audio_shim.h"
#include "audio_stage.h"
#include "opl_core1.h"

#define AUDIO_PIN_L 26u   /* PWM5 A, hardware-notes.md §5.1 */
#define AUDIO_PIN_R 27u   /* PWM5 B */

#define OVERSAMPLE      2u
#define FRAMES_PER_HALF 512u                          /* 10.3 ms at 49,718 Hz */
#define WORDS_PER_HALF  (FRAMES_PER_HALF * OVERSAMPLE)
#define RING_WORDS      (2u * WORDS_PER_HALF)
#define RING_BYTES      (RING_WORDS * sizeof(uint32_t))
#define RING_BITS       13u
_Static_assert(RING_BYTES == (1u << RING_BITS), "the ring must be 2^RING_BITS bytes");

/* Upstream's slot at 49,716 Hz and 35 tics a second, (49,716 + 34) / 35. */
#define STAGING_FRAMES 1421u

#define AUDIO_IRQ DMA_IRQ_1

/* The output's gain over upstream's mix (§8.1, M6). */
#ifndef PICOCALC_DOOM_AUDIO_GAIN
#define PICOCALC_DOOM_AUDIO_GAIN 8
#endif

#ifndef PICOCALC_DOOM_OPL_CORE1
#define PICOCALC_DOOM_OPL_CORE1 0
#endif

#ifndef PICOCALC_DOOM_AUDIO_STALL
#define PICOCALC_DOOM_AUDIO_STALL 0
#endif

audio_stats_t g_audio;
volatile uint32_t g_audio_peak_ask;

/* Aligned to its own size: channel_config_set_ring wraps the low
 * RING_BITS of the read address (hardware-notes.md §5.3). */
static uint32_t s_ring[RING_WORDS] __attribute__((aligned(RING_BYTES)));
static uint32_t s_staging[STAGING_FRAMES];
static audio_stage_t s_stage;

static volatile i2s_fill_cb_t s_fill;
static int s_ch[2] = { -1, -1 };
static unsigned s_slice;
static uint32_t s_period;
static uint32_t s_slot_frames;
static bool s_ready, s_running;

/* Set by the interrupt around upstream's callback: its memset then means
 * an empty ring, its memcpy a slot played. */
static volatile bool s_in_fill;

/* The redirected memcpy() and memset() of i_picosound.c's object (above).
 * Upstream's game-loop code calls them too, to clear and fill its slots.
 * The file is built with -fno-tree-loop-distribute-patterns, so these
 * loops are not turned back into calls to the library's. */
void *__not_in_flash_func(picocalc_audio_memcpy)(void *dst, const void *src, size_t n)
{
    if (s_in_fill) g_audio.played++;
    if ((((uintptr_t)dst | (uintptr_t)src | n) & 3u) == 0) {
        uint32_t *d = dst;
        const uint32_t *s = src;
        for (size_t i = 0; i < n / 4u; i++) d[i] = s[i];
    } else {
        uint8_t *d = dst;
        const uint8_t *s = src;
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    }
    return dst;
}

void *__not_in_flash_func(picocalc_audio_memset)(void *dst, int c, size_t n)
{
    if (s_in_fill) {
        if (g_audio.played) g_audio.underruns++;
        else g_audio.startup++;
    }
    if ((((uintptr_t)dst | n) & 3u) == 0) {
        uint32_t w = (uint8_t)c * 0x01010101u;
        uint32_t *d = dst;
        for (size_t i = 0; i < n / 4u; i++) d[i] = w;
    } else {
        uint8_t *d = dst;
        for (size_t i = 0; i < n; i++) d[i] = (uint8_t)c;
    }
    return dst;
}

#if PICOCALC_DOOM_OPL_CORE1
/* With the music on core 1 (opl_core1.c), upstream's next slot is taken
 * only once core 1 has finished it; until then the stage plays silence,
 * counted as core 1 late if the slot is handed over and waiting, or as
 * an underrun if upstream has not mixed it. So upstream's callback never
 * finds its ring empty here, and its own underrun path never runs. */
static void __not_in_flash_func(gated_fill)(int buf_index, uint32_t *buf, uint32_t frames)
{
    if (g_audio.played < g_opl_core1.done) {
        s_fill(buf_index, buf, frames);
        return;
    }
    for (uint32_t i = 0; i < frames; i++) buf[i] = 0;
    if (g_opl_core1.committed > g_audio.played) g_audio.music_late++;
    else if (g_audio.played) g_audio.underruns++;
    else g_audio.startup++;
}
#endif

static void __not_in_flash_func(refill)(unsigned which)
{
    int ch = s_ch[which], other = s_ch[which ^ 1u];
    uint32_t *half = &s_ring[which * WORDS_PER_HALF];

    /* As Quest's (hardware-notes.md §5.3): this channel running again
     * already means the other half drained too and chained back before
     * this interrupt ran, so the deadline was missed and the channel is
     * replaying through the wrap; it is left alone until its next
     * completion. Nor is this half written while the other channel's
     * read address is inside it. Upstream's slot is not taken either: its
     * sound is late, not lost. */
    uintptr_t off = (uintptr_t)dma_hw->ch[other].read_addr - (uintptr_t)half;
    if (dma_channel_is_busy(ch) ||
        (dma_channel_is_busy(other) && off < WORDS_PER_HALF * sizeof(uint32_t))) {
        g_audio.late++;
        return;
    }
    if (g_audio.peak_seen != g_audio_peak_ask) {
        g_audio.peak_seen = g_audio_peak_ask;
        s_stage.peak = 0;
    }
    s_in_fill = true;
#if PICOCALC_DOOM_OPL_CORE1
    audio_stage_fill(&s_stage, half, FRAMES_PER_HALF, s_fill ? gated_fill : NULL);
#else
    audio_stage_fill(&s_stage, half, FRAMES_PER_HALF, s_fill);
#endif
    s_in_fill = false;
    g_audio.peak = s_stage.peak;
    /* A trigger carries the read address on from where the last run
     * ended; set it back, and the count with it (hardware-notes.md §5.3). */
    dma_channel_set_read_addr(ch, half, false);
    dma_channel_set_trans_count(ch, WORDS_PER_HALF, false);
}

static void __not_in_flash_func(audio_irq)(void)
{
    uint32_t t0 = time_us_32();
    for (unsigned which = 0; which < 2u; which++) {
        uint32_t bit = 1u << s_ch[which];
        if (dma_hw->ints1 & bit) {
            dma_hw->ints1 = bit;
            refill(which);
            g_audio.halves++;
        }
    }
    uint32_t us = time_us_32() - t0;
    g_audio.irq_us += us;
    if (us > g_audio.irq_max_us) g_audio.irq_max_us = us;
}

uint32_t audio_slot_frames(void)
{
    return s_slot_frames;
}

/* The slice, the ring and the channels; the output starts silent at
 * i2s_start(). Upstream's config names I2S pins and a PIO, which are not
 * used; its rate and slot size are. */
void i2s_init(i2s_config_t *config)
{
    if (s_ready) return;
    if (config->dma_trans_count > STAGING_FRAMES || config->dma_trans_count == 0) {
        printf("audio: a slot of %u frames does not fit the staging buffer's %u; no sound\n",
               (unsigned)config->dma_trans_count, (unsigned)STAGING_FRAMES);
        return;
    }
    s_slot_frames = config->dma_trans_count;
    s_period = audio_period(clock_get_hz(clk_sys), config->sample_freq, OVERSAMPLE);
    audio_stage_init(&s_stage, s_staging, s_slot_frames, s_period, OVERSAMPLE,
                     PICOCALC_DOOM_AUDIO_GAIN);

    gpio_set_function(AUDIO_PIN_L, GPIO_FUNC_PWM);
    gpio_set_function(AUDIO_PIN_R, GPIO_FUNC_PWM);
    /* Both pins are one slice; ask, do not hard-code
     * (hardware-notes.md §5.1). */
    s_slice = pwm_gpio_to_slice_num(AUDIO_PIN_L);

    pwm_config pc = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&pc, 1);
    pwm_config_set_wrap(&pc, s_period - 1u);
    pwm_init(s_slice, &pc, false);
    pwm_set_both_levels(s_slice, s_period >> 1, s_period >> 1);

    uint32_t silence = audio_silence(s_period);
    for (unsigned i = 0; i < RING_WORDS; i++) s_ring[i] = silence;

    s_ch[0] = dma_claim_unused_channel(true);
    s_ch[1] = dma_claim_unused_channel(true);
    for (unsigned which = 0; which < 2u; which++) {
        dma_channel_config c = dma_channel_get_default_config(s_ch[which]);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        channel_config_set_dreq(&c, pwm_get_dreq(s_slice));
        channel_config_set_ring(&c, false, RING_BITS);
        channel_config_set_chain_to(&c, s_ch[which ^ 1u]);
        dma_channel_configure(s_ch[which], &c, &pwm_hw->slice[s_slice].cc,
                              &s_ring[which * WORDS_PER_HALF], WORDS_PER_HALF, false);
        dma_channel_set_irq1_enabled(s_ch[which], true);
    }

    /* Above the default 0x80, so a refill preempts everything else on
     * core 0 (hardware-notes.md §5.4). Core 0 calls this, from
     * I_InitSound(), so the interrupt is core 0's. */
    irq_set_exclusive_handler(AUDIO_IRQ, audio_irq);
    irq_set_priority(AUDIO_IRQ, 0x40);
    s_ready = true;

    printf("audio: PWM GP%u/GP%u slice %u, TOP %lu, carrier %lu Hz, %lu.%02lu Hz, "
           "gain %u, halves of %u frames, slots of %lu frames, DMA %d and %d, IRQ %d\n",
           AUDIO_PIN_L, AUDIO_PIN_R, s_slice, (unsigned long)(s_period - 1u),
           (unsigned long)(clock_get_hz(clk_sys) / s_period),
           (unsigned long)(clock_get_hz(clk_sys) / (s_period * OVERSAMPLE)),
           (unsigned long)((clock_get_hz(clk_sys) % (s_period * OVERSAMPLE)) * 100u /
                           (s_period * OVERSAMPLE)),
           (unsigned)PICOCALC_DOOM_AUDIO_GAIN, FRAMES_PER_HALF, (unsigned long)s_slot_frames, s_ch[0], s_ch[1], AUDIO_IRQ);
}

/* I_ShutdownSound(), on the way out: the channels stopped, the output
 * left at the mid-point, silent. */
void i2s_deinit(i2s_config_t *config)
{
    (void)config;
    if (!s_ready) return;
    irq_set_enabled(AUDIO_IRQ, false);
    for (unsigned which = 0; which < 2u; which++) {
        dma_channel_set_irq1_enabled(s_ch[which], false);
        /* Disabled first, so neither chains to the other while aborting. */
        hw_clear_bits(&dma_hw->ch[s_ch[which]].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    }
    dma_channel_abort(s_ch[0]);
    dma_channel_abort(s_ch[1]);
    dma_hw->ints1 = (1u << s_ch[0]) | (1u << s_ch[1]);
    pwm_set_both_levels(s_slice, s_period >> 1, s_period >> 1);
    dma_channel_unclaim(s_ch[0]);
    dma_channel_unclaim(s_ch[1]);
    s_fill = NULL;
    s_running = s_ready = false;
}

void i2s_set_fill_callback(i2s_fill_cb_t cb)
{
    s_fill = cb;
}

/* The output starts with the ring silent; the first refill, a half
 * later, takes upstream's first slot. */
void i2s_start(void)
{
    if (!s_ready || s_running) return;
#if PICOCALC_DOOM_OPL_CORE1
    opl_core1_start();
#endif
    irq_set_enabled(AUDIO_IRQ, true);
    pwm_set_enabled(s_slice, true);
    dma_channel_start(s_ch[0]);
    s_running = true;
}

/* Mixing's share (§8.3): upstream's game loop mixes ahead in
 * I_UpdateSound(), the OPL emulator with it. Link-wrapped, not changed;
 * the interrupt's time inside the call is taken out. The scratch build
 * that forces the late path (EL §6.4) masks core 0's interrupts here for
 * PICOCALC_DOOM_AUDIO_STALL ms every five seconds. */
void __real_I_UpdateSound(void);

void __wrap_I_UpdateSound(void)
{
#if PICOCALC_DOOM_AUDIO_STALL
    static uint32_t next_stall;
    uint32_t now = time_us_32();
    if (next_stall == 0) next_stall = now + 5000000u;
    if ((int32_t)(now - next_stall) >= 0) {
        next_stall = now + 5000000u;
        uint32_t irq = save_and_disable_interrupts();
        busy_wait_us_32(PICOCALC_DOOM_AUDIO_STALL * 1000u);
        restore_interrupts(irq);
    }
#endif
    uint32_t irq0 = g_audio.irq_us;
    uint32_t t0 = time_us_32();
    __real_I_UpdateSound();
#if PICOCALC_DOOM_OPL_CORE1
    opl_core1_commit();
#endif
    uint32_t us = (time_us_32() - t0) - (g_audio.irq_us - irq0);
    g_audio.mix_us += us;
    if (us > g_audio.mix_max_us) g_audio.mix_max_us = us;
    g_audio.updates++;
}

/* The push API, which the sound module does not use with a fill callback
 * set; here for the link, each call counted (§8.2). */

void i2s_dma_write(i2s_config_t *config, const int16_t *samples)
{
    (void)config;
    (void)samples;
    g_audio.pushes++;
}

bool i2s_dma_write_nb(i2s_config_t *config, const int16_t *samples)
{
    (void)config;
    (void)samples;
    g_audio.pushes++;
    return true;
}

bool i2s_is_buffer_free(void)
{
    return true;
}

/* Upstream's sound module asks for no attenuation and never changes it;
 * Doom's own volumes are applied in its mixer. */
void i2s_volume(i2s_config_t *config, uint8_t volume)
{
    (void)config;
    (void)volume;
}

void i2s_increase_volume(i2s_config_t *config)
{
    (void)config;
}

void i2s_decrease_volume(i2s_config_t *config)
{
    (void)config;
}
