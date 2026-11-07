/*
 * picocalc-doom — entry point (design.md §5.3, §13.1), in place of
 * upstream's src/main.c, which is not compiled.
 *
 * The boot order of §13.1: the clocks (§11.1); the log and stdio
 * (§14.2); the banner; PSRAM init; core 1, which brings up the
 * southbridge and the LCD (and draws the test pattern in a build that
 * asks for it); the PSRAM's round trip (§11.3), with a page if it fails;
 * then Doom, whose DG_Init() checks the card (dg_picocalc.c). From
 * picocalc-quest's main.c (D8).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "boot/picobin.h"
#include "hardware/sync.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "board.h"
#include "board_config.h"
#include "clock.h"
#include "core1.h"
#include "log.h"
#include "page.h"
#include "picocalc_doom_version.h"
#include "picocalc_shell.h"
#include "psram_check.h"
#include "psram_init.h"

#include <stdint.h>
#include <stdio.h>

/* dg_picocalc.c: doomgeneric_Create(), then its tick for ever. No Doom
 * header is included here, so the banner's printf survives the quiet
 * build's (frank_doom_quiet_stdio.h). */
void picocalc_doom_run(void);

/* The shell's clock, read on each side of the move to 300 MHz (§13.2):
 * the timer has its own tick source, and M1 checks the clock change
 * leaves it alone. */
typedef struct {
    bool     set;
    int64_t  utc_ms;
    int      offset_min;
    uint64_t uptime_us;
} shell_reading_t;

static shell_reading_t read_shell_clock(void) {
    shell_reading_t r = { 0 };
    r.uptime_us = time_us_64();
    r.set = picocalc_shell_time(&r.utc_ms, &r.offset_min);
    return r;
}

static void log_shell_clock(const shell_reading_t *before, const shell_reading_t *after) {
    clock_tm_t t;
    if (!before->set || !after->set) {
        clock_fallback(to_ms_since_boot(get_absolute_time()), &t);
        printf("  time         : the shell's clock is %s; the fallback, "
               "%04d-%02d-%02d %02d:%02d:%02d\n",
               before->set != after->set ? "set on one side of the clock change only" : "not set",
               t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        return;
    }
    int off = after->offset_min < 0 ? -after->offset_min : after->offset_min;
    clock_local(before->utc_ms, before->offset_min, &t);
    printf("  time         : the shell's, before the clock change %04d-%02d-%02d %02d:%02d:%02d\n",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    clock_local(after->utc_ms, after->offset_min, &t);
    printf("  time         : the shell's, after it %04d-%02d-%02d %02d:%02d:%02d UTC%c%d:%02d\n",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
           after->offset_min < 0 ? '-' : '+', off / 60, off % 60);
    printf("  time         : across the clock change it moved %lld ms in %llu us of uptime\n",
           (long long)(after->utc_ms - before->utc_ms),
           (unsigned long long)(after->uptime_us - before->uptime_us));
}

static void log_psram(bool ok, const psram_check_t *r) {
    if (ok) {
        printf("  psram        : 8 MB at 0x11000000, CS GP%u; %lu words round trip in %lu us\n",
               get_psram_pin(), (unsigned long)r->words, (unsigned long)r->us);
    } else {
        printf("  psram        : FAILED its check: %lu of %lu words bad, the first at +0x%06lx "
               "(read 0x%08lx, wrote 0x%08lx); not starting Doom\n",
               (unsigned long)r->bad, (unsigned long)r->words, (unsigned long)r->first_bad,
               (unsigned long)r->got, (unsigned long)r->want);
    }
}

/* UF2 Loader's app partition, which the image must fit (§12.3, §17),
 * from the bootrom's resident copy of the partition table. Nothing is
 * read from flash, which core 1 runs from. An image flashed over SWD has
 * written over the loader and its table, and finds none. */
static void log_partition(void) {
    extern char __flash_binary_end;
    uint32_t image = (uint32_t)(uintptr_t)&__flash_binary_end - XIP_BASE;
    /* The flags word the ROM filled, then the partition's location and its
     * flags: three words. Two gave BOOTROM_ERROR_BUFFER_TOO_SMALL (-13),
     * which picocalc-quest's copy of this read as no table (found in M1). */
    uint32_t info[3], size = 0;
    int n = rom_get_partition_table_info(info, 3,
        PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION | (0u << 24));
    if (n == 3 && (info[0] & PT_INFO_PARTITION_LOCATION_AND_FLAGS)) {
        uint32_t first = (info[1] & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS)
                         >> PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
        uint32_t last = (info[1] & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS)
                        >> PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
        size = (last + 1u - first) * 4096u;
    }
    if (size == 0) {
        printf("  flash        : image %lu KB; no partition table (%d)\n",
               (unsigned long)(image / 1024u), n);
    } else {
        printf("  flash        : image %lu KB; app partition %lu KB, %lu KB spare\n",
               (unsigned long)(image / 1024u), (unsigned long)(size / 1024u),
               (unsigned long)(size > image ? (size - image) / 1024u : 0u));
    }
}

int main(void) {
    core0_paint_stack();

    shell_reading_t before = read_shell_clock();
    bool clocks_ok = board_init_clocks();
    shell_reading_t after = read_shell_clock();

    /* The UART's divider is worked out from the clock that stays. */
    log_init();
    printf("\npicocalc-doom %s (FRANK DOOM %s)\n", PICOCALC_DOOM_VERSION, FRANK_DOOM_VERSION);
    if (!clocks_ok) {
        printf("  WARNING: clk_sys did not reach %u MHz; the SPI rates will not be the "
               "ones this build assumes\n", BOARD_MHZ);
    }
    log_shell_clock(&before, &after);

    /* Upstream's PSRAM init, at the clock that stays (§11.3). Before
     * core 1: it takes the QMI out of its memory-mapped mode for a
     * moment, and core 1 runs from flash. */
    psram_init(get_psram_pin());
    board_info_t info;
    board_identify(&info);
    board_log_banner(&info);

    /* Core 1 brings up the southbridge and the panel; from here on, core
     * 1 drains the log. */
    log_start_ring();
    core1_paint_stack();
    multicore_launch_core1(core1_main);
    while (!g_core1.ready) tight_loop_contents();
    __dmb();
    printf("  southbridge  : i2c %lu Hz, version %d, a register read %lu us%s%s\n",
           (unsigned long)g_core1.i2c_hz, g_core1.sb_version, (unsigned long)g_core1.i2c_us,
           g_core1.sb_power_off ? ", can power off" : ", cannot power off",
           g_core1.sb_version < 0 ? "; NOT ANSWERING: switch the PicoCalc off and on" : "");
    printf("  loader       : %s\n", picocalc_shell_loader_present()
           ? "started by UF2 Loader" : "installed from BOOTSEL, or over SWD");
    log_partition();
#if PICOCALC_DOOM_TEST_PATTERN
    printf("  lcd          : spi %lu Hz, init %lu us, test pattern %lu us\n",
           (unsigned long)g_core1.spi_hz, (unsigned long)g_core1.lcd_us,
           (unsigned long)g_core1.pattern_us);
#else
    printf("  lcd          : spi %lu Hz, init %lu us\n",
           (unsigned long)g_core1.spi_hz, (unsigned long)g_core1.lcd_us);
#endif

    /* The check, before anything is stored in the PSRAM. */
    psram_check_t check;
    bool psram_ok = psram_check(&check);
    log_psram(psram_ok, &check);
    if (!psram_ok) {
        /* Upstream keeps the zone, the screen and the music there:
         * starting Doom on a bad one would fail somewhere far from the
         * cause. Core 1 shows the page and keeps the heartbeat going. */
        page_show(PAGE_NO_PSRAM);
        for (;;) tight_loop_contents();
    }

    picocalc_doom_run();
}
