/* opl_core1.h — the OPL emulator rendered on core 1 (design.md §8.3, M6).
 *
 * Upstream's music runs in its mixer on core 0: the MUS sequencer writes
 * OPL registers, and EMU8950 renders each slot's music before the effects
 * are added. EMU8950 alone cost about 18 % of core 0 (M6). With
 * PICOCALC_DOOM_OPL_CORE1, core 0 keeps the sequencer and records each
 * register write with its sample offset in the slot, and core 1 renders
 * the music and adds it into the slot once upstream has finished mixing
 * it; the refill interrupt takes a slot only after core 1 is done with it.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_OPL_CORE1_H
#define PICOCALC_DOOM_OPL_CORE1_H

#include <stdbool.h>
#include <stdint.h>

/* Counters, each word with one writer, named beside it. */
typedef struct {
    volatile uint32_t committed;   /* core 0: slots handed to core 1           */
    volatile uint32_t done;        /* core 1: slots it has finished            */
    volatile uint32_t music;       /* core 1: of those, slots it rendered      */
    volatile uint32_t writes;      /* core 1: register writes applied          */
    volatile uint32_t render_us;   /* core 1: time rendering, since boot       */
    volatile uint32_t render_max_us; /* core 1: the longest slot               */
    volatile uint32_t dropped;     /* core 0: writes the queue had no room for */
} opl_core1_stats_t;

extern opl_core1_stats_t g_opl_core1;

/* Core 0, from i2s_start(): our generator in front of upstream's, so
 * every slot upstream mixes is handed over, with music or without. */
void opl_core1_start(void);

/* Core 0, at the end of each I_UpdateSound(): the slot upstream mixed
 * last is complete, and is handed over. */
void opl_core1_commit(void);

/* Core 1, from its loop: renders every slot handed over. Never sleeps. */
void opl_core1_service(void);

/* Core 1: renders a few samples at a time until the deadline (a
 * time_us_32() value) passes or nothing is left; true if work remains.
 * The presenter calls it while each row goes to the LCD (§7.4). */
bool opl_core1_step(uint32_t deadline);

#endif /* PICOCALC_DOOM_OPL_CORE1_H */
