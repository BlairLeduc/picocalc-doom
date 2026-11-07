/* page.h — the pages shown when something Doom needs is missing
 * (design.md §13.1, EL §8.1): never a blank screen.
 *
 * Core 0 finds the failure and names the page; core 1, which owns the
 * LCD, draws it over the whole panel in the 8x10 font and leaves it
 * there, presenting no frames over it. The PSRAM's page cannot go
 * through the frame pool, which lives in the PSRAM, so all of them are
 * drawn this way.
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
} page_t;

/* The longest message a page shows; longer is cut. */
#define PAGE_MESSAGE_MAX 320

/* Core 0: show a page. Core 1 draws it on its next loop. */
void page_show(page_t p);

/* Core 0: show a page with a message of its own, wrapped to the panel's
 * width. The message is copied. */
void page_show_message(page_t p, const char *message);

/* Core 1: the page core 0 asked for, and drawing it. */
page_t page_wanted(void);
void page_draw(page_t p);

/* Core 1: one line of text at y, centred, FONT_H rows of pixels, over a
 * full-width background; longer than the panel is cut. */
void page_line(unsigned y, const char *s, uint16_t fg, uint16_t bg);

#endif /* PICOCALC_DOOM_PAGE_H */
