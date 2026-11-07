/* exit.h — leaving (design.md §13.3, D14).
 *
 * To the shell when UF2 Loader started the port and a card is in
 * (picocalc_shell.h), and otherwise powering the PicoCalc off through the
 * southbridge, if its firmware can. From the WAD list's Esc (M4), and
 * from Doom's own Quit after the at-exit list (I_Quit(), M8). From
 * picocalc-quest's exit.c (D8).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_EXIT_H
#define PICOCALC_DOOM_EXIT_H

/* Core 0: where Esc goes, for the page that offers it ("Esc: back to the
 * shell", "Esc: switch off"), or NULL when it can go nowhere. */
const char *picocalc_exit_label(void);

/* Core 0, with no file open for writing: leave. Returns only if it
 * cannot, having logged why. */
void picocalc_exit(void);

#endif /* PICOCALC_DOOM_EXIT_H */
