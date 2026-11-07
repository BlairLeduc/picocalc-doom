/* status.h — the bottom band, under the picture (design.md §7.5).
 *
 * The 40 rows under upstream's 320x240 frame at y = 40 are outside
 * anything Doom draws. Core 0 puts a message there, a title and a line
 * of help, for a few seconds: a settings line not used (§10.3), the run
 * mode changed by Caps Lock (§9.4). Core 1, which owns the LCD, draws it
 * once when it changes and clears it when its time is up. A message asked
 * for while a page covers the panel, or before the first frame has
 * cleared the bands, waits for them. From picocalc-quest's status.c (D8).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_STATUS_H
#define PICOCALC_DOOM_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#define STATUS_SHOW_US 3000000u
/* For what is easy to miss and matters: a settings line not used. */
#define STATUS_LONG_US 10000000u

/* Core 0: show `title` over `help` (either may be NULL) for
 * STATUS_SHOW_US. Both are read when core 1 draws them, so they must
 * stay: string literals, or static buffers not written again. */
void status_show(const char *title, const char *help);

/* The same, for `us`. */
void status_show_for(const char *title, const char *help, uint32_t us);

/* Core 1, every loop: draw or clear the band if it is due. `ready` is
 * false while a page covers the panel or the bands are not yet cleared:
 * nothing is drawn, and a message waiting stays waiting. */
void status_service(uint32_t now_us, bool ready);

#endif /* PICOCALC_DOOM_STATUS_H */
