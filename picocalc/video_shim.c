/*
 * picocalc-doom — DG_DrawFrame() and the frame pool (design.md §5.3,
 * §7.2, §7.3).
 *
 * Upstream's I_FinishUpdate() copies Doom's frame into DG_ScreenBuffer,
 * then calls DG_DrawFrame(). Here DG_ScreenBuffer is the pool buffer core
 * 0 is filling; DG_DrawFrame() converts the palette if Doom changed it,
 * blacks the buffer's margins if the margins rule says to, cleans the
 * buffer out of the XIP cache (core 1 reads past it), publishes it, and
 * points DG_ScreenBuffer at a newly claimed one. Upstream's
 * I_FinishUpdate() needs no change for this.
 *
 * I_FinishUpdate() is wrapped at link time (--wrap) only to time its
 * copy, which is strategy A's cost (§7.3): the call less DG_DrawFrame.
 *
 * PICOCALC_DOOM_VIDEO_BUF chooses §7.3's strategy. A: Doom draws into
 * upstream's I_VideoBuffer in the zone, in the PSRAM. B: I_InitGraphics()
 * is wrapped, and I_VideoBuffer pointed at a buffer in SRAM after it, each
 * time it runs; the zone block upstream allocated is left as it is, so the
 * zone's layout stays the host engine's and the frame hashes compare.
 *
 * With PICOCALC_DOOM_FRAME_HASHES (M5, §15) each tic's last frame is
 * hashed as the host engine hashes it, after the margins step, and logged
 * as "fh TIC CRC"; scripts/host_engine.sh compare reads them from the
 * capture. It costs core 0 a pass over the frame, so a hash build is never
 * the one timed.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Doom's headers first (doomtype.h's boolean). */
#include "doomgeneric.h"
#include "doomtype.h"
#include "doomstat.h"
#include "i_video.h"
#include "d_loop.h"

#include "hardware/sync.h"
#include "hardware/xip_cache.h"
#include "pico/stdlib.h"

#include "core1.h"
#include "frame_hash.h"
#include "framepool.h"
#include "input.h"
#include "log.h"
#include "video.h"

#include <stdio.h>
#include <string.h>

#define PSRAM_CACHED   0x11000000u
#define PSRAM_UNCACHED 0x15000000u
#define FRAME_BYTES    (FRAMEPOOL_W * FRAMEPOOL_ROWS)

/* The buffers sit at the base of the PSRAM, in the 512 KB scratch region
 * upstream's allocator reserves below its bump allocator, which nothing
 * in upstream's tree uses (§7.2, §12.2). */
_Static_assert(FRAMEPOOL_COUNT * FRAME_BYTES <= 512u * 1024u,
               "the frame pool fits the PSRAM's scratch region");
_Static_assert(DOOMGENERIC_RESX == FRAMEPOOL_W && DOOMGENERIC_RESY == FRAMEPOOL_ROWS,
               "the pool's frames are upstream's");

extern boolean palette_changed;

video_stats_t g_video;

static framepool_t s_pool;
static spin_lock_t *s_lock;
static uint16_t s_pending[256];
static int s_filling = -1;
/* Set by core 0 once the pool exists: core 1 starts long before, and
 * looks for frames from the start. */
static volatile bool s_ready;
/* DG_DrawFrame's own time in the I_FinishUpdate() now running. */
static uint32_t s_draw_us;

#ifndef PICOCALC_DOOM_VIDEO_BUF_B
#define PICOCALC_DOOM_VIDEO_BUF_B 0
#endif

#if PICOCALC_DOOM_VIDEO_BUF_B
/* Strategy B (§7.3): the frame Doom draws, in SRAM. Word-aligned for
 * upstream's memcpy. */
static uint8_t s_video_sram[SCREENWIDTH * SCREENHEIGHT] __attribute__((aligned(4)));

void __real_I_InitGraphics(void);

void __wrap_I_InitGraphics(void) {
    __real_I_InitGraphics();
    memset(s_video_sram, 0, sizeof s_video_sram);
    I_VideoBuffer = s_video_sram;
}
#endif

#ifndef PICOCALC_DOOM_FRAME_HASHES
#define PICOCALC_DOOM_FRAME_HASHES 0
#endif

#if PICOCALC_DOOM_FRAME_HASHES
static frame_hash_tic_t s_hash_held;

#ifndef PICOCALC_DOOM_FRAME_DUMP_TIC
#define PICOCALC_DOOM_FRAME_DUMP_TIC -1
#endif

/* The first frame drawn at PICOCALC_DOOM_FRAME_DUMP_TIC, as "fd ROW HEX"
 * lines, to compare with the host engine's -hostshot when a hash differs.
 * Waits for the log to drain rather than drop a row: the one place core 0
 * waits on the UART, against §6.2, and only in a build made to look at
 * one tic, where the run's timing no longer matters. */
static void dump_frame(const uint8_t *px) {
    static const char hex[] = "0123456789abcdef";
    char line[16 + 2 * FRAMEPOOL_W];
    for (unsigned y = 0; y < FRAMEPOOL_ROWS; y++) {
        int n = snprintf(line, 16, "fd %u ", y);
        for (unsigned x = 0; x < FRAMEPOOL_W; x++) {
            uint8_t v = px[y * FRAMEPOOL_W + x];
            line[n++] = hex[v >> 4];
            line[n++] = hex[v & 15];
        }
        line[n++] = '\n';
        line[n] = 0;
        while (log_free() < (uint32_t)n + 16u) busy_wait_us_32(1000);
        fputs(line, stdout);
    }
}

static void hash_frame(const uint8_t *px) {
    uint8_t rgb[256 * 3];
    for (unsigned c = 0; c < 256; c++) {
        rgb[3 * c + 0] = colors[c].r;
        rgb[3 * c + 1] = colors[c].g;
        rgb[3 * c + 2] = colors[c].b;
    }
    static bool dumped;
    if (gametic == PICOCALC_DOOM_FRAME_DUMP_TIC && !dumped) {
        dumped = true;
        dump_frame(px);
    }
    frame_hash_tic_t done;
    if (frame_hash_note(&s_hash_held, gametic, frame_hash_frame(px, FRAME_BYTES, rgb), &done)) {
        printf("fh %d %08lx\n", done.tic, (unsigned long)done.crc);
    }
}
#endif

void video_hash_flush(void) {
#if PICOCALC_DOOM_FRAME_HASHES
    frame_hash_tic_t done;
    if (frame_hash_flush(&s_hash_held, &done)) {
        printf("fh %d %08lx\n", done.tic, (unsigned long)done.crc);
    }
#endif
}

static void claim(void) {
    uint32_t irq = spin_lock_blocking(s_lock);
    s_filling = framepool_claim(&s_pool);
    spin_unlock(s_lock, irq);
    /* Three buffers and one each make this impossible (framepool.h). */
    hard_assert(s_filling >= 0);
    DG_ScreenBuffer = s_pool.pixels[s_filling];
}

void video_init(void) {
    uint8_t *px[FRAMEPOOL_COUNT];
    for (unsigned i = 0; i < FRAMEPOOL_COUNT; i++) {
        px[i] = (uint8_t *)(uintptr_t)(PSRAM_CACHED + i * FRAME_BYTES);
        memset(px[i], 0, FRAME_BYTES);
    }
    xip_cache_clean_range((uintptr_t)px[0] - XIP_BASE, FRAMEPOOL_COUNT * FRAME_BYTES);
    s_lock = spin_lock_init(spin_lock_claim_unused(true));
    framepool_init(&s_pool, px);
    claim();
    __dmb();
    s_ready = true;
}

void DG_DrawFrame(void) {
    uint32_t t0 = time_us_32();
    int i = s_filling;
    uint8_t *px = s_pool.pixels[i];

    if (palette_changed) {
        for (unsigned c = 0; c < 256; c++) {
            s_pending[c] = frame_rgb565(((uint32_t)colors[c].r << 16) |
                                        ((uint32_t)colors[c].g << 8) | colors[c].b);
        }
        palette_changed = false;
    }
    memcpy(s_pool.palette[i], s_pending, sizeof s_pending);

    /* gamestate is the one upstream's copy just went by. */
    if (framepool_margins(&s_pool, i, gamestate == GS_LEVEL)) {
        const unsigned m = FRAMEPOOL_MARGIN_ROWS * FRAMEPOOL_W;
        memset(px, 0, m);
        memset(px + FRAME_BYTES - m, 0, m);
        g_video.margins++;
    }

#if PICOCALC_DOOM_FRAME_HASHES
    hash_frame(px);
#endif

    /* Core 1 reads through the uncached alias: the frame must be in the
     * PSRAM, not only in the cache (hardware-notes.md §9.3). */
    xip_cache_clean_range((uintptr_t)px - XIP_BASE, FRAME_BYTES);

    uint32_t irq = spin_lock_blocking(s_lock);
    framepool_publish(&s_pool, i);
    spin_unlock(s_lock, irq);
    claim();
    /* A press Doom took before this frame: its key-to-frame time (M4). */
    input_frame_published(time_us_32());

    g_core0_frames++;
    g_core0_stack = core0_stack_used();
    uint32_t us = time_us_32() - t0;
    s_draw_us = us;
    g_video.publish_us += us;
    if (us > g_video.publish_max_us) g_video.publish_max_us = us;
}

void __real_I_FinishUpdate(void);

void __wrap_I_FinishUpdate(void) {
    uint32_t t0 = time_us_32();
    s_draw_us = 0;
    __real_I_FinishUpdate();
    uint32_t copy = time_us_32() - t0 - s_draw_us;
    g_video.finishes++;
    g_video.copy_us += copy;
    if (copy > g_video.copy_max_us) g_video.copy_max_us = copy;
}

/* ---- core 1 ---------------------------------------------------------- */

int video_take(void) {
    if (!s_ready) return -1;
    __dmb();
    uint32_t irq = spin_lock_blocking(s_lock);
    int i = framepool_take(&s_pool);
    spin_unlock(s_lock, irq);
    return i;
}

void video_release(int i) {
    uint32_t irq = spin_lock_blocking(s_lock);
    framepool_release(&s_pool, i);
    spin_unlock(s_lock, irq);
}

const uint8_t *video_pixels(int i) {
    return (const uint8_t *)((uintptr_t)s_pool.pixels[i] - PSRAM_CACHED + PSRAM_UNCACHED);
}

const uint16_t *video_palette(int i) {
    return s_pool.palette[i];
}

void video_counts(uint32_t *published, uint32_t *dropped) {
    *published = *dropped = 0;
    if (!s_ready) return;
    __dmb();
    uint32_t irq = spin_lock_blocking(s_lock);
    *published = s_pool.published;
    *dropped = s_pool.dropped;
    spin_unlock(s_lock, irq);
}
