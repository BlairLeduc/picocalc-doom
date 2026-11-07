/* framepool.h — the core 0 -> core 1 frame handoff (design.md §7.2).
 *
 * Three 320x240 indexed frames in the PSRAM, each in one of four states.
 * Core 0 fills one (upstream's I_FinishUpdate() copies Doom's frame into
 * DG_ScreenBuffer, which is that buffer) and DG_DrawFrame() publishes it;
 * core 1 takes the newest published one, presents it, and releases it.
 * Core 1 never reads a buffer core 0 may still be writing.
 *
 * The palette travels with the frame: core 0 copies the pending RGB565
 * table into the buffer's record before publishing it, so core 1 never
 * sees a palette half-written.
 *
 * The margins (§7.2): outside a level upstream copies only Doom's 200
 * rows, to frame rows 20-219, and blacks rows 0-19 and 220-239 once, on
 * leaving a level, in whichever buffer is current. The other two would
 * keep a level's rows there, so each buffer carries a flag saying its
 * margins are black, and core 0 blacks them before publishing when it is
 * clear.
 *
 * This file is the state machine and nothing else. It holds no lock:
 * the RP2350 has no compare-and-swap, and masking interrupts is not a
 * lock between cores (hardware-notes.md §9.5), so the port calls every
 * transition under one SIO spinlock (video_shim.c). Keeping the lock out
 * is what lets the transitions be tested on a host (EL §2.4).
 *
 * From picocalc-quest's framepool (D8), less its row stamps (dirty rows
 * are an M9 lever here, §7.4) and with the margins flag added.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_FRAMEPOOL_H
#define PICOCALC_DOOM_FRAMEPOOL_H

#include <stdint.h>

#define FRAMEPOOL_COUNT 3
#define FRAMEPOOL_W     320u
#define FRAMEPOOL_ROWS  240u

/* Upstream's 320x200 screens outside a level are copied to frame rows
 * 20-219 (§3.2); these are the rows above and below. */
#define FRAMEPOOL_MARGIN_ROWS 20u

typedef enum {
    FRAME_FREE,       /* owned by nobody; core 0 may claim it        */
    FRAME_FILLING,    /* core 0 is writing it; core 1 must not look  */
    FRAME_READY,      /* complete, awaiting collection               */
    FRAME_RENDERING,  /* core 1 owns it                              */
} frame_state_t;

typedef struct {
    uint8_t      *pixels[FRAMEPOOL_COUNT];        /* fixed PSRAM slots    */
    uint16_t      palette[FRAMEPOOL_COUNT][256];  /* RGB565, per frame    */
    frame_state_t state[FRAMEPOOL_COUNT];
    uint8_t       margins_black[FRAMEPOOL_COUNT];
    uint32_t      published;   /* frames published                   */
    uint32_t      dropped;     /* superseded before core 1 took them */
} framepool_t;

/* Every buffer free, the counters zero, the palettes black. The pixels
 * are the caller's to clear: cleared to 0, every buffer's margins are
 * black, and the flags say so. */
void framepool_init(framepool_t *p, uint8_t *const pixels[FRAMEPOOL_COUNT]);

/* Core 0: take a free buffer to fill. Returns its index, or -1 if none
 * is free — which three buffers make impossible while each side holds
 * at most one, so -1 is a bug, not back-pressure. */
int  framepool_claim(framepool_t *p);

/* Core 0: filling -> ready. Any older ready buffer is superseded and
 * freed on the spot, unpresented, so there is always a free buffer for
 * the next claim and core 0 never waits for core 1. */
void framepool_publish(framepool_t *p, int i);

/* Core 1: take the ready buffer, if there is one; ready -> rendering.
 * Returns its index or -1. */
int  framepool_take(framepool_t *p);

/* Core 1: rendering -> free. */
void framepool_release(framepool_t *p, int i);

/* Core 0, with buffer i filled and before publishing it: whether its
 * margins must be blacked now. In a level the copy wrote every row, so
 * the margins hold the level's rows: the flag is cleared and 0 returned.
 * Outside one, 1 the first time (and the flag is set, the caller being
 * about to black them), 0 after, until the buffer is filled in a level
 * again. */
int  framepool_margins(framepool_t *p, int i, int in_level);

/* Upstream's 0xRRGGBB to the panel's RGB565: the top bits of each. */
uint16_t frame_rgb565(uint32_t rgb888);

/* n indexed pixels through a palette into RGB565, n a multiple of 4.
 * src is read a word at a time. In SRAM on the device: core 1 runs it
 * for every row presented. */
void frame_expand_row(const uint8_t *src, const uint16_t *pal, uint16_t *dst, unsigned n);

#endif /* PICOCALC_DOOM_FRAMEPOOL_H */
