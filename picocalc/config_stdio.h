/* config_stdio.h — the stdio calls Doom's configuration file makes, on the
 * card (design.md §10.2, §13.3).
 *
 * Upstream compiled out the reading and writing of default.cfg and
 * doomgenericdoom.cfg (m_config.c's LoadDefaultCollection() and
 * SaveDefaultCollection(), under ORIGCODE), so a change made in Doom's
 * menus never lasted. The patch compiles them back in and includes this,
 * last, in m_config.c alone. Their files come from fopen(), which the link
 * sends to FatFs (upstream's stdio_fatfs.c: a FILE * is a FIL *), but
 * fprintf(), fscanf() and feof() would treat that as newlib's FILE, and
 * the quiet build's frank_doom_quiet_stdio.h makes fprintf() nothing. So
 * here they are FatFs's, for m_config.c only.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_CONFIG_STDIO_H
#define PICOCALC_DOOM_CONFIG_STDIO_H

#include <stdio.h>

#undef fprintf
#undef fscanf
#undef feof
#define fprintf picocalc_config_fprintf
#define fscanf  picocalc_config_fscanf
#define feof    picocalc_config_feof

/* As fprintf(), to a file fopen() opened on the card. Returns the
 * characters written, or a negative number. */
int picocalc_config_fprintf(FILE *f, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

/* As fscanf(), one line at a time: the line is read, then scanned. A line
 * that does not match is used up, so a bad line cannot stop the read.
 * Returns what sscanf() does, or EOF at the end. */
int picocalc_config_fscanf(FILE *f, const char *format, ...)
    __attribute__((format(scanf, 2, 3)));

/* As feof(). */
int picocalc_config_feof(FILE *f);

#endif /* PICOCALC_DOOM_CONFIG_STDIO_H */
