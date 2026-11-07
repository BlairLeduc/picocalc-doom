/* audio_stage.h — upstream's mix slots to PWM compare words (design.md
 * §8.1, §8.2): the portable part of the audio shim, tested on the host.
 *
 * Upstream's sound module hands over one slot of its mix ring at a time,
 * 1,421 stereo frames of signed 16-bit samples, left first, through the
 * fill callback it gives i2s_set_fill_callback(). The PWM ring is drained
 * in halves of 512 frames. The stage keeps one slot in a staging buffer
 * and converts from it into a half, calling the callback for the next
 * slot whenever the staging buffer runs out, so a half can span two
 * slots. Each frame becomes one 32-bit compare word, written OVERSAMPLE
 * times: left in channel A's half (GP26), right in B's (GP27)
 * (hardware-notes.md §5.1).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_AUDIO_STAGE_H
#define PICOCALC_DOOM_AUDIO_STAGE_H

#include <stdint.h>

/* Upstream's fill callback (src/pico/audio.h's i2s_fill_cb_t). */
typedef void (*audio_fill_fn)(int buf_index, uint32_t *buf, uint32_t frames);

/* The PWM's period, TOP + 1, for a sample rate at a system clock, the
 * divider 1: clk_hz / (rate_hz * oversample), rounded to the nearest
 * (§8.1: 300 MHz, 49,716 Hz, 2 give 3,017). Never written as a constant. */
uint32_t audio_period(uint32_t clk_hz, uint32_t rate_hz, uint32_t oversample);

/* One signed 16-bit sample to a compare value in 0..period-1: times the
 * gain, scaled to the period about its mid-point, period / 2, and
 * saturated (§8.1). Upstream's mix peaks near 1,500 of 32,768 (M6), so
 * the gain is the shim's, not the mixer's. */
static inline uint32_t audio_level(int16_t s, uint32_t period, uint32_t gain) {
    int32_t v = (int32_t)(period >> 1)
              + (int32_t)(((int64_t)s * (int64_t)(period * gain)) >> 16);
    if (v < 0) v = 0;
    if (v > (int32_t)period - 1) v = (int32_t)period - 1;
    return (uint32_t)v;
}

/* One of upstream's stereo frames, as the mix ring holds it (left in the
 * low half on this little-endian core), to the slice's compare word. */
static inline uint32_t audio_word(uint32_t frame, uint32_t period, uint32_t gain) {
    return audio_level((int16_t)(frame & 0xFFFFu), period, gain)
         | (audio_level((int16_t)(frame >> 16), period, gain) << 16);
}

/* The compare word for silence. */
static inline uint32_t audio_silence(uint32_t period) {
    return (period >> 1) | ((period >> 1) << 16);
}

typedef struct {
    uint32_t *staging;     /* slot_frames frames, the callback's buffer   */
    uint32_t  slot_frames; /* upstream's PICO_SOUND_BUFFER_SAMPLES        */
    uint32_t  pos;         /* frames of the staging buffer already used   */
    uint32_t  period;      /* audio_period()                              */
    uint32_t  oversample;  /* compare words a frame                       */
    uint32_t  gain;        /* audio_level()'s                             */
    int       next_index;  /* the buf_index passed next, 0 or 1           */
    uint32_t  slots;       /* callbacks made, since init                  */
    uint32_t  peak;        /* the largest |sample| converted, either ear,
                              since the caller last cleared it            */
} audio_stage_t;

/* Empty: the first audio_stage_fill() calls the callback at once. */
void audio_stage_init(audio_stage_t *st, uint32_t *staging, uint32_t slot_frames,
                      uint32_t period, uint32_t oversample, uint32_t gain);

/* Fills dst with frames * oversample compare words, calling fill for a
 * new slot each time the staging buffer runs out (and with fill NULL,
 * silence in its place). In SRAM on the device: it runs in the refill
 * interrupt. */
void audio_stage_fill(audio_stage_t *st, uint32_t *dst, uint32_t frames, audio_fill_fn fill);

#endif /* PICOCALC_DOOM_AUDIO_STAGE_H */
