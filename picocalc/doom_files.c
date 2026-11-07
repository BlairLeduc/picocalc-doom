/* doom_files.c — Doom's own files on the card: its configuration's stdio
 * (config_stdio.h) and a save directory for each game (design.md §10.2).
 *
 * Saves: upstream's embedded branch of M_GetSaveGameDir() put every
 * game's in /doom/.savegame/, so a Doom II save showed in Doom's list and
 * loading it there failed or worse. Chocolate Doom keeps a directory for
 * each, named by D_SaveGameIWADName(): by the game's mission, so
 * doom1.wad's and doom.wad's are shared, as are Freedoom: Phase 2's and
 * Doom II's. Wrapped at link time, as no change to upstream (§5.3).
 *
 * FatFs's working memory (ff_memalloc()) is kept from the SDK's malloc
 * when it would not fit, below.
 *
 * A save's write is timed (M8): G_DoSaveGame() starts with
 * P_TempSaveGameFile() and ends, after the file is closed and renamed,
 * with R_FillBackScreen(), both in other files, so both can be wrapped.
 *
 * Core 0 only.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Doom's headers first (doomtype.h's boolean). */
#include "doomtype.h"
#include "d_main.h"
#include "m_misc.h"
#include "frank_doom_log.h"

#include "ff.h"
#include "pico/time.h"

#include "core1.h"

#include "config_stdio.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Static, not on core 0's stack, which has under 1 KB to spare (§12.1).
 * default.cfg's longest line is a name lined up to 30 characters and a
 * value: a key's number, a float, or a short string. */
static char s_line[256];

int picocalc_config_fprintf(FILE *f, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(s_line, sizeof s_line, format, ap);
    va_end(ap);
    if (n < 0) {
        return n;
    }
    UINT len = (size_t)n < sizeof s_line ? (UINT)n : (UINT)(sizeof s_line - 1u);
    UINT wrote = 0;
    if (f_write((FIL *)f, s_line, len, &wrote) != FR_OK || wrote != len) {
        return -1;
    }
    return (int)len;
}

int picocalc_config_fscanf(FILE *f, const char *format, ...)
{
    if (f_gets(s_line, sizeof s_line, (FIL *)f) == NULL) {
        return EOF;
    }
    va_list ap;
    va_start(ap, format);
    int n = vsscanf(s_line, format, ap);
    va_end(ap);
    return n;
}

int picocalc_config_feof(FILE *f)
{
    return f_eof((FIL *)f);
}

/* .savegame/<game>/, made if it is not there. Upstream's returns a
 * string of its own too; Doom keeps it to the end. */
char *__wrap_M_GetSaveGameDir(char *iwadname)
{
    M_MakeDirectory(".savegame");
    char *dir = M_StringJoin(".savegame/", iwadname, NULL);
    M_MakeDirectory(dir);
    free(dir);
    dir = M_StringJoin(".savegame/", iwadname, "/", NULL);
    FRANK_DOOM_LOG("picocalc-doom: saves in /doom/%s\n", dir);
    return dir;
}

/* A save's write, from its temporary file's name to the back screen
 * redrawn after the rename. */
char *__real_P_TempSaveGameFile(void);
void __real_R_FillBackScreen(void);

static uint32_t s_save_t0;
static bool s_saving;

char *__wrap_P_TempSaveGameFile(void)
{
    if (gameaction == ga_savegame) {
        s_save_t0 = time_us_32();
        s_saving = true;
    }
    return __real_P_TempSaveGameFile();
}

void __wrap_R_FillBackScreen(void)
{
    if (s_saving) {
        s_saving = false;
        FRANK_DOOM_LOG("picocalc-doom: saved in %lu ms\n",
                       (unsigned long)((time_us_32() - s_save_t0) / 1000u));
    }
    __real_R_FillBackScreen();
}

/* FatFs's working memory (FF_USE_LFN 3, upstream's ffsystem.c): the
 * long-name buffer on each call that takes a path, freed as it returns;
 * and in f_mkdir(), a buffer of up to 32 KB to clear the new directory's
 * cluster with, which FatFs halves until one is given and, if none is,
 * clears sector by sector. The SDK's malloc() panics rather than return
 * NULL (PICO_MALLOC_PANIC), so that fallback never ran: the first
 * directory the port made, a game's save directory, stopped the program
 * with "Out of memory" (M8). The name buffer goes to malloc() as before;
 * any other request is f_mkdir()'s, and gets NULL unless it fits below
 * the heap's limit with room to spare (§12.1). */
#define NAME_BUF ((FF_MAX_LFN + 1) * 2 + (FF_FS_EXFAT ? (FF_MAX_LFN + 44) / 15 * 32 : 0))
#define HEAP_SPARE 1024u

void *__real_ff_memalloc(UINT msize);

void *__wrap_ff_memalloc(UINT msize)
{
    if (msize != NAME_BUF &&
        (uint32_t)msize + HEAP_SPARE > core0_heap_size() - core0_heap_used()) {
        return NULL;
    }
    return __real_ff_memalloc(msize);
}
