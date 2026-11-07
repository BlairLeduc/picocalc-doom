/* input.h — the keyboard on core 0: Doom's DG_GetKey(), and the port's
 * own pages' keys (design.md §9.1, §13.1).
 *
 * input_shim.c takes the MCU's pairs from core 1's ring (kbd.h), runs
 * them through the key map (keymap.h), and hands Doom one key event at a
 * time. The WAD list and the error page read keys the same way, through
 * DG_GetKey(), so a key behaves alike before and in Doom.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_INPUT_H
#define PICOCALC_DOOM_INPUT_H

#include <stdint.h>

/* Kept by core 0, read by the heartbeat (§14.3). Each is one aligned
 * word with one writer. */
typedef struct {
    volatile uint32_t taken;          /* MCU pairs taken from the ring      */
    volatile uint32_t wait_sum_us;    /* their time in it: FIFO read to
                                         DG_GetKey's poll                   */
    volatile uint32_t wait_max_us;
    volatile uint32_t events;         /* key events given to Doom           */
    volatile uint32_t repeats;        /* the MCU's repeats, dropped         */
    volatile uint32_t stray;          /* releases of keys not down          */
    volatile uint32_t unknown;        /* codes the key map does not know    */
    volatile uint32_t held_over;      /* releases after which more waited:
                                         I_GetEvent() takes one a tic       */
    volatile uint32_t frame_max_us;   /* a press's FIFO read to the first
                                         frame published after Doom took it */
    volatile uint32_t frame_sum_us;
    volatile uint32_t frame_n;
} input_stats_t;

extern input_stats_t g_input;

/* Core 0, DG_DrawFrame(): a frame is published now. The first after a
 * press closes its key-to-frame time. */
void input_frame_published(uint32_t now_us);

/* Core 0: the next key pressed, Doom's code, or -1 if none is waiting.
 * Releases and repeats are passed over. For the port's pages. */
int input_next_press(void);

#endif /* PICOCALC_DOOM_INPUT_H */
