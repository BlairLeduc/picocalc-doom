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
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Doom's headers first (doomtype.h's boolean). */
#include "doomgeneric.h"
#include "doomtype.h"
#include "doomstat.h"
#include "i_video.h"

#include "hardware/sync.h"
#include "hardware/xip_cache.h"
#include "pico/stdlib.h"

#include "core1.h"
#include "framepool.h"
#include "video.h"

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

    /* Core 1 reads through the uncached alias: the frame must be in the
     * PSRAM, not only in the cache (hardware-notes.md §9.3). */
    xip_cache_clean_range((uintptr_t)px - XIP_BASE, FRAME_BYTES);

    uint32_t irq = spin_lock_blocking(s_lock);
    framepool_publish(&s_pool, i);
    spin_unlock(s_lock, irq);
    claim();

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
