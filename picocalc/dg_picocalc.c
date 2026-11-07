/*
 * picocalc-doom — the doomgeneric platform and Doom's I_* system calls
 * (design.md §5.3), in place of upstream's src/doomgeneric_rp2350.c, which
 * is not compiled.
 *
 * M0: upstream's behaviour less HDMI, PS/2 and USB, so the engine links.
 * The PSRAM verify (§11.3), the failure pages (§13.1), the WAD list (D13),
 * the at-exit list and the exit to the shell (§13.3) replace the marked
 * parts in later milestones.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Doom's headers first: doomtype.h declares boolean's false and true as
 * enumerators, which <stdbool.h>'s macros would turn into numbers. */
#include "doomgeneric.h"
#include "doomtype.h"
#include "i_system.h"

#include "pico/stdlib.h"

#include "board_config.h"
#include "ff.h"
#include "frank_doom_log.h"
#include "psram_allocator.h"
#include "psram_init.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern void stdio_fatfs_init(void);

/* The volume Doom's files are on; upstream's name, which its FatFs glue
 * does not use, kept so a link against either sees one definition. */
FATFS fs;

void DG_Init(void)
{
    /* Upstream's PSRAM start-up. M1 adds the round trip (§11.3). */
    psram_init(get_psram_pin());
    psram_set_sram_mode(0);

    /* M3 replaces this with the frame pool (§7.2). */
    DG_ScreenBuffer = psram_malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t));
    if (!DG_ScreenBuffer) {
        panic("DG_Init: no PSRAM for the screen buffer");
    }
    memset(DG_ScreenBuffer, 0, DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t));

    /* M3 names a failure on a page (§13.1). */
    if (f_mount(&fs, "", 1) != FR_OK) {
        panic("DG_Init: the card did not mount");
    }
    f_chdir("/doom");
    stdio_fatfs_init();
}

/* DG_StartScreen() is left to doomgeneric's weak default (no list) until
 * M3 starts the only IWAD and M4 draws the WAD list (§13.1). */

void DG_SleepMs(uint32_t ms)
{
    sleep_ms(ms);
}

uint32_t DG_GetTicksMs(void)
{
    return to_ms_since_boot(get_absolute_time());
}

void DG_SetWindowTitle(const char *title)
{
    (void)title;
}

/* get_fattime() for FatFs, which the patched sdcard.c leaves to the port
 * (SDCARD_EXTERNAL_FATTIME). M8 takes it from the shell's clock (§13.2);
 * until then FatFs's own fallback date, 1 January 2022. */
DWORD get_fattime(void)
{
    return ((DWORD)(FF_NORTC_YEAR - 1980) << 25) |
           ((DWORD)FF_NORTC_MON << 21) |
           ((DWORD)FF_NORTC_MDAY << 16);
}

/* Doom's system calls. */

void I_Error(char *error, ...)
{
    /* M4 shows it on a page and restarts on a key (§13.1). */
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
