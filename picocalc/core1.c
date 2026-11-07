/* core1.c — core 1's loop: the presenter, the pages, the southbridge
 * and the log (design.md §6.1, §7.4, §9.1, §13.1, §14.3).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "core1.h"

#include <stddef.h>
#include <stdio.h>

#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include "audio_shim.h"
#include "board.h"
#include "opl_core1.h"
#include "framepool.h"
#include "input.h"
#include "kbd.h"
#include "lcd.h"
#include "log.h"
#include "page.h"
#include "southbridge.h"
#include "status.h"
#include "video.h"

/* The battery is refreshed by the MCU every 20 s (hardware-notes.md §6);
 * once a second is plenty. */
#define BAT_POLL_US   1000000u
#define HEARTBEAT_US  1000000u

core1_bringup_t g_core1 = { .sb_version = -1 };

volatile uint32_t g_core0_sleep_us;
volatile uint32_t g_core0_frames;
volatile uint32_t g_core0_stack;
volatile uint32_t g_core0_heap;

#define RGB565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

/* Where the picture goes (D3, §7.1): upstream's 320x240 frame at y 40. */
#define FRAME_Y 40u
#define FRAME_W 320u
#define FRAME_H 240u

/* The log is pumped this often within a present, so a frame (some 18
 * ms) does not leave the UART idle for its length (§14.2). */
#define PUMP_ROWS 16u

static volatile bool s_park, s_parked;

/* Core 0's request to power off (§13.3), done between two of the
 * keyboard's transactions: the seconds, and the answer. */
static volatile uint8_t s_power_off;
static volatile int s_power_off_result;

bool core1_power_off(uint8_t seconds) {
    s_power_off_result = 0;
    __dmb();
    s_power_off = seconds;
    uint32_t t0 = time_us_32();
    while (s_power_off_result == 0 && time_us_32() - t0 < 200000u) tight_loop_contents();
    return s_power_off_result > 0;
}

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
/* How long the presenter renders music after handing a row to the LCD's
 * DMA (opl_core1.c): within the row's own transfer. */
#define OPL_ROW_US 50u

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
#if PICOCALC_DOOM_OPL_CORE1
        /* The music, while the row goes out: about 68 us of SPI at 75 MHz,
         * most of which the next row's wait would spend idle (§8.3). */
        opl_core1_step(time_us_32() + OPL_ROW_US);
#endif
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
extern char end, __HeapLimit;
void *sbrk(ptrdiff_t incr);

uint32_t core0_heap_used(void) {
    return (uint32_t)((uintptr_t)sbrk(0) - (uintptr_t)&end);
}

uint32_t core0_heap_size(void) {
    return (uint32_t)((uintptr_t)&__HeapLimit - (uintptr_t)&end);
}
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
    uint32_t page_gen = page_generation();
    int battery = -1, temp_c = board_temp_c();
    uint32_t now = time_us_32();
    uint32_t next_bat = now, next_kbd = now, next_beat = now + HEARTBEAT_US;
    uint32_t poll_us = 0, polls = 0;
    uint32_t seconds = 0, loops = 0;
    uint32_t last_sleep = g_core0_sleep_us, last_frames = g_core0_frames;
    uint32_t last_pub = 0, last_drop = 0;
    present_stats_t last_present = s_present;
    video_stats_t last_video = g_video;
    input_stats_t last_input = g_input;
    audio_stats_t last_audio = g_audio;
#if PICOCALC_DOOM_OPL_CORE1
    opl_core1_stats_t last_opl = g_opl_core1;
#endif
    for (;;) {
        log_pump();
        loops++;
#if PICOCALC_DOOM_OPL_CORE1
        /* The music (§8.3): the presenter renders it in the LCD's waits;
         * here, what is left, for up to 2 ms a loop. */
        opl_core1_step(time_us_32() + 2000u);
#endif
        if (s_park) {
            /* Between transactions: nothing of the southbridge's is in
             * flight here. */
            s_parked = true;
            for (;;) tight_loop_contents();
        }

        /* The power off asked for by core 0's exit (§13.3), between two
         * of the keyboard's transactions. */
        if (s_power_off != 0) {
            s_power_off_result = sb_write(SB_REG_OFF, s_power_off, NULL) == SB_OK ? 1 : -1;
            s_power_off = 0;
        }

        /* A page (§13.1) covers the panel and stays; nothing is
         * presented over it. The WAD list redraws its entries when the
         * highlight moves. */
        page_t wanted = page_wanted();
        uint32_t gen = page_generation();
        if (wanted != page) {
            page = wanted;
            page_draw(page);
            bands_cleared = false;
        } else if (gen != page_gen) {
            page_refresh(page);
        }
        page_gen = gen;

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

        /* The bottom band's message (§7.5), once the first frame has
         * cleared the bands and while no page covers them. */
        status_service(time_us_32(), page == PAGE_NONE && bands_cleared);

        /* The keyboard (§9.1): the MCU's FIFO every KBD_POLL_US, between
         * presents, and in the development build the UART's keys every
         * loop (§14.1). */
        kbd_uart();
        now = time_us_32();
        if ((int32_t)(now - next_kbd) >= 0) {
            next_kbd = now + KBD_POLL_US;
            kbd_poll();
            poll_us += time_us_32() - now;
            polls++;
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
            input_stats_t in = g_input;
            uint32_t n_taken = in.taken - last_input.taken;
            uint32_t n_frame = in.frame_n - last_input.frame_n;
            printf("hb %lu: core 0 busy %lu.%lu%%, frames %lu, published %lu, presented %lu, "
                   "dropped %lu; copy %lu us (max %lu), publish %lu us (max %lu), "
                   "present %lu us (max %lu), margins %lu; stacks: core 0 %lu of %lu B, "
                   "core 1 %lu of %lu B; heap %lu of %lu B; temp %d C, battery %d%%%s, i2c errors %lu, "
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
                   (unsigned long)g_core0_heap, (unsigned long)core0_heap_size(),
                   temp_c, battery < 0 ? -1 : battery & 0x7f,
                   battery >= 0 && (battery & 0x80) ? " charging" : "",
                   (unsigned long)sb_error_count(), (unsigned long)log_dropped(),
                   (unsigned long)loops);
            /* The keyboard's line (M4): pairs read and taken, their wait in
             * the ring, a press to the first frame after Doom took it,
             * releases I_GetEvent() left to the next tic (§9.1), and the
             * poll's own cost on core 1. */
            printf("hb %lu keys: read %lu, ring full %lu; taken %lu, wait %lu us (max %lu), "
                   "events %lu, repeats %lu, stray %lu, unknown %lu, held over %lu; "
                   "key to frame %lu us (max %lu); polls %lu, %lu us each\n",
                   (unsigned long)seconds, (unsigned long)kbd_events(),
                   (unsigned long)kbd_overflows(), (unsigned long)n_taken,
                   (unsigned long)(n_taken ? (in.wait_sum_us - last_input.wait_sum_us) / n_taken : 0),
                   (unsigned long)in.wait_max_us, (unsigned long)(in.events - last_input.events),
                   (unsigned long)in.repeats, (unsigned long)in.stray, (unsigned long)in.unknown,
                   (unsigned long)in.held_over,
                   (unsigned long)(n_frame ? (in.frame_sum_us - last_input.frame_sum_us) / n_frame : 0),
                   (unsigned long)in.frame_max_us, (unsigned long)polls,
                   (unsigned long)(polls ? poll_us / polls : 0));
            /* The audio's line (M6, §8.3): halves refilled and late, upstream's
             * slots played and those its ring was empty for (in samples, the
             * slot's frames), the interrupt's time, and mixing's, in
             * I_UpdateSound(), each as a share of core 0's second. */
            audio_stats_t au = g_audio;
            uint32_t slot = audio_slot_frames();
            uint32_t n_played = au.played - last_audio.played;
            uint32_t irq_share = (au.irq_us - last_audio.irq_us) / (HEARTBEAT_US / 1000u);
            uint32_t mix_share = (au.mix_us - last_audio.mix_us) / (HEARTBEAT_US / 1000u);
            printf("hb %lu audio: halves %lu, late %lu; slots played %lu, underrun %lu "
                   "(%lu samples; %lu since boot, %lu before the first); "
                   "interrupt %lu.%lu%% (max %lu us), mix %lu.%lu%% (%lu us a slot, "
                   "max call %lu us), updates %lu, pushes %lu; peak %lu\n",
                   (unsigned long)seconds, (unsigned long)(au.halves - last_audio.halves),
                   (unsigned long)au.late, (unsigned long)n_played,
                   (unsigned long)(au.underruns - last_audio.underruns),
                   (unsigned long)((au.underruns - last_audio.underruns) * slot),
                   (unsigned long)au.underruns, (unsigned long)au.startup,
                   (unsigned long)(irq_share / 10u), (unsigned long)(irq_share % 10u),
                   (unsigned long)au.irq_max_us,
                   (unsigned long)(mix_share / 10u), (unsigned long)(mix_share % 10u),
                   (unsigned long)(n_played ? (au.mix_us - last_audio.mix_us) / n_played : 0),
                   (unsigned long)au.mix_max_us,
                   (unsigned long)(au.updates - last_audio.updates), (unsigned long)au.pushes,
                   (unsigned long)au.peak);
            g_audio_peak_ask++;
#if PICOCALC_DOOM_OPL_CORE1
            opl_core1_stats_t op = g_opl_core1;
            uint32_t n_music = op.music - last_opl.music;
            uint32_t opl_share = (op.render_us - last_opl.render_us) / (HEARTBEAT_US / 1000u);
            printf("hb %lu opl: core 1 rendered %lu slots, %lu.%lu%% of core 1 (%lu us a slot, "
                   "max %lu), writes %lu, done %lu of %lu handed over, late halves %lu, "
                   "dropped %lu\n",
                   (unsigned long)seconds, (unsigned long)n_music,
                   (unsigned long)(opl_share / 10u), (unsigned long)(opl_share % 10u),
                   (unsigned long)(n_music ? (op.render_us - last_opl.render_us) / n_music : 0),
                   (unsigned long)op.render_max_us,
                   (unsigned long)(op.writes - last_opl.writes), (unsigned long)op.done,
                   (unsigned long)op.committed, (unsigned long)au.music_late,
                   (unsigned long)op.dropped);
            last_opl = op;
#endif
            last_audio = au;
            last_input = in;
            poll_us = polls = 0;
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
