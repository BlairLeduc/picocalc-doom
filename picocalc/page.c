/* page.c — the port's own pages (design.md §13.1, §13.3). From
 * picocalc-quest (D8).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "page.h"

#include <stdbool.h>
#include <string.h>

#include "hardware/sync.h"

#include "font8x10.h"
#include "lcd.h"

#define RGB565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

#define COLS (LCD_PANEL_W / FONT_W)    /* 40 */
#define TOP  100u                      /* the first line's y */
#define MESSAGE_LINES 8u

static volatile page_t s_wanted;
static volatile uint32_t s_generation;
static char s_message[PAGE_MESSAGE_MAX];
static uint16_t s_row[2][LCD_PANEL_W];

/* The WAD list (D13): written by core 0 before the page is asked for;
 * after that only the highlight moves. */
static char s_list[PAGE_LIST_MAX][PAGE_LIST_COLS + 1];
static unsigned s_list_n;
static volatile unsigned s_list_selected;
static char s_list_esc[PAGE_LIST_COLS + 1];

#define LIST_TOP    100u
#define LIST_STEP   (FONT_H + 4u)

typedef struct {
    const char *title;
    const char *lines[6];
} page_text_t;

/* Each line at most COLS characters. */
static const page_text_t s_pages[] = {
    [PAGE_NO_PSRAM] = { "The PSRAM failed its check.", {
        "This needs a Pimoroni Pico Plus 2 or",
        "Plus 2 W, which have 8 MB of PSRAM.",
        "",
        "Turn the PicoCalc off and on again to",
        "try once more.",
    } },
    [PAGE_NO_CARD] = { "No SD card.", {
        "Put in a card with Doom's WADs in",
        "/doom. The PicoCalc starts again when",
        "it goes in.",
    } },
    [PAGE_CARD_FAILED] = { "The SD card did not mount.", {
        "It needs to be FAT or exFAT.",
        "",
        "Take the card out and put it back to",
        "try again.",
    } },
    [PAGE_NO_DOOM] = { "No /doom folder on the card.", {
        "Put the WADs, doom.wad or doom2.wad",
        "and the rest, in a folder named /doom.",
        "",
        "Take the card out and put it back to",
        "try again.",
    } },
    [PAGE_NO_IWAD] = { "No IWAD in /doom.", {
        "Doom needs one of doom.wad, doom2.wad,",
        "plutonia.wad, tnt.wad, doom1.wad or",
        "freedoom1.wad and freedoom2.wad.",
        "",
        "Take the card out and put it back to",
        "try again.",
    } },
    /* The message goes between the title and these lines. */
    [PAGE_STOPPED] = { "Doom could not start.", {
        "",
        "Turn the PicoCalc off and on again.",
    } },
    [PAGE_DOOM_ERROR] = { "Doom stopped with an error.", {
        "",
        "Press any key to start again.",
    } },
    [PAGE_TO_SHELL] = { "Back to the shell.", { NULL } },
    [PAGE_POWER_OFF] = { "Switching off.", {
        "If it is still on in ten seconds,",
        "switch it off with its power button.",
    } },
    [PAGE_CANNOT_LEAVE] = { "Doom has quit.", {
        "Its settings are saved. There is no",
        "shell to go back to, and the PicoCalc",
        "cannot switch itself off.",
        "",
        "Press any key to start again.",
    } },
};

void page_show(page_t p) {
    s_wanted = p;
}

void page_show_message(page_t p, const char *message) {
    strncpy(s_message, message ? message : "", sizeof s_message - 1);
    s_message[sizeof s_message - 1] = '\0';
    __dmb();
    s_wanted = p;
}

void page_show_list(const char *const *entries, unsigned n, unsigned selected,
                    const char *esc) {
    if (n > PAGE_LIST_MAX) n = PAGE_LIST_MAX;
    for (unsigned i = 0; i < n; i++) {
        strncpy(s_list[i], entries[i], PAGE_LIST_COLS);
        s_list[i][PAGE_LIST_COLS] = '\0';
    }
    s_list_n = n;
    s_list_selected = selected;
    strncpy(s_list_esc, esc ? esc : "", PAGE_LIST_COLS);
    s_list_esc[PAGE_LIST_COLS] = '\0';
    __dmb();
    s_wanted = PAGE_WAD_LIST;
}

void page_list_select(unsigned selected) {
    s_list_selected = selected;
    __dmb();
    s_generation++;
}

page_t page_wanted(void) {
    return s_wanted;
}

uint32_t page_generation(void) {
    return s_generation;
}

void page_line(unsigned y, const char *s, uint16_t fg, uint16_t bg) {
    unsigned n = (unsigned)strlen(s);
    if (n > COLS) n = COLS;
    unsigned x0 = (LCD_PANEL_W - n * FONT_W) / 2u;

    lcd_blit_begin(0, y, LCD_PANEL_W, FONT_H);
    for (unsigned r = 0; r < FONT_H; r++) {
        /* Two buffers, so this row is drawn while the last is on the
         * wire (lcd.h). */
        uint16_t *row = s_row[r & 1u];
        for (unsigned x = 0; x < LCD_PANEL_W; x++) row[x] = bg;
        for (unsigned i = 0; i < n; i++) {
            uint8_t c = (uint8_t)s[i];
            uint8_t bits = c < FONT_GLYPHS ? font_8x10[c * FONT_H + r] : 0;
            for (unsigned b = 0; b < FONT_W; b++) {
                if (bits & (0x80u >> b)) row[x0 + i * FONT_W + b] = fg;
            }
        }
        lcd_blit_row(row, LCD_PANEL_W);
    }
    lcd_blit_end();
}

/* The list's entries, the chosen one highlighted. Only these rows are
 * drawn again when the highlight moves, so the page does not flash. */
static void list_entries(void) {
    unsigned selected = s_list_selected;
    for (unsigned i = 0; i < s_list_n; i++) {
        bool on = i == selected;
        page_line(LIST_TOP + i * LIST_STEP, s_list[i],
                  on ? RGB565(0, 0, 0) : RGB565(255, 255, 255),
                  on ? RGB565(255, 200, 0) : RGB565(0, 0, 0));
    }
}

static void list_draw(void) {
    const uint16_t bg = RGB565(0, 0, 0), grey = RGB565(128, 128, 128);
    page_line(40, "picocalc-doom", grey, bg);
    page_line(40 + 3u * FONT_H, "Choose a game.", RGB565(255, 200, 0), bg);
    list_entries();
    unsigned y = LIST_TOP + PAGE_LIST_MAX * LIST_STEP + 2u * FONT_H;
    page_line(y, "Up, Down: choose    Enter: start", grey, bg);
    if (s_list_esc[0] != '\0') page_line(y + FONT_H + 4u, s_list_esc, grey, bg);
}

void page_refresh(page_t p) {
    if (p == PAGE_WAD_LIST) list_entries();
}

void page_draw(page_t p) {
    const uint16_t bg = RGB565(0, 0, 0);
    lcd_fill(0, 0, LCD_PANEL_W, LCD_PANEL_H, bg);
    if (p == PAGE_WAD_LIST) {
        list_draw();
        return;
    }
    if (p == PAGE_NONE || p >= sizeof s_pages / sizeof s_pages[0]) return;

    const page_text_t *t = &s_pages[p];
    unsigned y = TOP;
    page_line(y, "picocalc-doom", RGB565(128, 128, 128), bg);
    y += 2u * FONT_H;
    page_line(y, t->title, RGB565(255, 200, 0), bg);
    y += 2u * FONT_H;
    /* The message, broken at spaces into lines of at most COLS, at
     * most MESSAGE_LINES of them. */
    const char *m = (p == PAGE_STOPPED || p == PAGE_DOOM_ERROR) ? s_message : NULL;
    if (m != NULL && *m == '\0') m = "It gave no reason.";
    for (unsigned n = 0; m != NULL && *m != '\0' && n < MESSAGE_LINES; n++) {
        while (*m == ' ' || *m == '\n') m++;
        size_t len = strcspn(m, "\n");
        if (len > COLS) {
            len = COLS;
            while (len > 0 && m[len] != ' ') len--;
            if (len == 0) len = COLS;
        }
        char line[COLS + 1];
        memcpy(line, m, len);
        line[len] = '\0';
        page_line(y, line, RGB565(255, 255, 255), bg);
        y += FONT_H + 2u;
        m += len;
    }
    for (unsigned i = 0; i < sizeof t->lines / sizeof t->lines[0] && t->lines[i]; i++) {
        page_line(y, t->lines[i], RGB565(255, 255, 255), bg);
        y += FONT_H + 2u;
    }
}
