/* log.h — stdio into a ring that core 1 drains to UART0 (design.md §14.2;
 * EL §2.3).
 *
 * At 115200 baud the UART takes about 11.5 bytes a millisecond, and
 * Doom prints a great deal at start-up, and upstream logs every large
 * PSRAM allocation. So
 * stdio does not block on the UART: it is a stdio_driver_t that copies
 * into a ring, and core 1, which polls anyway, moves bytes into the UART
 * FIFO as it has room. Until core 1 runs, the driver writes the UART
 * directly, so a boot that dies early still has its banner.
 *
 * Either core may print, and core 0's interrupts as well: writers take a
 * hardware spin lock for the copy, which is short. A write that does not
 * fit is dropped whole and counted.
 *
 * A build without the UART (PICOCALC_DOOM_UART=0, the one that ships)
 * has nowhere to log: log_init registers nothing, and stdio discards.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_LOG_H
#define PICOCALC_DOOM_LOG_H

#include <stdint.h>

/* Core 0, first thing after the clocks: UART0 on GP2/GP3 (D5), and the
 * driver, writing directly. */
void log_init(void);

/* Core 0, just before launching core 1: from here on, writes go to the
 * ring. */
void log_start_ring(void);

/* Core 1. Writes what fits in the UART FIFO now; never waits. */
void log_pump(void);

/* Writes dropped because the ring was full. */
uint32_t log_dropped(void);

#endif /* PICOCALC_DOOM_LOG_H */
