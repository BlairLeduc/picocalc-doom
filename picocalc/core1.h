/* core1.h — core 1's loop: the presenter, the pages, the southbridge
 * and the log (design.md §6.1).
 *
 * Bring-up of the southbridge and the LCD, the corner-coded test pattern
 * in a build that asks for it (hardware-notes.md §10, EL §13.3), the
 * presenter (M3, §7.4), the pages (§13.1), the keyboard's poll (M4,
 * §9.1), the log pump and the heartbeat (§14.3).
 *
 * From picocalc-quest's core1.c (D8).
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
extern volatile uint32_t g_core0_heap;       /* core0_heap_used(), likewise */
/* Doom's gametic, leveltime and gamestate at the last frame (M9): the
 * soak's signs of life. Core 1 reads no engine memory (§6.2), so core 0
 * copies them here. leveltime counts only play: it stops in the menu,
 * while paused and outside a level. */
extern volatile uint32_t g_core0_tic;
extern volatile uint32_t g_core0_leveltime;
extern volatile uint32_t g_core0_gamestate;

/* The C heap's extent, sbrk's break above `end`: newlib never gives it
 * back, so it is the high-water. Core 0 only (sbrk is not shared). And
 * the room it has, to __HeapLimit (§12.1). */
uint32_t core0_heap_used(void);
uint32_t core0_heap_size(void);

/* Core 0, before a reboot: core 1 stops between two transactions and
 * stays stopped, so the reboot cannot catch the southbridge mid-way
 * (§6.2, hardware-notes.md §6.1). Returns once it has, or after 100 ms. */
void core1_park(void);

/* Core 0: ask the southbridge to power off in `seconds` (at least 6),
 * done by core 1 between two transactions. True if it took it; false if
 * it refused or core 1 did not answer within 200 ms (§13.3). */
bool core1_power_off(uint8_t seconds);

/* Every restart the port makes: park core 1, then the watchdog's reboot
 * (§6.2). Both loaders start the program again (picocalc_shell.h). Does
 * not return. */
void picocalc_restart(void) __attribute__((noreturn));

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
