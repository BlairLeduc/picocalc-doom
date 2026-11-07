/* test_audio_stage.c — the audio shim's staging and conversion (design.md
 * §8.1, §8.2, §15).
 *
 * The period against the design's figures; the 16-bit-to-compare-value
 * conversion, with the shim's gain, against an independent model in
 * floating point, for every sample; then the staging: 1,421-frame slots in, 512-frame halves out,
 * each frame written twice, in order across every slot boundary and
 * buf_index alternating, against a model of upstream's mix ring in which
 * some slots underrun (upstream's callback writes zeros), which come out
 * as silence.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio_stage.h"

static int failures;

#define CHECK(cond, ...)                                         \
    do {                                                         \
        if (!(cond)) {                                           \
            failures++;                                          \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                        \
            fputc('\n', stderr);                                 \
        }                                                        \
    } while (0)

#define SLOT   1421u   /* upstream's PICO_SOUND_BUFFER_SAMPLES at 49,716 Hz */
#define HALF   512u
#define OVER   2u
#define PERIOD 3017u   /* §8.1, at 300 MHz */

#define GAIN   8u

/* The model: v = P/2 + floor(s * G * P / 65536), within 0..P-1. */
static uint32_t model_level(int s, uint32_t period, uint32_t gain) {
    double v = floor((double)(period / 2u) +
                     floor((double)s * (double)gain * (double)period / 65536.0));
    if (v < 0.0) v = 0.0;
    if (v > (double)(period - 1u)) v = (double)(period - 1u);
    return (uint32_t)v;
}

static void test_period(void) {
    CHECK(audio_period(300000000u, 49716u, 2u) == 3017u, "300 MHz: %u",
          audio_period(300000000u, 49716u, 2u));
    /* 300e6 / (3,017 * 2) is 49,718.26 Hz (§8.1). */
    double rate = 300e6 / (3017.0 * 2.0);
    CHECK(fabs(rate - 49718.26) < 0.01, "rate %f", rate);
    /* The carrier stays ultrasonic (hardware-notes.md §5.2). */
    CHECK(300e6 / 3017.0 > 73000.0, "carrier");
    CHECK(audio_period(252000000u, 49716u, 2u) == 2534u, "252 MHz: %u",
          audio_period(252000000u, 49716u, 2u));
    CHECK(audio_period(150000000u, 36621u, 2u) == 2048u, "150 MHz, Quest's rate: %u",
          audio_period(150000000u, 36621u, 2u));
}

static void test_level(void) {
    const uint32_t periods[] = { PERIOD, 2534u, 2048u, 4096u };
    const uint32_t gains[] = { 1u, GAIN, 16u };
    for (unsigned p = 0; p < sizeof periods / sizeof periods[0]; p++)
    for (unsigned g = 0; g < sizeof gains / sizeof gains[0]; g++) {
        uint32_t period = periods[p], gain = gains[g];
        unsigned bad = 0;
        for (int s = -32768; s <= 32767; s++) {
            uint32_t got = audio_level((int16_t)s, period, gain);
            uint32_t want = model_level(s, period, gain);
            if (got != want && bad++ < 5)
                CHECK(0, "period %u gain %u, sample %d: %u, model %u", period, gain, s, got, want);
        }
        CHECK(bad == 0, "period %u gain %u: %u samples differ", period, gain, bad);
        CHECK(audio_level(0, period, gain) == period / 2u, "silence at period %u", period);
        CHECK(audio_level(-32768, period, gain) == 0u, "floor at period %u", period);
        CHECK(audio_level(32767, period, gain) == period - 1u || gain == 1u, "ceiling");
        CHECK(audio_level(32767, period, gain) <= period - 1u, "ceiling at period %u", period);
    }
    /* Gain 8 saturates, it does not wrap: past 4,096 the output stays at
     * the rail. */
    CHECK(audio_level(4200, PERIOD, GAIN) == PERIOD - 1u, "saturate up");
    CHECK(audio_level(-4200, PERIOD, GAIN) == 0u, "saturate down");
    CHECK(audio_level(1500, PERIOD, GAIN) == 1508u + 552u, "1500 at gain 8: %u",
          audio_level(1500, PERIOD, GAIN));
    /* The design's formula, ((s * 3,017) >> 16) + 1,508, saturated. */
    CHECK(audio_level(32767, PERIOD, 1u) == 3016u, "full scale up");
    CHECK(audio_level(-32767, PERIOD, 1u) == 0u, "full scale down");
    CHECK(audio_level(1000, PERIOD, 1u) == 1508u + 46u, "1000: %u", audio_level(1000, PERIOD, 1u));
    /* Left in the low half, right in the high. */
    uint32_t frame = (uint32_t)(uint16_t)(int16_t)-20000 | ((uint32_t)(uint16_t)12345 << 16);
    CHECK(audio_word(frame, PERIOD, 1u) ==
              (model_level(-20000, PERIOD, 1u) | (model_level(12345, PERIOD, 1u) << 16)),
          "word %08x", audio_word(frame, PERIOD, 1u));
    CHECK(audio_silence(PERIOD) == (1508u | (1508u << 16)), "silence word");
}

/* A model of upstream's mix ring: slot k's frame i is a known pattern, and
 * the slots listed as underruns are zeros, as upstream's callback writes. */
static uint32_t g_calls;
static int g_last_index = -1;
static int g_index_ok = 1;

static int is_underrun(uint32_t slot) {
    return slot == 0u || slot == 3u || slot == 4u || slot % 11u == 7u;
}

static uint32_t pattern(uint32_t slot, uint32_t i) {
    int16_t l = (int16_t)((slot * 7919u + i * 31u) & 0xFFFFu);
    int16_t r = (int16_t)((slot * 104729u + i * 17u + 3u) & 0xFFFFu);
    return (uint32_t)(uint16_t)l | ((uint32_t)(uint16_t)r << 16);
}

static void fake_fill(int buf_index, uint32_t *buf, uint32_t frames) {
    if (frames != SLOT) g_index_ok = 0;
    if (g_last_index >= 0 && buf_index != (g_last_index ^ 1)) g_index_ok = 0;
    g_last_index = buf_index;
    uint32_t slot = g_calls++;
    for (uint32_t i = 0; i < frames; i++) buf[i] = is_underrun(slot) ? 0u : pattern(slot, i);
}

static void test_staging(void) {
    static uint32_t staging[SLOT];
    static uint32_t half[HALF * OVER];
    audio_stage_t st;
    audio_stage_init(&st, staging, SLOT, PERIOD, OVER, GAIN);

    /* 4,000 halves: about 1,441 slots, every boundary offset. */
    uint64_t frame = 0;
    unsigned bad = 0;
    for (unsigned h = 0; h < 4000u; h++) {
        memset(half, 0xA5, sizeof half);
        audio_stage_fill(&st, half, HALF, fake_fill);
        for (uint32_t i = 0; i < HALF; i++, frame++) {
            uint32_t slot = (uint32_t)(frame / SLOT), at = (uint32_t)(frame % SLOT);
            uint32_t src = is_underrun(slot) ? 0u : pattern(slot, at);
            int16_t l = (int16_t)(src & 0xFFFFu), r = (int16_t)(src >> 16);
            uint32_t want = model_level(l, PERIOD, GAIN) | (model_level(r, PERIOD, GAIN) << 16);
            if ((half[2u * i] != want || half[2u * i + 1u] != want) && bad++ < 5)
                CHECK(0, "half %u frame %u (slot %u +%u): %08x %08x, want %08x", h, i, slot,
                      at, half[2u * i], half[2u * i + 1u], want);
            if (is_underrun(slot) && half[2u * i] != audio_silence(PERIOD) && bad++ < 5)
                CHECK(0, "underrun slot %u not silent", slot);
        }
    }
    CHECK(bad == 0, "%u frames wrong", bad);
    uint32_t want_calls = (uint32_t)((4000ull * HALF + SLOT - 1u) / SLOT);
    CHECK(g_calls == want_calls, "callbacks %u, want %u", g_calls, want_calls);
    CHECK(st.slots == g_calls, "slots %u", st.slots);
    CHECK(g_index_ok, "buf_index did not alternate, or frames was not a slot");
    CHECK(st.peak > 30000u && st.peak <= 32768u, "peak %u", st.peak);

    /* No callback yet: silence, and the stage carries on with one later. */
    audio_stage_init(&st, staging, SLOT, PERIOD, OVER, GAIN);
    audio_stage_fill(&st, half, HALF, NULL);
    unsigned loud = 0;
    for (uint32_t i = 0; i < HALF * OVER; i++) loud += half[i] != audio_silence(PERIOD);
    CHECK(loud == 0, "%u words not silent with no callback", loud);
    CHECK(st.slots == 1u && st.pos == HALF, "no callback: slots %u pos %u", st.slots, st.pos);
    CHECK(st.peak == 0u, "silence: peak %u", st.peak);

    /* Oversample other than 2, and a half larger than a slot. */
    static uint32_t big[3u * 3000u];
    g_calls = 5;   /* a slot with the pattern */
    g_last_index = -1;
    audio_stage_init(&st, staging, SLOT, PERIOD, 3u, 1u);
    audio_stage_fill(&st, big, 3000u, fake_fill);
    bad = 0;
    for (uint32_t i = 0; i < 3000u; i++) {
        uint32_t slot = 5u + i / SLOT;
        uint32_t src = is_underrun(slot) ? 0u : pattern(slot, i % SLOT);
        uint32_t want = model_level((int16_t)(src & 0xFFFFu), PERIOD, 1u) |
                        (model_level((int16_t)(src >> 16), PERIOD, 1u) << 16);
        for (uint32_t k = 0; k < 3u; k++) bad += big[3u * i + k] != want;
    }
    CHECK(bad == 0, "oversample 3: %u words wrong", bad);
    CHECK(st.slots == 3u, "oversample 3: slots %u", st.slots);
}

int main(void) {
    test_period();
    test_level();
    test_staging();
    if (failures) {
        fprintf(stderr, "test_audio_stage: %d failures\n", failures);
        return 1;
    }
    printf("test_audio_stage: passed\n");
    return 0;
}
