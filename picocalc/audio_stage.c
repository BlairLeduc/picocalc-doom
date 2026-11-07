/* audio_stage.c — upstream's mix slots to PWM compare words (design.md
 * §8.1, §8.2).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "audio_stage.h"

/* The refill interrupt's path is in SRAM on the device (§6.2); the SDK's
 * __not_in_flash_func, without the SDK. */
#ifdef PICOCALC_DOOM_HOST
#define AUDIO_IN_RAM(name) name
#else
#define AUDIO_IN_RAM(name) __attribute__((section(".time_critical." #name))) name
#endif

uint32_t audio_period(uint32_t clk_hz, uint32_t rate_hz, uint32_t oversample) {
    uint64_t d = (uint64_t)rate_hz * oversample;
    return (uint32_t)(((uint64_t)clk_hz + d / 2u) / d);
}

void audio_stage_init(audio_stage_t *st, uint32_t *staging, uint32_t slot_frames,
                      uint32_t period, uint32_t oversample, uint32_t gain) {
    st->staging = staging;
    st->slot_frames = slot_frames;
    st->pos = slot_frames;
    st->period = period;
    st->oversample = oversample;
    st->gain = gain;
    st->next_index = 0;
    st->slots = 0;
    st->peak = 0;
}

void AUDIO_IN_RAM(audio_stage_fill)(audio_stage_t *st, uint32_t *dst, uint32_t frames,
                                    audio_fill_fn fill) {
    const uint32_t period = st->period, over = st->oversample, gain = st->gain;
    uint32_t peak = st->peak;
    while (frames) {
        if (st->pos == st->slot_frames) {
            if (fill) {
                fill(st->next_index, st->staging, st->slot_frames);
            } else {
                for (uint32_t i = 0; i < st->slot_frames; i++) st->staging[i] = 0;
            }
            st->next_index ^= 1;
            st->slots++;
            st->pos = 0;
        }
        uint32_t n = st->slot_frames - st->pos;
        if (n > frames) n = frames;
        const uint32_t *src = &st->staging[st->pos];
        for (uint32_t i = 0; i < n; i++) {
            int32_t l = (int16_t)(src[i] & 0xFFFFu), r = (int16_t)(src[i] >> 16);
            uint32_t a = (uint32_t)(l < 0 ? -l : l), b = (uint32_t)(r < 0 ? -r : r);
            if (a > peak) peak = a;
            if (b > peak) peak = b;
        }
        if (over == 2u) {
            for (uint32_t i = 0; i < n; i++) {
                uint32_t w = audio_word(src[i], period, gain);
                dst[0] = w;
                dst[1] = w;
                dst += 2;
            }
        } else {
            for (uint32_t i = 0; i < n; i++) {
                uint32_t w = audio_word(src[i], period, gain);
                for (uint32_t k = 0; k < over; k++) *dst++ = w;
            }
        }
        st->pos += n;
        frames -= n;
    }
    st->peak = peak;
}
