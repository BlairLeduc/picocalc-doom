/* core1.c — core 1's loop: the southbridge, the panel and the log
 * (design.md §6.1, §14.3).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "core1.h"

#include <stdio.h>

#include "hardware/sync.h"
#include "pico/stdlib.h"

#include "board.h"
#include "lcd.h"
#include "log.h"
#include "southbridge.h"

/* The battery is refreshed by the MCU every 20 s (hardware-notes.md §6);
 * once a second is plenty. Until M4 polls the keyboard, the battery read
 * is what keeps the MCU's bus watchdog fed (§6.1). */
#define BAT_POLL_US   1000000u
#define HEARTBEAT_US  1000000u

core1_bringup_t g_core1 = { .sb_version = -1 };

volatile uint32_t g_core0_sleep_us;
volatile uint32_t g_core0_frames;
volatile uint32_t g_core0_stack;

#define RGB565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

/* Where the picture goes (D3, §7.1): upstream's 320x240 frame at y 40. */
#define FRAME_Y 40u
#define FRAME_H 240u

static volatile bool s_park, s_parked;

void core1_park(void) {
    s_park = true;
    uint32_t t0 = time_us_32();
    while (!s_parked && time_us_32() - t0 < 100000u) tight_loop_contents();
}

/* ---- the stacks (§12.1) --------------------------------------------- */

#define STACK0_PAINT 0xC0C0C0C0u
#define STACK1_PAINT 0xC1C1C1C1u

extern uint32_t __StackOneBottom, __StackOneTop;
extern uint32_t __StackTop;
extern char __scratch_y_end__;

static uint32_t *core0_stack_floor(void) {
    return (uint32_t *)(((uintptr_t)&__scratch_y_end__ + 3u) & ~(uintptr_t)3u);
}

/* Below this function's own frame, with room for it: it calls nothing,
 * and nothing has set an interrupt going this early in main(). */
void __attribute__((noinline)) core0_paint_stack(void) {
    uintptr_t sp;
    __asm volatile("mov %0, sp" : "=r"(sp));
    uint32_t *end = (uint32_t *)((sp - 64u) & ~(uintptr_t)3u);
    for (uint32_t *p = core0_stack_floor(); p < end; p++) *p = STACK0_PAINT;
}

/* Core 0's own: core 1 reads no engine memory (§6.2), and core 0's stack
 * is that. */
uint32_t core0_stack_used(void) {
    const uint32_t *p = core0_stack_floor();
    while (p < &__StackTop && *p == STACK0_PAINT) p++;
    return (uint32_t)((uintptr_t)&__StackTop - (uintptr_t)p);
}

uint32_t core0_stack_size(void) {
    return (uint32_t)((uintptr_t)&__StackTop - (uintptr_t)core0_stack_floor());
}

void core1_paint_stack(void) {
    for (uint32_t *p = &__StackOneBottom; p < &__StackOneTop; p++) *p = STACK1_PAINT;
}

uint32_t core1_stack_used(void) {
    const uint32_t *p = &__StackOneBottom;
    while (p < &__StackOneTop && *p == STACK1_PAINT) p++;
    return (uint32_t)((uintptr_t)&__StackOneTop - (uintptr_t)p);
}

uint32_t core1_stack_size(void) {
    return (uint32_t)((uintptr_t)&__StackOneTop - (uintptr_t)&__StackOneBottom);
}

#if PICOCALC_DOOM_TEST_PATTERN
/* The corner-coded test pattern (EL §13.3, hardware-notes.md §10):
 * orientation, colour order and all four edges, by eye. Drawn at boot in
 * a build that asks for it, M1's bring-up and the default until M3
 * presents frames.
 *
 *   - red square top left, green top right, blue bottom left, white
 *     bottom right, 32 pixels each;
 *   - a one-pixel white border round the whole panel;
 *   - in the picture's 240 rows: eight colour bars, white, yellow, cyan,
 *     green, magenta, red, blue, black, 40 pixels each, over a grey ramp
 *     of 32 steps in the last 40 rows, black on the left.
 */
static void test_pattern(void) {
    static const uint16_t bars[8] = {
        RGB565(255, 255, 255), RGB565(255, 255, 0), RGB565(0, 255, 255), RGB565(0, 255, 0),
        RGB565(255, 0, 255),   RGB565(255, 0, 0),   RGB565(0, 0, 255),   RGB565(0, 0, 0),
    };
    const uint16_t white = RGB565(255, 255, 255);
    const unsigned w = LCD_PANEL_W, h = LCD_PANEL_H, sq = 32u;

    for (unsigned i = 0; i < 8u; i++) lcd_fill(i * 40u, FRAME_Y, 40u, FRAME_H - 40u, bars[i]);
    for (unsigned i = 0; i < 32u; i++) {
        unsigned v = i * 255u / 31u;
        lcd_fill(i * 10u, FRAME_Y + FRAME_H - 40u, 10u, 40u, RGB565(v, v, v));
    }

    lcd_fill(0, 0, w, 1, white);
    lcd_fill(0, h - 1u, w, 1, white);
    lcd_fill(0, 0, 1, h, white);
    lcd_fill(w - 1u, 0, 1, h, white);

    lcd_fill(0, 0, sq, sq, RGB565(255, 0, 0));
    lcd_fill(w - sq, 0, sq, sq, RGB565(0, 255, 0));
    lcd_fill(0, h - sq, sq, sq, RGB565(0, 0, 255));
    lcd_fill(w - sq, h - sq, sq, sq, white);
}
#endif

void core1_main(void) {
    /* 1. The southbridge first: a dead bus is the first thing to know
     *    about (hardware-notes.md §10). */
    g_core1.i2c_hz = sb_init();
    uint8_t r[2];
    uint32_t t0 = time_us_32();
    if (sb_read(SB_REG_VER, r) == SB_OK) g_core1.sb_version = r[1];
    g_core1.i2c_us = time_us_32() - t0;
    /* Firmware that can power off replies to a read of OFF with
     * something other than 0. */
    g_core1.sb_power_off = sb_read(SB_REG_OFF, r) == SB_OK && r[1] != 0;

    /* 2. The LCD, cleared before DISPON, then the pattern if asked for. */
    t0 = time_us_32();
    g_core1.spi_hz = lcd_init();
    g_core1.lcd_us = time_us_32() - t0;
#if PICOCALC_DOOM_TEST_PATTERN
    t0 = time_us_32();
    test_pattern();
    g_core1.pattern_us = time_us_32() - t0;
#endif

    board_temp_init();

    __dmb();
    g_core1.ready = true;

    int battery = -1, temp_c = board_temp_c();
    uint32_t now = time_us_32();
    uint32_t next_bat = now, next_beat = now + HEARTBEAT_US;
    uint32_t seconds = 0, loops = 0;
    uint32_t last_sleep = g_core0_sleep_us, last_frames = g_core0_frames;
    for (;;) {
        log_pump();
        loops++;
        if (s_park) {
            /* Between transactions: nothing of the southbridge's is in
             * flight here. */
            s_parked = true;
            for (;;) tight_loop_contents();
        }

        now = time_us_32();
        if ((int32_t)(now - next_bat) >= 0) {
            next_bat = now + BAT_POLL_US;
            battery = sb_read(SB_REG_BAT, r) == SB_OK ? r[1] : -1;
            temp_c = board_temp_c();
        }

        /* The heartbeat's skeleton (§14.3): the later milestones' counters
         * join it as they arrive. */
        now = time_us_32();
        if ((int32_t)(now - next_beat) >= 0) {
            next_beat += HEARTBEAT_US;
            seconds++;
            uint32_t sleep = g_core0_sleep_us, frames = g_core0_frames;
            /* Core 0's busy share: the second's wall time not in
             * DG_SleepMs (§14.3), in thousandths. */
            uint32_t idle = sleep - last_sleep;
            uint32_t busy = idle >= HEARTBEAT_US ? 0 : 1000u - idle / (HEARTBEAT_US / 1000u);
            printf("hb %lu: core 0 busy %lu.%lu%%, frames %lu, stacks: core 0 %lu of %lu B, "
                   "core 1 %lu of %lu B; temp %d C, battery %d%%%s, i2c errors %lu, "
                   "log dropped %lu, core 1 loops %lu\n",
                   (unsigned long)seconds, (unsigned long)(busy / 10u),
                   (unsigned long)(busy % 10u), (unsigned long)(frames - last_frames),
                   (unsigned long)g_core0_stack, (unsigned long)core0_stack_size(),
                   (unsigned long)core1_stack_used(), (unsigned long)core1_stack_size(),
                   temp_c, battery < 0 ? -1 : battery & 0x7f,
                   battery >= 0 && (battery & 0x80) ? " charging" : "",
                   (unsigned long)sb_error_count(), (unsigned long)log_dropped(),
                   (unsigned long)loops);
            last_sleep = sleep;
            last_frames = frames;
            loops = 0;
        }

        /* Never sleep_us here (hardware-notes.md §9.7). */
        busy_wait_us_32(20);
    }
}
