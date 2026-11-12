/* test_settings.c — /doom/picocalc.ini's parser (design.md §10.3, §15).
 *
 * The defaults; each setting's values, in any case; a file with bad lines
 * applies the rest and names each bad line, by number and in order, with
 * why (M7's done-when); comments, a BOM, CRLF and a last line with no
 * end; the later of two lines winning, and a bad value leaving the
 * earlier; run = hold with no run_key; the keys refused whatever else is
 * bound: Shift's (§9.3), and those Doom's menu and automap take; and
 * §9.6's bindings: every action and key name, none, a key on two actions
 * against §9.2's keys and another line, swaps in both orders, a refusal
 * that brings back a default another line had taken, the run key against
 * the bindings as the file leaves them, and each key's code against
 * keymap_feed() and m_controls.c's defaults.
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
        { 10, "a key already on strafe_left" },
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
    CHECK(g_e[0].line == 1 && strcmp(g_e[0].why, "run = hold needs a run_key") == 0, "named");
    /* Among other errors, in the file's order. */
    CHECK(parse("bad = 1\nrun = hold\nworse = 2\nrun_key = none\n") == 3, "three");
    CHECK(g_e[0].line == 1 && g_e[1].line == 2 && g_e[2].line == 3,
          "in order: %u %u %u", g_e[0].line, g_e[1].line, g_e[2].line);
    CHECK(strcmp(g_e[1].why, "run = hold needs a run_key") == 0, "the hold line's why");
    /* A run_key line refused names only itself, before or after the run
     * line, for its own reason or for a key already bound; running stays
     * always. */
    static const struct { const char *text; unsigned line; const char *why; } refused[] = {
        { "run = hold\nrun_key = 0\n", 2, "the automap's max zoom key" },
        { "run_key = 0\nrun = hold\n", 1, "the automap's max zoom key" },
        { "run = hold\nrun_key = !\n", 2, "a key typed without Shift" },
        { "run = hold\nrun_key = .\n", 2, "a key already on strafe_right" },
        { "run = hold\nrun_key = z\nrun_key = 1\n", 3, "a key already on weapon1" },
    };
    for (unsigned i = 0; i < sizeof refused / sizeof refused[0]; i++) {
        CHECK(parse(refused[i].text) == 1 && g_e[0].line == refused[i].line &&
                  strcmp(g_e[0].why, refused[i].why) == 0 && g_s.run == SETTINGS_RUN_ALWAYS,
              "%s: %u errors, line %u \"%s\"", refused[i].text, parse(refused[i].text),
              g_e[0].line, g_e[0].why);
    }
    /* A run key with run left at always is kept, and unused. */
    CHECK(parse("run_key = z\n") == 0 && g_s.run == SETTINGS_RUN_ALWAYS && g_s.run_key == 'z',
          "run_key alone");
}

static void test_refused(void) {
    /* Keys typed only with Shift. */
    for (const char *p = "!@#$%^&*()_+{}|:\"<>?~"; *p; p++) {
        CHECK(settings_key_refused((uint8_t)*p) != NULL, "'%c' refused (Shift)", *p);
    }
    /* Keys Doom's menu and the automap take in play. */
    for (const char *p = "-=0fgmcFGMC"; *p; p++) {
        CHECK(settings_key_refused((uint8_t)*p) != NULL, "'%c' refused (Doom's)", *p);
    }
    /* Not printable. */
    CHECK(settings_key_refused(' ') != NULL, "space as a character");
    CHECK(settings_key_refused(MCU_SHIFT_L) != NULL, "Shift itself");
    CHECK(settings_key_refused(MCU_CTRL) != NULL, "Ctrl as a character");
    /* The rest of the letters, digits and punctuation are keys; a capital
     * is its letter. Which are free is the whole file's to say. */
    for (const char *p = "abdehijklnopqrstuvwxyz123456789;'[]\\/`,.Z"; *p; p++) {
        CHECK(settings_key_refused((uint8_t)*p) == NULL, "'%c' a key: %s", *p,
              settings_key_refused((uint8_t)*p));
    }
    CHECK(parse("run_key = zz\n") == 1 && strcmp(g_e[0].why, "one key, or a key's name") == 0,
          "two keys");
    CHECK(parse("fire = shift\n") == 1 && strcmp(g_e[0].why, "one key, or a key's name") == 0,
          "Shift has no name");
    CHECK(parse("fire = enter\n") == 1 && g_s.key[SETTINGS_FIRE] == MCU_CTRL,
          "Enter has no name");
    CHECK(parse("fire = !\n") == 1 && strcmp(g_e[0].why, "a key typed without Shift") == 0,
          "fire on !");
    CHECK(parse("fire = f\n") == 1 && strcmp(g_e[0].why, "the automap's follow key") == 0,
          "fire on f");
    CHECK(parse("fire = -\n") == 1 && strcmp(g_e[0].why, "Doom's screen size key") == 0,
          "fire on -");
    CHECK(parse("jump = z\n") == 1 && strcmp(g_e[0].why, "no such setting") == 0, "no jump");
}

static void test_run_key(void) {
    /* The run key against each action's key in §9.2. */
    char text[32];
    for (unsigned a = 0; a < SETTINGS_ACTIONS; a++) {
        uint8_t k = settings_default_key(a);
        if (k == 0) continue;
        char name[2];
        snprintf(text, sizeof text, "run_key = %s\n", settings_key_name(k, name));
        char want[48];
        snprintf(want, sizeof want, "a key already on %s", settings_action_name(a));
        CHECK(parse(text) == 1 && strcmp(g_e[0].why, want) == 0 && g_s.run_key == 0,
              "%s: \"%s\", want \"%s\"", text, g_e[0].why, want);
    }
    CHECK(parse("run_key = z\nrun_key = y\n") == 0 && g_s.run_key == 'y', "y, free in play");
    /* Against the bindings as the file leaves them. */
    CHECK(parse("weapon1 = none\nrun = hold\nrun_key = 1\n") == 0 && g_s.run_key == '1' &&
              g_s.run == SETTINGS_RUN_HOLD, "1 freed for running");
    CHECK(parse("fire = z\nrun = hold\nrun_key = ctrl\n") == 0 && g_s.run_key == MCU_CTRL,
          "Ctrl freed for running");
    CHECK(parse("run = hold\nrun_key = z\nfire = z\n") == 1 && g_e[0].line == 3 &&
              strcmp(g_e[0].why, "a key already on run_key") == 0 &&
              g_s.key[SETTINGS_FIRE] == MCU_CTRL && g_s.run_key == 'z', "a binding after");
}

static void test_bindings(void) {
    /* §9.2's keys, each different. */
    CHECK(parse("") == 0, "empty");
    for (unsigned a = 0; a < SETTINGS_ACTIONS; a++) {
        CHECK(g_s.key[a] == settings_default_key(a), "%s at its default",
              settings_action_name(a));
        for (unsigned b = a + 1; b < SETTINGS_ACTIONS; b++) {
            CHECK(g_s.key[a] == 0 || g_s.key[a] != g_s.key[b], "%s and %s share",
                  settings_action_name(a), settings_action_name(b));
        }
    }

    /* Every action takes a free key, its name in any case. */
    char text[64];
    for (unsigned a = 0; a < SETTINGS_ACTIONS; a++) {
        char upper[24];
        const char *n = settings_action_name(a);
        size_t i = 0;
        for (; n[i] && i < sizeof upper - 1; i++) {
            upper[i] = n[i] >= 'a' && n[i] <= 'z' ? (char)(n[i] - 32) : n[i];
        }
        upper[i] = '\0';
        snprintf(text, sizeof text, "%s = z\n", a % 2 ? n : upper);
        CHECK(parse(text) == 0 && g_s.key[a] == 'z', "%s", text);
    }

    /* Every key name: free (Del), or some action's in §9.2. */
    static const struct { const char *name; uint8_t key; } names[] = {
        { "space", ' ' }, { "CTRL", MCU_CTRL }, { "Alt", MCU_ALT }, { "up", MCU_UP },
        { "down", MCU_DOWN }, { "left", MCU_LEFT }, { "right", MCU_RIGHT }, { "tab", MCU_TAB },
        { "del", MCU_DEL },
    };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        unsigned owner = SETTINGS_ACTIONS;
        for (unsigned a = 0; a < SETTINGS_ACTIONS; a++) {
            if (settings_default_key(a) == names[i].key) owner = a;
        }
        snprintf(text, sizeof text, "next_weapon = %s\n", names[i].name);
        if (owner == SETTINGS_ACTIONS) {
            CHECK(parse(text) == 0 && g_s.key[SETTINGS_NEXT_WEAPON] == names[i].key, "%s", text);
        } else {
            snprintf(text + strlen(text), sizeof text - strlen(text), "%s = none\n",
                     settings_action_name(owner));
            CHECK(parse(text) == 0 && g_s.key[SETTINGS_NEXT_WEAPON] == names[i].key &&
                      g_s.key[owner] == 0, "%s", text);
        }
    }

    /* none frees a key. */
    CHECK(parse("weapon8 = none\nnext_weapon = 8\n") == 0 && g_s.key[SETTINGS_WEAPON8] == 0 &&
              g_s.key[SETTINGS_NEXT_WEAPON] == '8', "weapon8's key to next_weapon");

    /* A swap, in either order. */
    CHECK(parse("fire = space\nuse = ctrl\n") == 0 && g_s.key[SETTINGS_FIRE] == ' ' &&
              g_s.key[SETTINGS_USE] == MCU_CTRL, "swap");
    CHECK(parse("use = ctrl\nfire = space\n") == 0 && g_s.key[SETTINGS_FIRE] == ' ' &&
              g_s.key[SETTINGS_USE] == MCU_CTRL, "swap, the other order");

    /* Against §9.2's key of an action the file leaves alone. */
    CHECK(parse("fire = 1\n") == 1 && g_e[0].line == 1 &&
              strcmp(g_e[0].why, "a key already on weapon1") == 0 &&
              g_s.key[SETTINGS_FIRE] == MCU_CTRL, "fire on weapon1's key");
    /* Against another line: the later is not used. */
    CHECK(parse("fire = z\nuse = z\n") == 1 && g_e[0].line == 2 &&
              strcmp(g_e[0].why, "a key already on fire") == 0 &&
              g_s.key[SETTINGS_FIRE] == 'z' && g_s.key[SETTINGS_USE] == ' ', "two on z");
    /* A refusal brings back a default another line took. */
    CHECK(parse("use = ctrl\nfire = 1\n") == 2, "cascade: two");
    CHECK(g_e[0].line == 1 && strcmp(g_e[0].why, "a key already on fire") == 0 &&
              g_e[1].line == 2 && strcmp(g_e[1].why, "a key already on weapon1") == 0,
          "cascade: in order, %u %s, %u %s", g_e[0].line, g_e[0].why, g_e[1].line, g_e[1].why);
    CHECK(g_s.key[SETTINGS_FIRE] == MCU_CTRL && g_s.key[SETTINGS_USE] == ' ', "cascade: §9.2");
    /* The same action twice, the later on a taken key: back to §9.2's. */
    CHECK(parse("fire = z\nfire = 1\n") == 1 && g_e[0].line == 2 &&
              g_s.key[SETTINGS_FIRE] == MCU_CTRL, "fire twice");
    /* Among other errors, in the file's order. */
    CHECK(parse("fire = 1\nbad = 2\n") == 2 && g_e[0].line == 1 && g_e[1].line == 2,
          "in order: %u %u", g_e[0].line, g_e[1].line);
    CHECK(g_e[0].len == strlen("fire = 1"), "the line found");
}

/* Each key the file can name gives, pressed alone, the code its binding
 * stores; and §9.2's keys are m_controls.c's defaults, so a file that
 * binds nothing leaves Doom as it was. */
static void test_codes(void) {
    static keymap_t km;
    km_event_t out[KM_EVENTS_MAX];
    unsigned n;
    static const uint8_t named[] = { ' ', MCU_CTRL, MCU_ALT, MCU_UP, MCU_DOWN, MCU_LEFT,
                                     MCU_RIGHT, MCU_TAB, MCU_DEL };
    for (unsigned i = 0; i < sizeof named; i++) {
        keymap_reset(&km);
        CHECK(keymap_feed(&km, MCU_PRESSED, named[i], out, &n) == KM_EVENT && n == 1 &&
                  out[0].key == keymap_play_code(named[i]) && out[0].key != 0,
              "0x%02x: 0x%02x", named[i], keymap_play_code(named[i]));
    }
    for (unsigned c = 0x21; c < 0x7F; c++) {
        if (settings_key_refused((uint8_t)c) != NULL) continue;
        uint8_t phys = (uint8_t)(c >= 'A' && c <= 'Z' ? c | 0x20 : c);
        keymap_reset(&km);
        CHECK(keymap_feed(&km, MCU_PRESSED, (uint8_t)c, out, &n) == KM_EVENT && n == 1 &&
                  out[0].key == keymap_play_code(phys) && out[0].key != 0,
              "'%c': 0x%02x", c, keymap_play_code(phys));
    }
    CHECK(keymap_play_code(0) == 0, "none is 0");

    static const uint8_t doom[SETTINGS_ACTIONS] = {
        [SETTINGS_FORWARD] = DK_UPARROW,       [SETTINGS_BACK] = DK_DOWNARROW,
        [SETTINGS_TURN_LEFT] = DK_LEFTARROW,   [SETTINGS_TURN_RIGHT] = DK_RIGHTARROW,
        [SETTINGS_STRAFE_LEFT] = DK_STRAFE_L,  [SETTINGS_STRAFE_RIGHT] = DK_STRAFE_R,
        [SETTINGS_STRAFE] = DK_RALT,           [SETTINGS_FIRE] = DK_FIRE,
        [SETTINGS_USE] = DK_USE,
        [SETTINGS_WEAPON1 + 0] = '1', [SETTINGS_WEAPON1 + 1] = '2', [SETTINGS_WEAPON1 + 2] = '3',
        [SETTINGS_WEAPON1 + 3] = '4', [SETTINGS_WEAPON1 + 4] = '5', [SETTINGS_WEAPON1 + 5] = '6',
        [SETTINGS_WEAPON1 + 6] = '7', [SETTINGS_WEAPON8] = '8',
        [SETTINGS_PREV_WEAPON] = 0,            [SETTINGS_NEXT_WEAPON] = 0,
        [SETTINGS_AUTOMAP] = DK_TAB,
    };
    for (unsigned a = 0; a < SETTINGS_ACTIONS; a++) {
        CHECK(keymap_play_code(settings_default_key(a)) == doom[a], "%s: 0x%02x, Doom's 0x%02x",
              settings_action_name(a), keymap_play_code(settings_default_key(a)), doom[a]);
    }
    /* And §9.2's table in the key map agrees. */
    for (unsigned i = 0; i < keymap_binding_count; i++) {
        const km_binding_t *b = &keymap_bindings[i];
        if (b->shift || b->alt) continue;
        CHECK(keymap_play_code(b->phys) == b->key, "%s: 0x%02x, the table's 0x%02x", b->what,
              keymap_play_code(b->phys), b->key);
    }
}

int main(void) {
    test_defaults();
    test_values();
    test_bad_lines();
    test_later_wins();
    test_hold();
    test_refused();
    test_run_key();
    test_bindings();
    test_codes();
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_settings: ok\n");
    return 0;
}
