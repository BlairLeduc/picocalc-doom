/*
 * picocalc-doom — the doomgeneric platform and Doom's I_* system calls
 * (design.md §5.3), in place of upstream's src/doomgeneric_rp2350.c, which
 * is not compiled.
 *
 * Upstream's behaviour less HDMI, PS/2 and USB. main() has started and
 * checked the PSRAM by the time DG_Init() runs (§11.3, M1). The failure
 * pages (§13.1), the WAD list (D13), the at-exit list and the exit to the
 * shell (§13.3) replace the marked parts in later milestones.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Doom's headers first: doomtype.h declares boolean's false and true as
 * enumerators, which <stdbool.h>'s macros would turn into numbers. */
#include "doomgeneric.h"
#include "doomtype.h"
#include "i_system.h"

#include "pico/stdlib.h"

#include "core1.h"
#include "ff.h"
#include "frank_doom_log.h"
#include "psram_allocator.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern void stdio_fatfs_init(void);

/* The volume Doom's files are on; upstream's name, which its FatFs glue
 * does not use, kept so a link against either sees one definition. */
FATFS fs;

/* A failure before Doom starts: logged, and core 0 stops here while core
 * 1 keeps the heartbeat. Not panic(): with the probe attached its
 * breakpoint halts core 0, and the timer with it (hardware-notes.md
 * §2.7). M3 names it on a page (§13.1). */
static void stop(const char *why)
{
    g_core0_stack = core0_stack_used();
    FRANK_DOOM_LOG("picocalc-doom: %s; stopped\n", why);
    (void)why;  /* unused when FRANK_DOOM_QUIET compiles the log out */
    for (;;) {
        tight_loop_contents();
    }
}

/* main()'s last call. Here, not in main.c: a file that includes Doom's
 * headers has printf compiled out in the quiet build (doomtype.h includes
 * upstream's frank_doom_quiet_stdio.h), and main.c's banner is printf. */
void picocalc_doom_run(void)
{
    static char *argv[] = { "doom", NULL };
    doomgeneric_Create(1, argv);
    for (;;) {
        doomgeneric_Tick();
    }
}

void DG_Init(void)
{
    /* The PSRAM was started and checked by main() (§11.3). */
    psram_set_sram_mode(0);

    /* M3 replaces this with the frame pool (§7.2). */
    DG_ScreenBuffer = psram_malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t));
    if (!DG_ScreenBuffer) {
        stop("no PSRAM for the screen buffer");
    }
    memset(DG_ScreenBuffer, 0, DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t));

    if (f_mount(&fs, "", 1) != FR_OK) {
        stop("the card did not mount");
    }
    f_chdir("/doom");
    stdio_fatfs_init();
}

/* DG_StartScreen() is left to doomgeneric's weak default (no list) until
 * M3 starts the only IWAD and M4 draws the WAD list (§13.1). */

/* Core 0's idle time, for the heartbeat's busy share (§14.3). */
void DG_SleepMs(uint32_t ms)
{
    uint32_t t0 = time_us_32();
    sleep_ms(ms);
    g_core0_sleep_us += time_us_32() - t0;
    g_core0_stack = core0_stack_used();
}

uint32_t DG_GetTicksMs(void)
{
    return to_ms_since_boot(get_absolute_time());
}

void DG_SetWindowTitle(const char *title)
{
    (void)title;
}

/* Doom's system calls. */

void I_Error(char *error, ...)
{
    /* M4 shows it on a page and restarts on a key (§13.1). */
    g_core0_stack = core0_stack_used();
    va_list ap;
    va_start(ap, error);
    vprintf(error, ap);
    va_end(ap);
    putchar_raw('\n');
    for (;;) {
        tight_loop_contents();
    }
}

void *I_Realloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (size != 0 && p == NULL) {
        I_Error("I_Realloc: failed on reallocation of %zu bytes", size);
    }
    return p;
}

void I_Quit(void)
{
    /* M8 runs the at-exit list and leaves to the shell or powers off
     * (§13.3). Upstream rebooted; M0 stops, since a reset with a
     * southbridge transaction in flight is fatal to the keyboard once
     * core 1 polls it (§6.2). */
    for (;;) {
        tight_loop_contents();
    }
}

byte *I_ZoneBase(int *size)
{
    /* Upstream's: 3 MB of PSRAM, or 2 MB if that fails (§12.2). */
    *size = 3 * 1024 * 1024;
    void *p = psram_malloc(*size);
    if (!p) {
        *size = 2 * 1024 * 1024;
        p = psram_malloc(*size);
    }
    return p;
}

void I_AtExit(void (*func)(void), boolean run_on_error)
{
    /* M8 keeps these and I_Quit() runs them (§13.3). */
    (void)func;
    (void)run_on_error;
}

void I_PrintBanner(char *msg)
{
    FRANK_DOOM_LOG("%s\n", msg);
    (void)msg;  /* unused when FRANK_DOOM_QUIET compiles the log out */
}

void I_PrintDivider(void)
{
    FRANK_DOOM_LOG("------------------------------------------------\n");
}

void I_PrintStartupBanner(char *gamedescription)
{
    I_PrintDivider();
    FRANK_DOOM_LOG("%s\n", gamedescription);
    (void)gamedescription;
    I_PrintDivider();
}

boolean I_ConsoleStdout(void)
{
    return true;
}

void I_InitGraphics(void);
void I_InitTimer(void);

void I_Init(void)
{
    I_InitTimer();
    I_InitGraphics();
}

void I_InitJoystick(void)
{
}

void I_BindJoystickVariables(void)
{
}

void I_Tactile(int on, int off, int total)
{
    (void)on;
    (void)off;
    (void)total;
}

boolean I_GetMemoryValue(unsigned int offset, void *value, int size)
{
    (void)offset;
    (void)value;
    (void)size;
    return false;
}
