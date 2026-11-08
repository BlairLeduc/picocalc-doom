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
 * C (M9) is B with the copy into the pool by DMA. I_FinishUpdate()'s wrap
 * then replaces upstream's (which is the copy and the margins, both done
 * here) rather than calling it: it converts the palette, blacks the
 * margins past the cache if they need it, and starts a DMA from the SRAM
 * frame to the pool buffer's uncached alias, which writes past the XIP
 * cache and so needs no clean. The channel's interrupt publishes the
 * buffer and claims the next when the copy is done. Core 0 meanwhile runs
 * the next tic; it waits for the copy before anything can draw into
 * I_VideoBuffer again: after S_UpdateSounds() (the last call before
 * D_Display() in doomgeneric_Tick()), before each step of the screen wipe
 * (whose loop draws and finishes frames inside D_Display()), and at the
 * next I_FinishUpdate(). Nothing in TryRunTics() draws: I_BeginRead() is
 * empty, so there is no disk icon. PICOCALC_DOOM_COPY_CHECK proves that
 * on the board: at each wait it compares the pool's frame with Doom's.
 *
 * With `-framehashes` in Doom's arguments (M10, §15), or in a build with
 * PICOCALC_DOOM_FRAME_HASHES (M5), each tic's last frame is hashed as the
 * host engine hashes it, after the margins step, logged as "fh TIC CRC"
 * and kept in a list in the PSRAM that the counter block points to, so
 * the build that ships, with no UART, can be checked too:
 * scripts/host_engine.sh compare reads the capture, scripts/counters.sh
 * --hashes the list. It costs core 0 a pass over the frame, so a run with
 * hashes is never the one timed; without, one test a frame.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Doom's headers first (doomtype.h's boolean). */
#include "doomgeneric.h"
#include "doomtype.h"
#include "doomstat.h"
#include "i_video.h"
#include "d_loop.h"
#include "s_sound.h"

#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/xip_cache.h"
#include "pico/stdlib.h"

#include "m_argv.h"
#include "psram_allocator.h"

#include "core1.h"
#include "counters.h"
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
#ifndef PICOCALC_DOOM_VIDEO_BUF_C
#define PICOCALC_DOOM_VIDEO_BUF_C 0
#endif
#ifndef PICOCALC_DOOM_COPY_CHECK
#define PICOCALC_DOOM_COPY_CHECK 0
#endif

static inline uint8_t *uncached(uint8_t *px) {
    return (uint8_t *)((uintptr_t)px - PSRAM_CACHED + PSRAM_UNCACHED);
}

#if PICOCALC_DOOM_VIDEO_BUF_B || PICOCALC_DOOM_VIDEO_BUF_C
/* Strategies B and C (§7.3): the frame Doom draws, in SRAM. Word-aligned for
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

static frame_hash_tic_t s_hash_held;
/* Whether frames are hashed: -1 until the first frame decides. */
static int s_hashing = -1;
/* The list, TIC then CRC, in the PSRAM: demo1 is 1,709 tics. */
#define HASH_MAX 8192u
static uint32_t *s_hash_list;

static bool hashing(void) {
    if (s_hashing < 0) {
        s_hashing = PICOCALC_DOOM_FRAME_HASHES || M_CheckParm("-framehashes") > 0;
        if (s_hashing) {
            s_hash_list = psram_malloc(HASH_MAX * 2u * sizeof(uint32_t));
            picocalc_doom_counters.hash_list = (uint32_t)(uintptr_t)s_hash_list;
            (void)printf("frame hashes: on, the list at %p\n", (void *)s_hash_list);
        }
    }
    return s_hashing > 0;
}

static void hash_done(const frame_hash_tic_t *done) {
    (void)printf("fh %d %08lx\n", done->tic, (unsigned long)done->crc);
    uint32_t n = picocalc_doom_counters.hashes;
    if (s_hash_list && n < HASH_MAX) {
        s_hash_list[2u * n] = (uint32_t)done->tic;
        s_hash_list[2u * n + 1u] = done->crc;
        __dmb();
        picocalc_doom_counters.hashes = n + 1u;
    }
}

#if PICOCALC_DOOM_FRAME_HASHES
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
#endif

static void hash_frame(const uint8_t *px) {
    uint8_t rgb[256 * 3];
    for (unsigned c = 0; c < 256; c++) {
        rgb[3 * c + 0] = colors[c].r;
        rgb[3 * c + 1] = colors[c].g;
        rgb[3 * c + 2] = colors[c].b;
    }
#if PICOCALC_DOOM_FRAME_HASHES
    static bool dumped;
    if (gametic == PICOCALC_DOOM_FRAME_DUMP_TIC && !dumped) {
        dumped = true;
        dump_frame(px);
    }
#endif
    frame_hash_tic_t done;
    if (frame_hash_note(&s_hash_held, gametic, frame_hash_frame(px, FRAME_BYTES, rgb), &done)) {
        hash_done(&done);
    }
}

void video_hash_flush(void) {
    frame_hash_tic_t done;
    if (s_hashing > 0 && frame_hash_flush(&s_hash_held, &done)) hash_done(&done);
}

#if PICOCALC_DOOM_VIDEO_BUF_C
static void copy_init(void);
#endif

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
#if PICOCALC_DOOM_VIDEO_BUF_C
    /* Under C nothing reads or writes the pool through the cache again:
     * no lines of it are left there to go stale. */
    xip_cache_invalidate_range((uintptr_t)px[0] - XIP_BASE, FRAMEPOOL_COUNT * FRAME_BYTES);
#endif
    s_lock = spin_lock_init(spin_lock_claim_unused(true));
    framepool_init(&s_pool, px);
    claim();
#if PICOCALC_DOOM_VIDEO_BUF_C
    copy_init();
#endif
    __dmb();
    s_ready = true;
}

/* Buffer i's palette, Doom's current one; and whether its margins must
 * be blacked now (framepool.h). */
static bool prepare(int i) {
    if (palette_changed) {
        for (unsigned c = 0; c < 256; c++) {
            s_pending[c] = frame_rgb565(((uint32_t)colors[c].r << 16) |
                                        ((uint32_t)colors[c].g << 8) | colors[c].b);
        }
        palette_changed = false;
    }
    memcpy(s_pool.palette[i], s_pending, sizeof s_pending);
    /* gamestate is the one the copy goes by. */
    if (!framepool_margins(&s_pool, i, gamestate == GS_LEVEL)) return false;
    g_video.margins++;
    return true;
}

static void black_margins(uint8_t *px) {
    const unsigned m = FRAMEPOOL_MARGIN_ROWS * FRAMEPOOL_W;
    memset(px, 0, m);
    memset(px + FRAME_BYTES - m, 0, m);
}

/* Core 0's per-frame readings for the heartbeat: the stack, the heap,
 * and Doom's clocks and state (core1.h). */
static void note_frame(void) {
    g_core0_tic = (uint32_t)gametic;
    g_core0_leveltime = (uint32_t)leveltime;
    g_core0_gamestate = (uint32_t)gamestate;
    g_core0_frames++;
    g_core0_stack = core0_stack_used();
    g_core0_heap = core0_heap_used();
}

void DG_DrawFrame(void) {
    uint32_t t0 = time_us_32();
    int i = s_filling;
    uint8_t *px = s_pool.pixels[i];

    if (prepare(i)) black_margins(px);

    if (hashing()) hash_frame(px);

    /* Core 1 reads through the uncached alias: the frame must be in the
     * PSRAM, not only in the cache (hardware-notes.md §9.3). */
    xip_cache_clean_range((uintptr_t)px - XIP_BASE, FRAME_BYTES);

    uint32_t irq = spin_lock_blocking(s_lock);
    framepool_publish(&s_pool, i);
    spin_unlock(s_lock, irq);
    claim();
    /* A press Doom took before this frame: its key-to-frame time (M4). */
    input_frame_published(time_us_32());

    note_frame();
    uint32_t us = time_us_32() - t0;
    s_draw_us = us;
    g_video.publish_us += us;
    if (us > g_video.publish_max_us) g_video.publish_max_us = us;
}

#if !PICOCALC_DOOM_VIDEO_BUF_C

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

#else /* strategy C */

/* DMA_IRQ_1 is the audio's (audio_shim.c); core 1's channels are polled. */
#define COPY_IRQ DMA_IRQ_0

static int s_copy_dma = -1;
/* Set by core 0 when it starts a copy, cleared by the interrupt when the
 * copy is done and the buffer published. */
static volatile bool s_copy_busy;
static uint32_t s_copy_t0;
/* The interrupt's time of publishing, for the key-to-frame figure, which
 * logs, so is noted in thread context (input_frame_published). */
static volatile uint32_t s_copy_published_at;
static volatile bool s_copy_note;

#if PICOCALC_DOOM_COPY_CHECK
static const uint8_t *s_check_dst;
static unsigned s_check_bytes;
#endif

static void copy_irq(void) {
    dma_irqn_acknowledge_channel(0, (unsigned)s_copy_dma);
    uint32_t now = time_us_32();
    uint32_t us = now - s_copy_t0;
    g_video.dma_us += us;
    if (us > g_video.dma_max_us) g_video.dma_max_us = us;

    /* Core 0's thread never holds the lock with interrupts on, so this
     * cannot deadlock with it; core 1 holds it for a few instructions. */
    uint32_t irq = spin_lock_blocking(s_lock);
    framepool_publish(&s_pool, s_filling);
    s_filling = framepool_claim(&s_pool);
    spin_unlock(s_lock, irq);
    hard_assert(s_filling >= 0);
    DG_ScreenBuffer = s_pool.pixels[s_filling];

    s_copy_published_at = now;
    s_copy_note = true;
    __dmb();
    s_copy_busy = false;
}

static void copy_init(void) {
    s_copy_dma = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config((unsigned)s_copy_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, true);
    dma_channel_set_config((unsigned)s_copy_dma, &c, false);
    dma_irqn_set_channel_enabled(0, (unsigned)s_copy_dma, true);
    irq_set_exclusive_handler(COPY_IRQ, copy_irq);
    irq_set_enabled(COPY_IRQ, true);
}

/* Core 0, thread context: the copy started by the last I_FinishUpdate()
 * done, so I_VideoBuffer may be drawn into again. */
static void copy_wait(void) {
    if (s_copy_busy) {
        uint32_t t0 = time_us_32();
        while (s_copy_busy) tight_loop_contents();
        uint32_t us = time_us_32() - t0;
        g_video.waits++;
        g_video.wait_us += us;
        if (us > g_video.wait_max_us) g_video.wait_max_us = us;
    }
    if (s_copy_note) {
        s_copy_note = false;
        /* A press Doom took before this frame: its key-to-frame time (M4). */
        input_frame_published(s_copy_published_at);
#if PICOCALC_DOOM_COPY_CHECK
        /* Doom's frame must be as it was when the copy began: nothing
         * drew between the copy's start and this wait. */
        const uint8_t *src = I_VideoBuffer;
        unsigned diff = 0, first = 0;
        for (unsigned k = 0; k < s_check_bytes; k++) {
            if (s_check_dst[k] != src[k]) {
                if (diff++ == 0) first = k;
            }
        }
        g_video.checked++;
        if (diff) {
            g_video.check_failed++;
            printf("copy check: tic %d, %u bytes differ, the first at row %u, column %u\n",
                   gametic, diff, first / FRAMEPOOL_W, first % FRAMEPOOL_W);
        }
#endif
    }
}

void __wrap_I_FinishUpdate(void) {
    copy_wait();
    uint32_t t0 = time_us_32();
    int i = s_filling;
    uint8_t *px = uncached(s_pool.pixels[i]);

    /* Upstream's copy (i_video.c), at its scale of 1 and offset 0: in a
     * level all 240 rows; outside one Doom's 200, to rows 20-219. Its own
     * blacking of the margins on leaving a level is the margins rule's
     * first case, so prepare() covers it. */
    bool level = gamestate == GS_LEVEL;
    unsigned off = level ? 0u : FRAMEPOOL_MARGIN_ROWS * FRAMEPOOL_W;
    unsigned bytes = level ? FRAME_BYTES : (FRAMEPOOL_ROWS - 2u * FRAMEPOOL_MARGIN_ROWS) * FRAMEPOOL_W;
    if (prepare(i)) black_margins(px);

#if PICOCALC_DOOM_COPY_CHECK
    s_check_dst = px + off;
    s_check_bytes = bytes;
#endif
    s_copy_t0 = time_us_32();
    s_copy_busy = true;
    __dmb();
    dma_channel_set_read_addr((unsigned)s_copy_dma, I_VideoBuffer, false);
    dma_channel_set_write_addr((unsigned)s_copy_dma, px + off, false);
    dma_channel_set_trans_count((unsigned)s_copy_dma, bytes / 4u, true);

    if (hashing()) {
        /* A run with hashes waits here and hashes what the copy wrote; it
         * is never the one timed. */
        copy_wait();
        hash_frame(px);
    }

    note_frame();
    uint32_t us = time_us_32() - t0;
    g_video.finishes++;
    g_video.publish_us += us;
    if (us > g_video.publish_max_us) g_video.publish_max_us = us;
}

void __real_S_UpdateSounds(mobj_t *listener);

void __wrap_S_UpdateSounds(mobj_t *listener) {
    __real_S_UpdateSounds(listener);
    copy_wait();
}

int __real_wipe_ScreenWipe(int wipeno, int x, int y, int width, int height, int ticks);

int __wrap_wipe_ScreenWipe(int wipeno, int x, int y, int width, int height, int ticks) {
    copy_wait();
    return __real_wipe_ScreenWipe(wipeno, x, y, width, height, ticks);
}

#endif /* strategy C */

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
