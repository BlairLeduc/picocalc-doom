/* shell_args.c — Doom's arguments from the shell (design.md §13.4, D15).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PICOCALC_DOOM_HOST
/* Doom's headers first (doomtype.h's boolean). */
#include "frank_doom_log.h"

#include "ff.h"
#include "psram_allocator.h"
#endif

#include "shell_args.h"

#include <string.h>
#include <strings.h>

/* The longest path FatFs takes, with its NUL. */
#define PATH_MAX_LEN 256u

typedef enum {
    TAKES_NOTHING,  /* an option whose words, if any, are not files    */
    TAKES_ONE,      /* one file                                        */
    TAKES_MANY,     /* files until the next option, as Doom reads them */
    TAKES_DEMO,     /* a demo: a file, or a lump's name                */
    TAKES_NEW,      /* one file Doom makes, so it need not exist       */
} takes_t;

static takes_t takes(const char *option) {
    static const struct { const char *name; takes_t takes; } options[] = {
        { "-iwad", TAKES_ONE },      { "-record", TAKES_NEW },
        { "-file", TAKES_MANY },     { "-merge", TAKES_MANY },  { "-deh", TAKES_MANY },
        { "-playdemo", TAKES_DEMO }, { "-timedemo", TAKES_DEMO },
    };
    for (size_t i = 0; i < sizeof options / sizeof options[0]; i++) {
        if (strcasecmp(option, options[i].name) == 0) return options[i].takes;
    }
    return TAKES_NOTHING;
}

typedef struct {
    char *buf;
    size_t size, used;
    char **out;
    int max, n, dropped;
} words_t;

/* `prefix` then `s`, as the next word, if it fits. */
static void emit(words_t *w, const char *prefix, const char *s) {
    size_t lp = strlen(prefix), ls = strlen(s);
    if (w->n == w->max || w->used + lp + ls + 1u > w->size) {
        w->dropped++;
        return;
    }
    char *p = w->buf + w->used;
    memcpy(p, prefix, lp);
    memcpy(p + lp, s, ls + 1u);
    w->used += lp + ls + 1u;
    w->out[w->n++] = p;
}

/* A demo is a file when the path, or the path with Doom's .lmp, is one;
 * otherwise it is a lump's name, which Doom looks for in the WADs. */
static bool demo_is_file(const char *path, bool (*exists)(const char *)) {
    if (exists(path)) return true;
    char lmp[PATH_MAX_LEN + 4u];
    size_t n = strlen(path);
    if (n + 5u > sizeof lmp) return false;
    memcpy(lmp, path, n);
    memcpy(lmp + n, ".lmp", 5u);
    return exists(lmp);
}

int shell_args_to_doom(const picocalc_shell_args_t *args, char *buf, size_t size, char **out,
                       int max, bool (*exists)(const char *path), int *dropped) {
    words_t w = { buf, size, 0, out, max, 0, 0 };
    takes_t pending = TAKES_NOTHING;
    for (int i = 0; i < args->argc; i++) {
        const char *word = picocalc_shell_args_word(args, i);
        /* DOS's switch character: /FILE is -FILE. */
        bool option = word[0] == '-' || (args->dos && word[0] == '/');
        if (option) {
            const char *name = word + 1;
            emit(&w, "-", name);
            char dashed[32] = "-";
            strncat(dashed, name, sizeof dashed - 2u);
            pending = takes(dashed);
            continue;
        }
        if (pending == TAKES_NOTHING) {
            emit(&w, "", word);
            continue;
        }
        /* A file not found from the user's directory goes as typed, for
         * Doom to look for from its own, /doom: so `-iwad doom2.wad`
         * works from anywhere. A demo not found is a lump's name. */
        char path[PATH_MAX_LEN];
        bool is_path = picocalc_shell_args_path(args, word, path, sizeof path);
        if (is_path && pending == TAKES_DEMO) {
            is_path = demo_is_file(path, exists);
        } else if (is_path && pending != TAKES_NEW) {
            is_path = exists(path);
        }
        emit(&w, "", is_path ? path : word);
        if (pending != TAKES_MANY) pending = TAKES_NOTHING;
    }
    if (dropped) *dropped = w.dropped;
    return w.n;
}

#ifndef PICOCALC_DOOM_HOST

/* The words for Doom: the paths can be longer than the words typed. */
#define WORDS_BYTES (SHELL_ARGS_MAX * PATH_MAX_LEN)

static char *s_words[SHELL_ARGS_MAX];
static int s_n;

static bool exists(const char *path) {
    FILINFO fi;
    return f_stat(path, &fi) == FR_OK && !(fi.fattrib & AM_DIR);
}

void shell_args_load(void) {
    FIL f;
    if (f_open(&f, PICOCALC_SHELL_ARGS_FILE, FA_READ) != FR_OK) {
        FRANK_DOOM_LOG("  shell        : no arguments (not started by the shell)\n");
        return;
    }
    /* Kept to the end: Doom's myargv points into the second. Both in the
     * PSRAM, as the SRAM heap is short (§12.1). */
    char *text = psram_malloc(PICOCALC_SHELL_ARGS_SIZE);
    char *words = psram_malloc(WORDS_BYTES);
    UINT got = 0;
    FRESULT r = text && words ? f_read(&f, text, PICOCALC_SHELL_ARGS_SIZE, &got) : FR_NOT_ENOUGH_CORE;
    f_close(&f);
    /* Read once: whatever it held, it was for this start (picocalc_shell.h). */
    f_unlink(PICOCALC_SHELL_ARGS_FILE);
    picocalc_shell_args_t args;
    if (r != FR_OK || !picocalc_shell_args_parse(text, got, SHELL_ARGS_NAME, &args)) {
        FRANK_DOOM_LOG("  shell        : %s not for this program, or unreadable (%d, %u bytes); "
                       "ignored\n", PICOCALC_SHELL_ARGS_FILE, (int)r, (unsigned)got);
        return;
    }
    int dropped = 0;
    s_n = shell_args_to_doom(&args, words, WORDS_BYTES, s_words, SHELL_ARGS_MAX, exists, &dropped);
    FRANK_DOOM_LOG("  shell        : %s flavour, in %s, %d word%s", args.dos ? "DOS" : "UNIX",
                   args.dir, args.argc, args.argc == 1 ? "" : "s");
    for (int i = 0; i < s_n; i++) FRANK_DOOM_LOG("%s%s", i == 0 ? ": " : " ", s_words[i]);
    FRANK_DOOM_LOG("\n");
    if (dropped > 0) FRANK_DOOM_LOG("  shell        : %d word(s) dropped, too many\n", dropped);
}

char **shell_args_words(int *n) {
    *n = s_n;
    return s_words;
}

#endif
