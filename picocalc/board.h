/* board.h — clocks, rail and board identification (design.md §11).
 *
 * From picocalc-quest's board.c (D8), less its flash sweep.
 * hardware-notes.md §2.1: the banner reports the physical board
 * separately from the SDK build target.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_BOARD_H
#define PICOCALC_DOOM_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_MHZ 300u   /* D7 */

typedef struct {
    const char *sdk_board;      /* what we were compiled for            */
    char        unique_id[17];  /* physical: the module's flash id      */
    const char *chip;           /* physical: RP2350A or B, by package   */
    uint8_t     chip_version;   /* physical: RP2350 revision            */
    uint32_t    flash_bytes;    /* physical: from the flash's JEDEC id  */
    uint32_t    clk_sys_hz;
    uint32_t    clk_peri_hz;
    uint32_t    qmi_timing;     /* flash XIP's, as the clock left it     */
    uint32_t    psram_timing;   /* PSRAM's, once psram_init has run      */
    unsigned    vreg_mv;        /* the core rail, as set                 */
} board_info_t;

/* 150 MHz, rail to 1.20 V, the flash's QMI divider and RX delay doubled
 * from SRAM, then the PLL to 300 MHz, and clk_peri onto clk_sys (§11.1).
 * Call it first in main(), before any peripheral is brought up: each
 * derives its rate from the clock it finds. It reads the flash's id
 * first, at the bootrom's timing. Returns false if the PLL would not
 * take it. */
bool board_init_clocks(void);

/* The die temperature, hardware-notes.md §8.1: init once, then whole
 * degrees C, uncalibrated, averaged over sixteen conversions. Core 1's. */
void board_temp_init(void);
int  board_temp_c(void);

void board_identify(board_info_t *info);
void board_log_banner(const board_info_t *info);

#endif /* PICOCALC_DOOM_BOARD_H */
