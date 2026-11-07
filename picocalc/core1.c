/* core1.c — core 1's loop: the presenter, the pages, the southbridge
 * and the log (design.md §6.1, §7.4, §13.1, §14.3).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "core1.h"

#include <stdio.h>

#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include "board.h"
#include "framepool.h"
#include "lcd.h"
#include "log.h"
#include "page.h"
#include "southbridge.h"
#include "video.h"

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
#define FRAME_W 320u
#define FRAME_H 240u

/* The log is pumped this often within a present, so a frame (some 18
 * ms) does not leave the UART idle for its length (§14.2). */
#define PUMP_ROWS 16u

static volatile bool s_park, s_parked;

void core1_park(void) {
    s_park = true;
    uint32_t t0 = time_us_32();
    while (!s_parked && time_us_32() - t0 < 100000u) tight_loop_contents();
}

void picocalc_restart(void) {
    busy_wait_us_32(100000);          /* the log's last line out */
    /* Never reset with a southbridge transaction in flight: it can leave
     * the MCU dead until a power cycle (§6.2, hardware-notes.md §6.1). */
    core1_park();
    watchdog_reboot(0, 0, 0);
    for (;;) tight_loop_contents();
}

/* ---- the presenter (§7.4) ------------------------------------------- */

typedef struct {
    uint32_t presents;
    uint32_t last_us;    /* the last present, wall time   */
    uint32_t max_us;     /* the longest since boot        */
    uint32_t sum_us;     /* all presents, since boot      */
} present_stats_t;

static present_stats_t s_present;
static uint16_t s_line[2][FRAME_W];

/* Rows of indices, copied out of the PSRAM's uncached alias a row ahead
 * of the one being expanded (hardware-notes.md §4.7, §9.3). */
static uint32_t s_idx[2][FRAME_W / 4u];
static int s_row_dma = -1;

static void row_dma_start(const uint8_t *src, unsigned slot) {
    dma_channel_config c = dma_channel_get_default_config((unsigned)s_row_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, true);
    dma_channel_configure((unsigned)s_row_dma, &c, s_idx[slot], src, FRAME_W / 4u, true);
}

/* The whole frame, through its palette, into one line buffer while the
 * other is on the wire (hardware-notes.md §4.6). */
static void present(int i) {
    const uint8_t *px = video_pixels(i);
    const uint16_t *pal = video_palette(i);
    uint32_t t0 = time_us_32();

    lcd_blit_begin(0, FRAME_Y, FRAME_W, FRAME_H);
    row_dma_start(px, 0);
    for (unsigned row = 0; row < FRAME_H; row++) {
        dma_channel_wait_for_finish_blocking((unsigned)s_row_dma);
        if (row + 1u < FRAME_H) row_dma_start(px + (row + 1u) * FRAME_W, (row + 1u) & 1u);
        uint16_t *line = s_line[row & 1u];
        frame_expand_row((const uint8_t *)s_idx[row & 1u], pal, line, FRAME_W);
        lcd_blit_row(line, FRAME_W);
        if (row % PUMP_ROWS == PUMP_ROWS - 1u) log_pump();
    }
    lcd_blit_end();

    uint32_t us = time_us_32() - t0;
    s_present.presents++;
    s_present.last_us = us;
    s_present.sum_us += us;
    if (us > s_present.max_us) s_present.max_us = us;
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
    s_row_dma = dma_claim_unused_channel(true);

    __dmb();
    g_core1.ready = true;

    bool bands_cleared = false;
    page_t page = PAGE_NONE;
    int battery = -1, temp_c = board_temp_c();
    uint32_t now = time_us_32();
    uint32_t next_bat = now, next_beat = now + HEARTBEAT_US;
    uint32_t seconds = 0, loops = 0;
    uint32_t last_sleep = g_core0_sleep_us, last_frames = g_core0_frames;
    uint32_t last_pub = 0, last_drop = 0;
    present_stats_t last_present = s_present;
    video_stats_t last_video = g_video;
    for (;;) {
        log_pump();
        loops++;
        if (s_park) {
            /* Between transactions: nothing of the southbridge's is in
             * flight here. */
            s_parked = true;
            for (;;) tight_loop_contents();
        }

        /* A page (§13.1) covers the panel and stays; nothing is
         * presented over it. */
        page_t wanted = page_wanted();
        if (wanted != page) {
            page = wanted;
            page_draw(page);
            bands_cleared = false;
        }

        int frame = video_take();
        if (frame >= 0 && page != PAGE_NONE) {
            video_release(frame);
            frame = -1;
        }
        if (frame >= 0) {
            if (!bands_cleared) {
                /* The bands are black (§7.5): the test pattern's
                 * corners and border, if drawn, go with the first
                 * frame. */
                lcd_fill(0, 0, LCD_PANEL_W, FRAME_Y, 0);
                lcd_fill(0, FRAME_Y + FRAME_H, LCD_PANEL_W, LCD_PANEL_H - FRAME_Y - FRAME_H, 0);
                bands_cleared = true;
            }
            present(frame);
            video_release(frame);
        }

        now = time_us_32();
        if ((int32_t)(now - next_bat) >= 0) {
            next_bat = now + BAT_POLL_US;
            battery = sb_read(SB_REG_BAT, r) == SB_OK ? r[1] : -1;
            temp_c = board_temp_c();
        }

        /* The heartbeat (§14.3): the later milestones' counters join it
         * as they arrive. */
        now = time_us_32();
        if ((int32_t)(now - next_beat) >= 0) {
            next_beat += HEARTBEAT_US;
            seconds++;
            uint32_t sleep = g_core0_sleep_us, frames = g_core0_frames;
            uint32_t pub, drop;
            video_counts(&pub, &drop);
            video_stats_t v = g_video;
            present_stats_t ps = s_present;
            /* Core 0's busy share: the second's wall time not in
             * DG_SleepMs (§14.3), in thousandths. */
            uint32_t idle = sleep - last_sleep;
            uint32_t busy = idle >= HEARTBEAT_US ? 0 : 1000u - idle / (HEARTBEAT_US / 1000u);
            uint32_t n_fin = v.finishes - last_video.finishes;
            uint32_t n_pres = ps.presents - last_present.presents;
            printf("hb %lu: core 0 busy %lu.%lu%%, frames %lu, published %lu, presented %lu, "
                   "dropped %lu; copy %lu us (max %lu), publish %lu us (max %lu), "
                   "present %lu us (max %lu), margins %lu; stacks: core 0 %lu of %lu B, "
                   "core 1 %lu of %lu B; temp %d C, battery %d%%%s, i2c errors %lu, "
                   "log dropped %lu, core 1 loops %lu\n",
                   (unsigned long)seconds, (unsigned long)(busy / 10u),
                   (unsigned long)(busy % 10u), (unsigned long)(frames - last_frames),
                   (unsigned long)(pub - last_pub), (unsigned long)n_pres,
                   (unsigned long)(drop - last_drop),
                   (unsigned long)(n_fin ? (v.copy_us - last_video.copy_us) / n_fin : 0),
                   (unsigned long)v.copy_max_us,
                   (unsigned long)(n_fin ? (v.publish_us - last_video.publish_us) / n_fin : 0),
                   (unsigned long)v.publish_max_us,
                   (unsigned long)(n_pres ? (ps.sum_us - last_present.sum_us) / n_pres : 0),
                   (unsigned long)ps.max_us, (unsigned long)v.margins,
                   (unsigned long)g_core0_stack, (unsigned long)core0_stack_size(),
                   (unsigned long)core1_stack_used(), (unsigned long)core1_stack_size(),
                   temp_c, battery < 0 ? -1 : battery & 0x7f,
                   battery >= 0 && (battery & 0x80) ? " charging" : "",
                   (unsigned long)sb_error_count(), (unsigned long)log_dropped(),
                   (unsigned long)loops);
            last_sleep = sleep;
            last_frames = frames;
            last_pub = pub;
            last_drop = drop;
            last_video = v;
            last_present = ps;
            loops = 0;
        }

        /* Never sleep_us here (hardware-notes.md §9.7). */
        if (frame < 0) busy_wait_us_32(20);
    }
}
