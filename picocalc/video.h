/* video.h — the frame pool between Doom's frames on core 0 and the
 * presenter on core 1 (design.md §7.2, §7.3).
 *
 * Core 0 reaches the pool through doomgeneric's own seam: upstream's
 * I_FinishUpdate() copies Doom's frame into DG_ScreenBuffer, which is the
 * pool buffer core 0 is filling, and calls DG_DrawFrame(), which
 * publishes it with its palette and claims the next (video_shim.c).
 * Core 1 reaches it through the calls below.
 *
 * §7.3's strategy B by default (M5): Doom draws into an SRAM buffer
 * (PICOCALC_DOOM_VIDEO_BUF), and upstream's copy, unchanged, moves it
 * into the pool; A, the zone's buffer in the PSRAM, is the control.
 * Core 1 reads a frame by DMA from the PSRAM's uncached alias into an
 * SRAM row, a row ahead (hardware-notes.md §4.7, §9.3), so core 0 cleans
 * the buffer's lines out of the XIP cache at publish.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_VIDEO_H
#define PICOCALC_DOOM_VIDEO_H

#include <stdbool.h>
#include <stdint.h>

/* Core 0, in DG_Init(), before anything else is put in the PSRAM's
 * scratch region: the three buffers cleared, the pool, and the first
 * buffer claimed as DG_ScreenBuffer. */
void video_init(void);

/* Core 0: the last tic's frame hash out to the log, in a hash build
 * (M5, §15); nothing otherwise. Called when Doom stops. */
void video_hash_flush(void);

/* Core 1: the newest published frame, or -1; then give it back. */
int  video_take(void);
void video_release(int i);

/* The frame's pixels, 320x240 indexed, at the uncached alias core 1
 * reads them by, and the RGB565 palette published with them. Valid
 * between video_take and video_release. */
const uint8_t  *video_pixels(int i);
const uint16_t *video_palette(int i);

/* Frames core 0 has published, and those superseded before core 1 took
 * them, since boot. */
void video_counts(uint32_t *published, uint32_t *dropped);

/* Core 0's costs, for the heartbeat (§14.3). Each word has one writer. */
typedef struct {
    volatile uint32_t finishes;      /* I_FinishUpdate calls               */
    volatile uint32_t copy_us;       /* its copy, summed since boot        */
    volatile uint32_t copy_max_us;   /* the longest copy                   */
    volatile uint32_t publish_us;    /* DG_DrawFrame, summed since boot    */
    volatile uint32_t publish_max_us;
    volatile uint32_t margins;       /* buffers whose margins were blacked */
} video_stats_t;

extern video_stats_t g_video;

#endif /* PICOCALC_DOOM_VIDEO_H */
