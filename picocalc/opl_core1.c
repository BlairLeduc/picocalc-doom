/* opl_core1.c — the OPL emulator rendered on core 1 (design.md §8.3, M6).
 *
 * No change to upstream: three link wraps.
 *
 *  - I_PicoSoundSetMusicGenerator(): upstream's OPL glue hands its
 *    generator here instead; ours stays installed from i2s_start(), so
 *    every slot upstream mixes passes through it, and calls upstream's
 *    when there is music.
 *  - OPL_writeReg(): each write is queued with its sample offset in the
 *    slot being mixed, for core 1.
 *  - OPL_calc_buffer_stereo(): renders nothing on core 0; it writes
 *    silence, so upstream's slot holds the effects alone, and counts the
 *    samples, which gives the next write its offset.
 *
 * A slot is complete when upstream has mixed its effects too: when our
 * generator is called for the next slot, or when I_UpdateSound() returns.
 * Then its writes and a marker (the slot's address, its frames, whether
 * it has music) are published to core 1 through a single-producer queue.
 * Core 1 replays the writes into EMU8950 at their offsets, renders the
 * music in chunks between them, adds it into the slot, saturated, and
 * counts the slot done. The refill interrupt (audio_shim.c) takes a slot
 * from upstream's ring only once it is done, so neither core touches a
 * slot the other is working on: core 0 mixes a slot until it is
 * published, core 1 until it is done, the interrupt reads it after.
 *
 * Core 1 reads no engine memory (§6.2): only the sound module's slot,
 * published like a frame, and EMU8950's state, which core 0 creates at
 * init and never touches again.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdbool.h>
#include <stdint.h>

#include "hardware/sync.h"
#include "hardware/timer.h"

#include "emu8950.h"
#include "i_picosound.h"

#include "opl_core1.h"

/* Words in the queue: a write is one, a marker two. Writes leave room for
 * the markers of every slot upstream's ring can hold, so a marker always
 * fits; a write that does not is dropped and counted. */
#define QUEUE_WORDS   1024u
#define MARKER_ROOM   32u
#define MARKER        0x80000000u
#define MARKER_MUSIC  0x40000000u
#define CHUNK         256u

opl_core1_stats_t g_opl_core1;

#if !PICOCALC_DOOM_OPL_CORE1

/* Off: upstream renders the music in its mixer on core 0. */
void opl_core1_start(void) {}
void opl_core1_commit(void) {}
void opl_core1_service(void) {}
bool opl_core1_step(uint32_t deadline) { (void)deadline; return false; }

#else

static uint32_t s_queue[QUEUE_WORDS];
static volatile uint32_t s_published;   /* core 0: words core 1 may read */
static volatile uint32_t s_read;        /* core 1: words it has read     */
static uint32_t s_written;              /* core 0's own write position   */

static OPL *volatile s_opl;

/* Core 0's slot in progress. */
static void (*s_gen)(audio_buffer_t *);
static int16_t *s_slot;
static uint32_t s_slot_frames;
static uint32_t s_pos;
static bool s_slot_music, s_pending;

/* Core 1's chunk of EMU8950's output: packed stereo, both halves equal. */
static int32_t s_chunk[CHUNK];

void __real_I_PicoSoundSetMusicGenerator(void (*generator)(audio_buffer_t *buffer));
void __real_OPL_writeReg(OPL *opl, uint32_t reg, uint8_t val);
void __real_OPL_calc_buffer_stereo(OPL *opl, int32_t *buffer, uint32_t nsamples);

static void put(uint32_t w)
{
    s_queue[s_written % QUEUE_WORDS] = w;
    s_written++;
}

void opl_core1_commit(void)
{
    if (!s_pending) return;
    s_pending = false;
    if (s_written + 2u - s_read > QUEUE_WORDS) {
        /* Cannot happen while the writes leave MARKER_ROOM. */
        g_opl_core1.dropped++;
        return;
    }
    put(MARKER | (s_slot_music ? MARKER_MUSIC : 0u) | s_slot_frames);
    put((uint32_t)(uintptr_t)s_slot);
    __dmb();
    s_published = s_written;
    g_opl_core1.committed++;
    s_pos = 0;
}

static void music_generator(audio_buffer_t *buffer)
{
    opl_core1_commit();
    s_slot = (int16_t *)buffer->buffer->bytes;
    s_slot_frames = buffer->max_sample_count;
    s_pos = 0;
    void (*gen)(audio_buffer_t *) = s_gen;
    s_slot_music = gen != NULL;
    if (gen) {
        gen(buffer);
    } else {
        uint32_t *p = (uint32_t *)buffer->buffer->bytes;
        for (uint32_t i = 0; i < s_slot_frames; i++) p[i] = 0;
        buffer->sample_count = buffer->max_sample_count;
    }
    s_pending = true;
}

void opl_core1_start(void)
{
    __real_I_PicoSoundSetMusicGenerator(music_generator);
}

void __wrap_I_PicoSoundSetMusicGenerator(void (*generator)(audio_buffer_t *buffer))
{
    s_gen = generator;
}

void __wrap_OPL_writeReg(OPL *opl, uint32_t reg, uint8_t val)
{
    s_opl = opl;
    if (s_written + 1u + MARKER_ROOM - s_read > QUEUE_WORDS) {
        g_opl_core1.dropped++;
        return;
    }
    uint32_t off = s_pos > 2047u ? 2047u : s_pos;
    put((off << 20) | ((reg & 0x1FFu) << 8) | val);
}

void __wrap_OPL_calc_buffer_stereo(OPL *opl, int32_t *buffer, uint32_t nsamples)
{
    s_opl = opl;
    for (uint32_t i = 0; i < nsamples; i++) buffer[i] = 0;
    s_pos += nsamples;
}

static inline int16_t clamp16(int32_t v)
{
    return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v;
}

/* Core 1: frames [from, to) of the slot's music, added in. */
static void render(OPL *opl, int16_t *slot, uint32_t from, uint32_t to)
{
    while (from < to) {
        uint32_t n = to - from > CHUNK ? CHUNK : to - from;
        __real_OPL_calc_buffer_stereo(opl, s_chunk, n);
        int16_t *p = &slot[2u * from];
        for (uint32_t i = 0; i < n; i++) {
            int32_t m = (int16_t)(s_chunk[i] & 0xFFFF);
            p[2u * i] = clamp16(p[2u * i] + m);
            p[2u * i + 1u] = clamp16(p[2u * i + 1u] + m);
        }
        from += n;
    }
}

/* Core 1's slot in progress: rendered a few samples at a time, in the
 * time the presenter would otherwise spend waiting for the LCD's DMA. */
#define STEP 8u

static struct {
    bool active, music;
    int16_t *slot;
    uint32_t i, m;          /* the next write's word, and the marker's   */
    uint32_t frames, pos;   /* the slot's frames, and those rendered     */
    uint32_t us, writes;
} s_cur;

static bool begin(void)
{
    uint32_t end = s_published;
    if (s_read == end) return false;
    __dmb();
    /* Every publish ends in a marker: find this slot's. */
    uint32_t m = s_read;
    while (!(s_queue[m % QUEUE_WORDS] & MARKER)) m++;
    uint32_t mark = s_queue[m % QUEUE_WORDS];
    s_cur.slot = (int16_t *)(uintptr_t)s_queue[(m + 1u) % QUEUE_WORDS];
    s_cur.frames = mark & 0xFFFFu;
    s_cur.music = (mark & MARKER_MUSIC) != 0 && s_opl != NULL;
    s_cur.i = s_read;
    s_cur.m = m;
    s_cur.pos = 0;
    s_cur.us = 0;
    s_cur.writes = 0;
    s_cur.active = true;
    return true;
}

static void finish(void)
{
    __dmb();
    s_read = s_cur.m + 2u;
    s_cur.active = false;
    g_opl_core1.writes += s_cur.writes;
    if (s_cur.music) {
        g_opl_core1.music++;
        g_opl_core1.render_us += s_cur.us;
        if (s_cur.us > g_opl_core1.render_max_us) g_opl_core1.render_max_us = s_cur.us;
    }
    g_opl_core1.done++;
}

bool opl_core1_step(uint32_t deadline)
{
    OPL *opl = s_opl;
    for (;;) {
        if (!s_cur.active && !begin()) return false;
        /* The writes due at this sample. */
        uint32_t target = s_cur.frames;
        while (s_cur.i != s_cur.m) {
            uint32_t w = s_queue[s_cur.i % QUEUE_WORDS];
            uint32_t off = w >> 20;
            if (off > s_cur.frames) off = s_cur.frames;
            if (s_cur.music && off > s_cur.pos) {
                target = off;
                break;
            }
            if (opl) __real_OPL_writeReg(opl, (w >> 8) & 0x1FFu, (uint8_t)w);
            s_cur.writes++;
            s_cur.i++;
        }
        if (s_cur.music && s_cur.pos < target) {
            uint32_t n = target - s_cur.pos > STEP ? STEP : target - s_cur.pos;
            uint32_t t0 = time_us_32();
            render(opl, s_cur.slot, s_cur.pos, s_cur.pos + n);
            s_cur.us += time_us_32() - t0;
            s_cur.pos += n;
        } else if (s_cur.i == s_cur.m) {
            finish();
        }
        if ((int32_t)(time_us_32() - deadline) >= 0) return s_cur.active || s_read != s_published;
    }
}

void opl_core1_service(void)
{
    while (opl_core1_step(time_us_32() + 100000u)) {
    }
}

#endif /* PICOCALC_DOOM_OPL_CORE1 */
