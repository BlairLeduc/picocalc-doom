/* test_shell_args.c — Doom's arguments from the shell (design.md §13.4, §15).
 *
 * The shell's file through picocalc_shell.h's parser, then the port's
 * conversion: words passed as typed; files after -iwad, -file, -merge,
 * -deh, -record made full paths from the user's directory, every one after
 * -file and only one after -iwad; a demo made a path only when it is a
 * file, with or without .lmp, and left as a lump's name otherwise; the DOS
 * flavour's switches as Doom's options, its paths by its rules, and a
 * drive it refuses left as typed; too many words dropped and counted.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <string.h>

#include "shell_args.h"

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

/* The card's files, for exists(). */
static const char *const s_files[] = {
    "/home/sigil.wad", "/home/mine.lmp", "/doom/doom2.wad", "/wads/a.wad", "/wads/b.wad",
};

static bool exists(const char *path) {
    for (size_t i = 0; i < sizeof s_files / sizeof s_files[0]; i++) {
        if (strcmp(path, s_files[i]) == 0) return true;
    }
    return false;
}

static char s_text[PICOCALC_SHELL_ARGS_SIZE];
static char s_buf[4096];
static char *s_out[SHELL_ARGS_MAX];
static int s_dropped;

/* The shell's file for `flavour` in `dir`, then the words, as Doom's;
 * the joined result in `joined`. Returns how many, or -1 if the parse
 * refused the file. */
static int run(const char *flavour, const char *dir, const char *words, char *joined,
               size_t size) {
    int len = snprintf(s_text, sizeof s_text, "picocalc-args 1\n%s\n%s\n/GAMES/DOOM.UF2\n%s\n%s",
                       flavour, dir, SHELL_ARGS_NAME, words);
    picocalc_shell_args_t args;
    if (!picocalc_shell_args_parse(s_text, (size_t)len, SHELL_ARGS_NAME, &args)) return -1;
    int n = shell_args_to_doom(&args, s_buf, sizeof s_buf, s_out, SHELL_ARGS_MAX, exists,
                               &s_dropped);
    joined[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (i) strncat(joined, " ", size - strlen(joined) - 1);
        strncat(joined, s_out[i], size - strlen(joined) - 1);
    }
    return n;
}

static void expect(const char *flavour, const char *dir, const char *words, const char *want) {
    char got[2048];
    int n = run(flavour, dir, words, got, sizeof got);
    CHECK(n >= 0, "parsed: %s", words);
    CHECK(strcmp(got, want) == 0, "%s in %s:\n  want \"%s\"\n  got  \"%s\"", flavour, dir, want,
          got);
}

static void test_unix(void) {
    expect("unix", "/home", "", "");
    expect("unix", "/home", "-warp\n1\n1\n", "-warp 1 1");
    expect("unix", "/home", "-file\nsigil.wad\n-warp\n1\n1\n",
           "-file /home/sigil.wad -warp 1 1");
    expect("unix", "/", "-file\nwads/a.wad\n/wads/b.wad\n-skill\n4\n",
           "-file /wads/a.wad /wads/b.wad -skill 4");
    expect("unix", "/home/sub", "-iwad\n../../doom/doom2.wad\nnot-a-file\n",
           "-iwad /doom/doom2.wad not-a-file");
    expect("unix", "/home", "-merge\nsigil.wad\n-deh\nmine.lmp\n../wads/a.wad\n",
           "-merge /home/sigil.wad -deh /home/mine.lmp /wads/a.wad");
    /* Not found from the user's directory: as typed, for Doom to find in
     * /doom, its own. */
    expect("unix", "/home", "-iwad\ndoom2.wad\n-file\nnerve.wad\nsigil.wad\n",
           "-iwad doom2.wad -file nerve.wad /home/sigil.wad");
    expect("unix", "/home", "-record\nrun1\n", "-record /home/run1");
    expect("unix", "/home", "-FILE\nsigil.wad\n", "-FILE /home/sigil.wad");
    /* A demo: a file with or without .lmp, or a lump's name. */
    expect("unix", "/home", "-playdemo\nmine\n", "-playdemo /home/mine");
    expect("unix", "/home", "-timedemo\nmine.lmp\n", "-timedemo /home/mine.lmp");
    expect("unix", "/home", "-timedemo\ndemo1\n-nodraw\n", "-timedemo demo1 -nodraw");
    /* A word that is not a file stays a word. */
    expect("unix", "/home", "-skill\n3\n-file\n", "-skill 3 -file");
}

static void test_dos(void) {
    expect("dos", "/home", "/FILE\nSIGIL.WAD\n/WARP\n1\n1\n",
           "-FILE SIGIL.WAD -WARP 1 1");
    expect("dos", "/home", "/FILE\nsigil.wad\n", "-FILE /home/sigil.wad");
    expect("dos", "/home", "/file\n..\\doom\\doom2.wad\n", "-file /doom/doom2.wad");
    expect("dos", "/home", "/IWAD\nC:\\doom\\doom2.wad\n", "-IWAD /doom/doom2.wad");
    /* Another drive cannot be a path, so the word goes as typed. */
    expect("dos", "/HOME", "/FILE\nD:\\X.WAD\n", "-FILE D:\\X.WAD");
    /* '-' is Doom's own, in either flavour. */
    expect("dos", "/HOME", "-warp\n2\n", "-warp 2");
}

static void test_limits(void) {
    char words[1024] = "", got[4096];
    for (int i = 0; i < SHELL_ARGS_MAX + 5; i++) strcat(words, "9\n");
    CHECK(run("unix", "/home", words, got, sizeof got) == SHELL_ARGS_MAX, "max words");
    CHECK(s_dropped == 5, "five dropped, %d", s_dropped);

    /* Room: a buffer too small for the next word drops it. */
    picocalc_shell_args_t args;
    int len = snprintf(s_text, sizeof s_text,
                       "picocalc-args 1\nunix\n/home\n/D.UF2\n%s\nabcdef\nxyz\n", SHELL_ARGS_NAME);
    CHECK(picocalc_shell_args_parse(s_text, (size_t)len, SHELL_ARGS_NAME, &args), "parse");
    char small[9];
    int dropped = 0;
    int n = shell_args_to_doom(&args, small, sizeof small, s_out, SHELL_ARGS_MAX, exists,
                               &dropped);
    CHECK(n == 1 && strcmp(s_out[0], "abcdef") == 0 && dropped == 1, "room for one");

    /* Another program's file is refused by the header's parser. */
    len = snprintf(s_text, sizeof s_text, "picocalc-args 1\nunix\n/home\n/O.UF2\npico-oric\n");
    CHECK(!picocalc_shell_args_parse(s_text, (size_t)len, SHELL_ARGS_NAME, &args),
          "another program's");
}

int main(void) {
    test_unix();
    test_dos();
    test_limits();
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_shell_args: ok\n");
    return 0;
}
