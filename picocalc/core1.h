/* core1.h — core 1's loop: the southbridge, the panel and the log
 * (design.md §6.1).
 *
 * M1: bring-up of the southbridge and the LCD, the corner-coded test
 * pattern (hardware-notes.md §10, EL §13.3), the log pump and the
 * heartbeat's skeleton (§14.3). The presenter (M3, §7.4), the keyboard's
 * poll (M4, §9.1) and the pages (§13.1) join this loop later.
 *
 * From picocalc-quest's core1.c (D8), cut to what M1 needs.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_CORE1_H
#define PICOCALC_DOOM_CORE1_H

#include <stdbool.h>
#include <stdint.h>

/* What core 1 found at bring-up, for core 0's banner. Written before
 * `ready` is set, and read only after core 0 sees it. */
typedef struct {
    volatile bool ready;
    uint32_t      i2c_hz;        /* rate the I2C peripheral took         */
    int           sb_version;    /* -1 if the southbridge did not answer */
    uint32_t      i2c_us;        /* one register read                    */
    bool          sb_power_off;  /* the southbridge can power off (§13.3) */
    uint32_t      spi_hz;        /* the LCD's, as configured             */
    uint32_t      lcd_us;        /* lcd_init, reset to DISPON            */
    uint32_t      pattern_us;    /* drawing the test pattern             */
} core1_bringup_t;

extern core1_bringup_t g_core1;

/* Kept by core 0, read by the heartbeat (§14.3). Each is one aligned
 * word with one writer. */
extern volatile uint32_t g_core0_sleep_us;   /* in DG_SleepMs, since boot */
extern volatile uint32_t g_core0_frames;     /* DG_DrawFrame calls        */
extern volatile uint32_t g_core0_stack;      /* core0_stack_used(), at the
                                                last frame, sleep or stop */

/* Core 0, before a reboot: core 1 stops between two transactions and
 * stays stopped, so the reboot cannot catch the southbridge mid-way
 * (§6.2, hardware-notes.md §6.1). Returns once it has, or after 100 ms. */
void core1_park(void);

/* Both stacks are painted, so the heartbeat can report how deep each has
 * gone (§12.1). Core 0's is the SDK's default, the 4 KB of SCRATCH_Y
 * above anything placed there; below it, in SCRATCH_X, is core 1's.
 * core0_paint_stack is core 0's first call in main(), and paints below
 * its own frame; core1_paint_stack is core 0's, before launching
 * core 1. */
void core0_paint_stack(void);
void core1_paint_stack(void);

/* Bytes of each stack ever used, and each stack's size. Core 0's used
 * equal to its size means it has run into core 1's. */
uint32_t core0_stack_used(void);
uint32_t core0_stack_size(void);
uint32_t core1_stack_used(void);
uint32_t core1_stack_size(void);

void core1_main(void);

#endif /* PICOCALC_DOOM_CORE1_H */
