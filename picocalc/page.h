/* page.h — the port's own pages (design.md §13.1, §13.3, EL §8.1): the
 * WAD list, the way out, and a page for each failure, so never a blank
 * screen.
 *
 * Core 0 names the page; core 1, which owns the LCD, draws it over the
 * whole panel in the 8x10 font and leaves it there, presenting no frames
 * over it. The PSRAM's page cannot go through the frame pool, which
 * lives in the PSRAM, so all of them are drawn this way.
 *
 * From picocalc-quest's page.c (D8), with Doom's pages.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_PAGE_H
#define PICOCALC_DOOM_PAGE_H

#include <stdint.h>

typedef enum {
    PAGE_NONE = 0,
    PAGE_NO_PSRAM,     /* the PSRAM failed its check (§11.3)         */
    PAGE_NO_CARD,      /* card detect says the slot is empty         */
    PAGE_CARD_FAILED,  /* a card is in, but did not mount            */
    PAGE_NO_DOOM,      /* the card has no /doom                      */
    PAGE_NO_IWAD,      /* /doom holds no IWAD Doom knows             */
    PAGE_STOPPED,      /* the port could not start Doom; a message   */
    PAGE_DOOM_ERROR,   /* Doom's I_Error() ended the game; a message */
    PAGE_WAD_LIST,     /* the IWADs to choose from (D13)             */
    PAGE_TO_SHELL,     /* leaving to the shell (§13.3)               */
    PAGE_POWER_OFF,    /* the southbridge is switching off (§13.3)   */
    PAGE_CANNOT_LEAVE, /* Doom quit, with no shell and no power off  */
} page_t;

/* The longest message a page shows; longer is cut. */
#define PAGE_MESSAGE_MAX 320

/* Core 0: show a page. Core 1 draws it on its next loop. */
void page_show(page_t p);

/* Core 0: show a page with a message of its own, wrapped to the panel's
 * width. The message is copied. */
void page_show_message(page_t p, const char *message);

/* The WAD list's most entries, and the longest. */
#define PAGE_LIST_MAX  10
#define PAGE_LIST_COLS 40

/* Core 0: show the WAD list, its entries copied, `selected` highlighted,
 * and `esc` saying where Esc goes (NULL: nowhere). */
void page_show_list(const char *const *entries, unsigned n, unsigned selected,
                    const char *esc);

/* Core 0: move the WAD list's highlight. */
void page_list_select(unsigned selected);

/* Core 1: the page core 0 asked for, and drawing it; and a count that
 * moves when the page's contents change without the page changing (the
 * list's highlight), for page_refresh. */
page_t page_wanted(void);
uint32_t page_generation(void);
void page_draw(page_t p);
void page_refresh(page_t p);

/* Core 1: one line of text at y, centred, FONT_H rows of pixels, over a
 * full-width background; longer than the panel is cut. */
void page_line(unsigned y, const char *s, uint16_t fg, uint16_t bg);

#endif /* PICOCALC_DOOM_PAGE_H */
