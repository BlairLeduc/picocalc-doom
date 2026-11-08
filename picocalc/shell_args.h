/* shell_args.h — Doom's arguments from the shell (design.md §13.4, D15).
 *
 * `picocalc-doom -file sigil.wad -warp 1 1` in the shell, or
 * `PICOCALC-DOOM /FILE SIGIL.WAD /WARP 1 1` in its DOS flavour, reaches
 * the port as /.shell/args (picocalc_shell.h). The words go to Doom's
 * myargv after picocalc.ini's args. In the DOS flavour a word that starts
 * with '/', DOS's switch character and never a path there, is Doom's
 * option with '-'; Doom compares options without case. The files that
 * follow -iwad, -file, -merge, -deh, -playdemo, -timedemo and -record are
 * made full paths from the user's directory, since Doom's own is /doom,
 * when the file is there; one that is not goes as typed, for Doom to find
 * in /doom (`-iwad doom2.wad` from anywhere), and a demo not found is a
 * lump's name. -record's file is Doom's to make, so always a path.
 *
 * shell_args_to_doom() is pure, and has a host test (§15).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_SHELL_ARGS_H
#define PICOCALC_DOOM_SHELL_ARGS_H

#include <stdbool.h>
#include <stddef.h>

#include "picocalc_shell.h"

/* The name the port declares to the shell (PICOCALC_SHELL_TAKES_ARGS). */
#define SHELL_ARGS_NAME "picocalc-doom"

/* The most words taken from the shell. */
#define SHELL_ARGS_MAX 24

/* The shell's words in `args` as Doom's: at most `max` pointers into
 * `out`, filled from `buf`'s `size` bytes. `exists` says whether a path
 * names a file. Returns how many; words that do not fit are dropped and
 * counted in *dropped. */
int shell_args_to_doom(const picocalc_shell_args_t *args, char *buf, size_t size, char **out,
                       int max, bool (*exists)(const char *path), int *dropped);

#ifndef PICOCALC_DOOM_HOST
/* Core 0, with the card mounted, before Doom: read /.shell/args, remove
 * it, and keep its words for shell_args_words(). Logs what it found. */
void shell_args_load(void);

/* The words shell_args_load() kept, as Doom's arguments; *n of them. */
char **shell_args_words(int *n);
#endif

#endif /* PICOCALC_DOOM_SHELL_ARGS_H */
