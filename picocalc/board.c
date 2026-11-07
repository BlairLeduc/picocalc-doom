/* board.c — clocks, rail and board identification (design.md §11).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "board.h"

#include <stdio.h>
#include <string.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/structs/powman.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/sysinfo.h"
#include "hardware/structs/timer.h"
#include "hardware/structs/watchdog.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"

#define BOARD_MHZ_STOCK 150u

/* Flash XIP runs off clk_sys through the QMI's divider, and the bootrom
 * chose it for 150 MHz. At 300 MHz the flash runs at the divider and RX
 * delay picocalc-quest qualified, 4 and 4, 75 MHz (hardware-notes.md §3,
 * design.md §11.4); with PICOCALC_DOOM_FLASH_DIV 0, at the bootrom's
 * timing scaled by the clock's factor, the RX delay too, which is in half
 * clk_sys cycles. From SRAM, with interrupts off: nothing may fetch from
 * flash while its timing changes. */
static void __no_inline_not_in_flash_func(flash_timing_scale)(unsigned mul) {
    uint32_t irq = save_and_disable_interrupts();
    uint32_t t = qmi_hw->m[0].timing;
    uint32_t div = (t & QMI_M0_TIMING_CLKDIV_BITS) >> QMI_M0_TIMING_CLKDIV_LSB;
    uint32_t rx = (t & QMI_M0_TIMING_RXDELAY_BITS) >> QMI_M0_TIMING_RXDELAY_LSB;
    div *= mul;
    rx *= mul;
#if PICOCALC_DOOM_FLASH_DIV
    div = PICOCALC_DOOM_FLASH_DIV;
    rx = PICOCALC_DOOM_FLASH_RXDELAY;
#endif
    if (div > 255u) div = 255u;
    if (rx > 7u) rx = 7u;
    t &= ~(QMI_M0_TIMING_CLKDIV_BITS | QMI_M0_TIMING_RXDELAY_BITS);
    t |= div << QMI_M0_TIMING_CLKDIV_LSB | rx << QMI_M0_TIMING_RXDELAY_LSB;
    qmi_hw->m[0].timing = t;
    /* The write lands before anything after it is fetched from flash. */
    __dsb();
    __isb();
    restore_interrupts(irq);
}

/* The flash's size from its JEDEC id (§11.2): the third byte is log2 of
 * the capacity on the parts these boards carry. Read before the clock
 * moves: flash_do_cmd takes the flash out of XIP for the command, and
 * going back to XIP puts the bootrom's QMI timing back, undoing
 * flash_timing_scale (found in picocalc-quest's M1). */
static uint32_t s_flash_bytes;

static void board_read_flash_size(void) {
    uint8_t tx[4] = { 0x9f, 0, 0, 0 };
    uint8_t rx[4] = { 0 };
    flash_do_cmd(tx, rx, sizeof tx);
    s_flash_bytes = (rx[3] >= 16u && rx[3] <= 28u) ? 1u << rx[3] : 0u;
}

bool board_init_clocks(void) {
    board_read_flash_size();
    if (clock_get_hz(clk_sys) != BOARD_MHZ * 1000000u) {
        /* Up from 150, at power-on, before anything else is running. The
         * rail goes up first and settles, so it is never the lower of the
         * two while the clock is the higher. It is set even if another
         * firmware left it high: the regulator survives a reset
         * (hardware-notes.md §3). */
        if (clock_get_hz(clk_sys) != BOARD_MHZ_STOCK * 1000000u &&
            !set_sys_clock_khz(BOARD_MHZ_STOCK * 1000u, false)) {
            return false;
        }
        vreg_set_voltage(VREG_VOLTAGE_1_20);
        busy_wait_us_32(1000);
        flash_timing_scale(BOARD_MHZ / BOARD_MHZ_STOCK);
        if (!set_sys_clock_khz(BOARD_MHZ * 1000u, false)) {
            /* The flash stays slower than it need be; the rail stays up.
             * Neither is a fault, and the caller says the clock failed. */
            return false;
        }
    }

    /* set_sys_clock_pll otherwise parks clk_peri on the 48 MHz USB PLL,
     * and clk_peri does not follow clk_sys by default. Every derived
     * clock (the SPI baud rates, the UART's, the audio PWM carrier) is
     * set by its own driver after this returns (hardware-notes.md §3). */
    clock_configure(clk_peri,
                    0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                    clock_get_hz(clk_sys),
                    clock_get_hz(clk_sys));
    return true;
}

/* The die's sensor (hardware-notes.md §8.1). Its ADC input is the last:
 * 4 on the QFN-60 RP2350A, 8 on the QFN-80 RP2350B. Read off the chip,
 * not the board header, so the number is right whatever the build says. */
static unsigned s_temp_input;

static bool board_is_rp2350a(void) {
    return (sysinfo_hw->package_sel & SYSINFO_PACKAGE_SEL_BITS) != 0;
}

void board_temp_init(void) {
    adc_init();   /* touches no GPIO: the audio PWM on GP26/27 is safe */
    adc_set_temp_sensor_enabled(true);
    s_temp_input = board_is_rp2350a() ? 4u : 8u;
}

int board_temp_c(void) {
    /* Written directly, not with adc_select_input, whose parameter check
     * is the board header's channel count. */
    hw_write_masked(&adc_hw->cs, s_temp_input << ADC_CS_AINSEL_LSB, ADC_CS_AINSEL_BITS);
    uint32_t sum = 0;
    for (unsigned i = 0; i < 16u; i++) sum += adc_read();   /* ~32 us */
    /* The datasheet's formula, uncalibrated, over the 3.3 V reference:
     * whole degrees are as much as it can say (§8.1). */
    float v = (float)sum * (3.3f / 4096.0f / 16.0f);
    float t = 27.0f - (v - 0.706f) / 0.001721f;
    return (int)(t + (t >= 0.0f ? 0.5f : -0.5f));
}

void board_identify(board_info_t *info) {
    memset(info, 0, sizeof(*info));
    info->sdk_board = PICO_BOARD;
    info->chip = board_is_rp2350a() ? "RP2350A" : "RP2350B";
    info->chip_version = (uint8_t)((sysinfo_hw->chip_id >> 28) & 0xFu);
    pico_get_unique_board_id_string(info->unique_id, sizeof(info->unique_id));
    info->flash_bytes = s_flash_bytes;
    info->clk_sys_hz  = clock_get_hz(clk_sys);
    info->clk_peri_hz = clock_get_hz(clk_peri);
    info->qmi_timing  = qmi_hw->m[0].timing;
    info->psram_timing = qmi_hw->m[1].timing;
    info->vreg_mv     = 550u + 50u * (unsigned)vreg_get_voltage();
}

static unsigned timing_div(uint32_t t) {
    return (t & QMI_M0_TIMING_CLKDIV_BITS) >> QMI_M0_TIMING_CLKDIV_LSB;
}

/* Why the chip last reset, from POWMAN's record of it and the
 * watchdog's: a restart nobody asked for says what it was (found in
 * picocalc-quest's M8: two in a soak, with no line before them). */
static void log_reset(void) {
    static const struct {
        uint32_t    bit;
        const char *name;
    } causes[] = {
        { POWMAN_CHIP_RESET_HAD_POR_BITS, "power-on" },
        { POWMAN_CHIP_RESET_HAD_BOR_BITS, "brown-out" },
        { POWMAN_CHIP_RESET_HAD_RUN_LOW_BITS, "RUN pin" },
        { POWMAN_CHIP_RESET_HAD_DP_RESET_REQ_BITS, "debugger" },
        { POWMAN_CHIP_RESET_HAD_RESCUE_BITS, "rescue" },
        { POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_POWMAN_ASYNC_BITS, "watchdog (powman async)" },
        { POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_POWMAN_BITS, "watchdog (powman)" },
        { POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_SWCORE_BITS, "watchdog (switched core)" },
        { POWMAN_CHIP_RESET_HAD_SWCORE_PD_BITS, "switched core powered down" },
        { POWMAN_CHIP_RESET_HAD_GLITCH_DETECT_BITS, "glitch detector" },
        { POWMAN_CHIP_RESET_HAD_HZD_SYS_RESET_REQ_BITS, "SYSRESETREQ" },
        { POWMAN_CHIP_RESET_HAD_WATCHDOG_RESET_PSM_BITS, "watchdog (PSM)" },
    };
    uint32_t chip = powman_hw->chip_reset, why = watchdog_hw->reason;
    printf("  reset        : chip_reset 0x%08lx watchdog reason 0x%lx:", (unsigned long)chip,
           (unsigned long)why);
    for (size_t i = 0; i < sizeof causes / sizeof causes[0]; i++) {
        if (chip & causes[i].bit) printf(" %s", causes[i].name);
    }
    if (why & WATCHDOG_REASON_TIMER_BITS) printf(", watchdog timer");
    if (why & WATCHDOG_REASON_FORCE_BITS) printf(", watchdog forced");
    printf("\n");
}

static unsigned timing_rx(uint32_t t) {
    return (t & QMI_M0_TIMING_RXDELAY_BITS) >> QMI_M0_TIMING_RXDELAY_LSB;
}

void board_log_banner(const board_info_t *info) {
    /* Build target and physical identity, reported separately and
     * labelled as such (hardware-notes.md §2.1). Whether a CYW43 is
     * fitted is not probed (§11.2). */
    printf("  build target : board=%s\n", info->sdk_board);
    printf("  module       : id=%s %s rev %u, %lu MB flash, temperature on ADC input %u\n",
           info->unique_id, info->chip, info->chip_version,
           (unsigned long)(info->flash_bytes >> 20), board_is_rp2350a() ? 4u : 8u);
    log_reset();
    printf("  clocks       : clk_sys=%lu Hz clk_peri=%lu Hz, core rail ~%u mV\n",
           (unsigned long)info->clk_sys_hz, (unsigned long)info->clk_peri_hz, info->vreg_mv);
    unsigned fdiv = timing_div(info->qmi_timing);
    printf("  flash        : QMI clkdiv %u rxdelay %u (%lu kHz)\n", fdiv, timing_rx(info->qmi_timing),
           (unsigned long)(fdiv ? info->clk_sys_hz / fdiv / 1000u : 0u));
    unsigned pdiv = timing_div(info->psram_timing);
    printf("  psram        : QMI clkdiv %u rxdelay %u (%lu kHz)\n", pdiv,
           timing_rx(info->psram_timing),
           (unsigned long)(pdiv ? info->clk_sys_hz / pdiv / 1000u : 0u));
}
