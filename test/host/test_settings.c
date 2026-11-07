/* test_settings.c — /doom/picocalc.ini's parser (design.md §10.3, §15).
 *
 * The defaults; each setting's values, in any case; a file with bad lines
 * applies the rest and names each bad line, by number and in order, with
 * why (M7's done-when); comments, a BOM, CRLF and a last line with no
 * end; the later of two lines winning, and a bad value leaving the
 * earlier; run = hold with no run_key; and the run key against every key
 * the key map binds, every key Doom uses, and every key typed only with
 * Shift (§9.3), so no run key can collide with a binding.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <string.h>

#include "keymap.h"
#include "settings.h"

static int failures;

#define CHECK(cond, ...)                                         \
    do {                                                         \
        if (!(cond)) {                                           \
            failures++;                                          \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                        \
            fputc('\n', stderr);                                 \
        }                                                        \
    } while (0)

#define ERRORS 8u

static settings_t g_s;
static settings_error_t g_e[ERRORS];

static unsigned parse(const char *text) {
    settings_defaults(&g_s);
    memset(g_e, 0, sizeof g_e);
    return settings_parse(&g_s, text, strlen(text), g_e, ERRORS);
}

static void test_defaults(void) {
    CHECK(parse("") == 0, "an empty file");
    CHECK(g_s.iwad[0] == '\0' && g_s.iwad_line == 0, "no iwad");
    CHECK(g_s.args[0] == '\0', "no args");
    CHECK(g_s.run == SETTINGS_RUN_ALWAYS, "running always (D12)");
    CHECK(g_s.run_key == 0, "no run key");
    CHECK(parse("# nothing\n; nothing\n\n   \n\t# indented\n") == 0, "comments only");
}

static void test_values(void) {
    CHECK(parse("iwad = doom2.wad\n") == 0 && strcmp(g_s.iwad, "doom2.wad") == 0, "iwad");
    CHECK(g_s.iwad_line == 1, "iwad's line");
    CHECK(parse("IWAD=PLUTONIA.WAD") == 0 && strcmp(g_s.iwad, "plutonia.wad") == 0,
          "iwad in capitals, no spaces, no line end");
    CHECK(parse("iwad = freedoom1.wad # the free one\n") == 0 &&
              strcmp(g_s.iwad, "freedoom1.wad") == 0, "iwad with a comment");

    CHECK(parse("args = -file sigil.wad -skill 3\n") == 0 &&
              strcmp(g_s.args, "-file sigil.wad -skill 3") == 0, "args");
    CHECK(parse("args = -warp\t1 1 ; first map\n") == 0 && strcmp(g_s.args, "-warp 1 1") == 0,
          "args: a tab is a space; a comment cut");
    CHECK(parse("Args = -File Sigil.WAD\n") == 0 && strcmp(g_s.args, "-File Sigil.WAD") == 0,
          "args keep their case");

    static const struct { const char *text; unsigned run; } runs[] = {
        { "run = always", SETTINGS_RUN_ALWAYS }, { "run = OFF", SETTINGS_RUN_OFF },
        { "run = Hold\nrun_key = z", SETTINGS_RUN_HOLD },
        { "run_key = Z\nrun = hold", SETTINGS_RUN_HOLD },
    };
    for (unsigned i = 0; i < sizeof runs / sizeof runs[0]; i++) {
        CHECK(parse(runs[i].text) == 0 && g_s.run == runs[i].run, "%s", runs[i].text);
    }
    CHECK(g_s.run_key == 'z', "run_key Z is z");
    CHECK(strcmp(settings_run_name(SETTINGS_RUN_ALWAYS), "always") == 0 &&
              strcmp(settings_run_name(SETTINGS_RUN_HOLD), "hold") == 0 &&
              strcmp(settings_run_name(SETTINGS_RUN_OFF), "off") == 0, "run names");
}

static void test_bad_lines(void) {
    /* M7's done-when: a bad line is named, and the rest applies. */
    const char *text =
        "\xEF\xBB\xBF# picocalc.ini\r\n"   /* 1 */
        "iwad = doom2.wad\r\n"             /* 2 */
        "volume = 8\r\n"                   /* 3: no such setting */
        "run = sometimes\r\n"              /* 4: not a run mode */
        "run_key\r\n"                      /* 5: no = */
        " = z\r\n"                         /* 6: no name */
        "args =\r\n"                       /* 7: no value */
        "iwad = /doom/tnt.wad\r\n"         /* 8: a folder */
        "iwad = tnt\r\n"                   /* 9: not .wad */
        "run_key = ,\r\n"                  /* 10: Doom's strafe */
        "run = off\r\n"                    /* 11 */
        "args = -skill 4";                 /* 12, no line end */
    unsigned n = parse(text);
    static const struct { unsigned line; const char *why; } want[] = {
        { 3, "no such setting" },
        { 4, "always, hold or off" },
        { 5, "no = in it" },
        { 6, "no name before the =" },
        { 7, "no value after the =" },
        { 8, "a file in /doom, with no folder" },
        { 9, "a file name ending .wad" },
        { 10, "a key Doom already uses" },
    };
    CHECK(n == 8, "eight lines not used, got %u", n);
    for (unsigned i = 0; i < sizeof want / sizeof want[0] && i < n; i++) {
        CHECK(g_e[i].line == want[i].line, "error %u: line %u, want %u", i, g_e[i].line,
              want[i].line);
        CHECK(strcmp(g_e[i].why, want[i].why) == 0, "line %u: \"%s\", want \"%s\"", g_e[i].line,
              g_e[i].why, want[i].why);
    }
    /* Where each is in the text, without its CR. */
    CHECK(g_e[0].len == strlen("volume = 8") && memcmp(text + g_e[0].at, "volume = 8", g_e[0].len) == 0,
          "line 3 found in the text");
    /* The rest applied; the bad iwads left the good one. */
    CHECK(strcmp(g_s.iwad, "doom2.wad") == 0 && g_s.iwad_line == 2, "iwad from line 2");
    CHECK(g_s.run == SETTINGS_RUN_OFF, "run from line 11");
    CHECK(strcmp(g_s.args, "-skill 4") == 0, "args from the last line");

    /* More than fit: counted, the first described. */
    char many[512] = "";
    for (int i = 0; i < 12; i++) strcat(many, "nonsense = 1\n");
    CHECK(parse(many) == 12, "twelve counted");
    CHECK(g_e[ERRORS - 1].line == ERRORS, "the first eight described");
}

static void test_later_wins(void) {
    CHECK(parse("iwad = doom.wad\niwad = tnt.wad\n") == 0 && strcmp(g_s.iwad, "tnt.wad") == 0 &&
              g_s.iwad_line == 2, "the later iwad");
    CHECK(parse("run = off\nrun = bananas\n") == 1 && g_s.run == SETTINGS_RUN_OFF,
          "a bad run leaves the earlier");
    char longer[SETTINGS_ARGS_MAX + 16] = "args = ";
    memset(longer + 7, 'x', SETTINGS_ARGS_MAX);
    longer[7 + SETTINGS_ARGS_MAX] = '\0';
    CHECK(parse(longer) == 1 && g_s.args[0] == '\0', "args too long");
    CHECK(parse("iwad = abcdefghijklmnop.wad") == 1 && g_s.iwad[0] == '\0', "iwad too long");
}

static void test_hold(void) {
    /* Hold with nothing to hold: the run line is named, running stays
     * always. */
    CHECK(parse("run = hold\n") == 1, "hold alone");
    CHECK(g_s.run == SETTINGS_RUN_ALWAYS, "falls back to always");
    CHECK(g_e[0].line == 1 && strcmp(g_e[0].why, "hold, with no run_key") == 0, "named");
    /* Among other errors, in the file's order. */
    CHECK(parse("bad = 1\nrun = hold\nworse = 2\nrun_key = .\n") == 4, "four");
    CHECK(g_e[0].line == 1 && g_e[1].line == 2 && g_e[2].line == 3 && g_e[3].line == 4,
          "in order: %u %u %u %u", g_e[0].line, g_e[1].line, g_e[2].line, g_e[3].line);
    CHECK(strcmp(g_e[1].why, "hold, with no run_key") == 0, "the hold line's why");
    /* A run key with run left at always is kept, and unused. */
    CHECK(parse("run_key = z\n") == 0 && g_s.run == SETTINGS_RUN_ALWAYS && g_s.run_key == 'z',
          "run_key alone");
}

static void test_run_key(void) {
    /* Every key the key map binds without Shift is refused. */
    for (unsigned i = 0; i < keymap_binding_count; i++) {
        const km_binding_t *b = &keymap_bindings[i];
        if (b->shift || b->phys <= ' ' || b->phys >= 0x7Fu) continue;
        CHECK(settings_run_key_refused(b->phys) != NULL, "'%c' (%s) refused", b->phys, b->what);
    }
    /* And the keys Doom uses itself (m_controls.c). */
    for (const char *p = "8-=0fgmcyn1234567,."; *p; p++) {
        CHECK(settings_run_key_refused((uint8_t)*p) != NULL, "'%c' refused", *p);
    }
    /* Keys typed only with Shift. */
    for (const char *p = "!@#$%^&*()_+{}|:\"<>?~"; *p; p++) {
        CHECK(settings_run_key_refused((uint8_t)*p) != NULL, "'%c' refused (Shift)", *p);
    }
    /* Not printable. */
    CHECK(settings_run_key_refused(' ') != NULL, "space");
    CHECK(settings_run_key_refused(MCU_SHIFT_L) != NULL, "Shift itself");
    CHECK(settings_run_key_refused(MCU_CTRL) != NULL, "Ctrl");
    /* The rest of the letters, and the free punctuation, are taken; a
     * capital is its letter. */
    unsigned free_keys = 0;
    for (const char *p = "abdehijklopqrstuvwxz9;'[]\\/`"; *p; p++) {
        CHECK(settings_run_key_refused((uint8_t)*p) == NULL, "'%c' allowed: %s", *p,
              settings_run_key_refused((uint8_t)*p));
        free_keys++;
    }
    CHECK(settings_run_key_refused('Z') == NULL, "'Z' allowed");
    CHECK(parse("run_key = zz\n") == 1 && strcmp(g_e[0].why, "one key") == 0, "two keys");
    (void)free_keys;
}

int main(void) {
    test_defaults();
    test_values();
    test_bad_lines();
    test_later_wins();
    test_hold();
    test_run_key();
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_settings: ok\n");
    return 0;
}
