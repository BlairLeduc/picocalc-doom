/* test_config_file.c — Doom's configuration files through the patched
 * m_config.c (design.md §13.3, §15).
 *
 * Upstream compiled out LoadDefaultCollection() and
 * SaveDefaultCollection(); the patch compiles them back in with
 * PICOCALC_DOOM_CONFIG_FILES, and their fprintf(), fscanf() and feof() are
 * the port's (config_stdio.h). Here those three are over the host's stdio,
 * as the firmware's are over FatFs: a line read, then scanned. The test
 * saves Doom's defaults, changes, loads and compares; reads a file with
 * CRLF, a bad line, an unknown name and no last line end; and shows the
 * four bindings default.cfg cannot hold, which dg_picocalc.c's
 * keep_bindings() puts back.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Doom's headers first (doomtype.h's boolean). */
#include "doomkeys.h"
#include "doomtype.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_stdio.h"

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

/* config_stdio.h renames fprintf; the test's own go to the host's. */
#undef fprintf

/* The port's three, over the host's stdio as doom_files.c's are over
 * FatFs. */
static char s_line[256];

int picocalc_config_fprintf(FILE *f, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(s_line, sizeof s_line, format, ap);
    va_end(ap);
    if (n < 0) return n;
    size_t len = (size_t)n < sizeof s_line ? (size_t)n : sizeof s_line - 1u;
    return fwrite(s_line, 1, len, f) == len ? (int)len : -1;
}

int picocalc_config_fscanf(FILE *f, const char *format, ...) {
    if (fgets(s_line, sizeof s_line, f) == NULL) return EOF;
    va_list ap;
    va_start(ap, format);
    int n = vsscanf(s_line, format, ap);
    va_end(ap);
    return n;
}

int picocalc_config_feof(FILE *f) {
    /* f_eof() is true once the last byte is read; stdio's only after a
     * read past it. Peek, so both stop at the same line. */
    int c = getc(f);
    if (c == EOF) return 1;
    ungetc(c, f);
    return 0;
}

/* What m_config.c and m_misc.c call that the test does not link. */
void I_Error(char *error, ...) {
    va_list ap;
    va_start(ap, error);
    vfprintf(stderr, error, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void *Z_Malloc(int size, int tag, void *user) {
    (void)tag;
    (void)user;
    return malloc((size_t)size);
}

void Z_Free(void *p) {
    free(p);
}

static int s_screenblocks = 10, s_sfx_volume = 8, s_show_endoom = 1;
static float s_accel = 2.0f;
static char *s_chat = "hello";

#define DIR "test_config_file.d/"

static void bind_all(void) {
    M_BindBaseControls();
    M_BindWeaponControls();
    M_BindMapControls();
    M_BindMenuControls();
    M_BindVariable("screenblocks", &s_screenblocks);
    M_BindVariable("sfx_volume", &s_sfx_volume);
    M_BindVariable("show_endoom", &s_show_endoom);
    M_BindVariable("mouse_acceleration", &s_accel);
    M_BindVariable("chatmacro0", &s_chat);
}

static void write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    fputs(text, f);
    fclose(f);
}

int main(void) {
    static char *argv[] = { "test", NULL };
    myargc = 1;
    myargv = argv;
    M_SetConfigDir(DIR);
    M_SetConfigFilenames("default.cfg", "doomgenericdoom.cfg");
    remove(DIR "default.cfg");
    remove(DIR "doomgenericdoom.cfg");
    bind_all();

    /* No files: the defaults stand, and saving writes both. */
    M_LoadDefaults();
    CHECK(key_fire == KEY_FIRE && key_up == KEY_UPARROW, "the defaults");
    s_screenblocks = 7;
    s_sfx_volume = 3;
    s_show_endoom = 0;
    s_accel = 1.5f;
    s_chat = "gg";
    key_weapon1 = 'z';
    key_speed = KEY_RSHIFT;
    M_SaveDefaults();
    FILE *f = fopen(DIR "default.cfg", "rb");
    CHECK(f != NULL, "default.cfg written");
    if (f) fclose(f);
    f = fopen(DIR "doomgenericdoom.cfg", "rb");
    CHECK(f != NULL, "doomgenericdoom.cfg written");
    if (f) fclose(f);

    /* Changed after the save, then loaded: the saved values return. */
    s_screenblocks = 11;
    s_sfx_volume = 15;
    s_show_endoom = 1;
    s_accel = 9.0f;
    s_chat = "x";
    key_weapon1 = '1';
    key_up = 'w';
    M_LoadDefaults();
    CHECK(s_screenblocks == 7, "screenblocks %d", s_screenblocks);
    CHECK(s_sfx_volume == 3, "sfx_volume %d", s_sfx_volume);
    CHECK(s_show_endoom == 0, "show_endoom %d", s_show_endoom);
    CHECK(s_accel > 1.49f && s_accel < 1.51f, "mouse_acceleration %f", (double)s_accel);
    CHECK(strcmp(s_chat, "gg") == 0, "chatmacro0 \"%s\"", s_chat);
    CHECK(key_weapon1 == 'z', "key_weapon1 %d", key_weapon1);
    CHECK(key_up == KEY_UPARROW, "key_up %d", key_up);
    CHECK(key_speed == KEY_RSHIFT, "key_speed %d", key_speed);
    CHECK(key_menu_activate == KEY_ESCAPE && key_menu_quit == KEY_F10, "menu keys");

    /* doomgeneric's own codes are no scan codes: saved as 160 to 163,
     * read back as 0, unbound. dg_picocalc.c's keep_bindings() is why. */
    CHECK(key_fire == 0 && key_use == 0 && key_strafeleft == 0 && key_straferight == 0,
          "the four come back unbound: %d %d %d %d", key_fire, key_use, key_strafeleft,
          key_straferight);

    /* A file written by hand: CRLF, a line with no value, a name Doom does
     * not know, a quoted string, and no line end at the last. */
    write_file(DIR "default.cfg",
               "screenblocks 9\r\n"
               "sfx_volume\r\n"
               "no_such_setting 4\r\n"
               "chatmacro0 \"a b\"\r\n"
               "\r\n"
               "key_up 17");
    M_LoadDefaults();
    CHECK(s_screenblocks == 9, "CRLF: screenblocks %d", s_screenblocks);
    CHECK(s_sfx_volume == 3, "a line with no value changes nothing: %d", s_sfx_volume);
    CHECK(strcmp(s_chat, "a b") == 0, "quoted: \"%s\"", s_chat);
    CHECK(key_up == 'w', "no last line end: key_up %d", key_up);

    remove(DIR "default.cfg");
    remove(DIR "doomgenericdoom.cfg");
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_config_file: ok\n");
    return 0;
}
