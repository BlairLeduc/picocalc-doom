/* status.c — the bottom band, under the picture (design.md §7.5).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "status.h"

#include "hardware/sync.h"

#include "font8x10.h"
#include "lcd.h"
#include "page.h"

#define RGB565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

/* The band: rows 280 to 319, under the 240-row frame at 40 (§7.1, D3). */
#define BAND_Y 280u
#define BAND_H (LCD_PANEL_H - BAND_Y)

static const char *volatile s_title, *volatile s_help;
static volatile uint32_t s_us;
static volatile uint32_t s_seq;

void status_show_for(const char *title, const char *help, uint32_t us) {
    s_title = title;
    s_help = help;
    s_us = us;
    __dmb();
    s_seq++;
}

void status_show(const char *title, const char *help) {
    status_show_for(title, help, STATUS_SHOW_US);
}

void status_service(uint32_t now_us, bool ready) {
    static uint32_t seen, until;
    static bool shown;

    if (!ready) {
        /* A page draws over the band; what was shown is gone, and what
         * is asked for waits. */
        shown = false;
        return;
    }
    uint32_t seq = s_seq;
    if (seq != seen) {
        seen = seq;
        __dmb();
        const char *title = s_title, *help = s_help;
        const uint16_t bg = 0;
        lcd_fill(0, BAND_Y, LCD_PANEL_W, BAND_H, bg);
        page_line(BAND_Y + 8u, title ? title : "", RGB565(255, 200, 0), bg);
        page_line(BAND_Y + 8u + FONT_H + 4u, help ? help : "", RGB565(160, 160, 160), bg);
        until = now_us + s_us;
        shown = true;
    } else if (shown && (int32_t)(now_us - until) >= 0) {
        lcd_fill(0, BAND_Y, LCD_PANEL_W, BAND_H, 0);
        shown = false;
    }
}
