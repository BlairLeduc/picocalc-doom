/* test_keymap.c — the southbridge's key events to Doom's (design.md
 * §9.2, §9.3, §15).
 *
 * The set of codes the MCU can send is written out here again from its
 * keymap (Code/picocalc_keyboard/keyboard.ino at clockworkpi/PicoCalc
 * f915198), apart from keymap.c's tables, and every code is checked: the
 * MCU's map to exactly one physical key and give a down and a matching
 * up; every other code is refused. Then the retranslated releases, the
 * Shift trap's sequences, Insert under Alt, stray releases and repeats,
 * the save-name mode, and every binding of §9.2 reached the way the MCU
 * would send it, with none needing a chord the MCU swallows (HW §6.3)
 * and no key ever giving Doom its run key, Shift.
 *
 * From picocalc-quest's test_keymap.c (D8).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <string.h>

#include "keymap.h"

static int failures;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (!(cond)) {                                      \
            failures++;                                     \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                   \
            fputc('\n', stderr);                            \
        }                                                   \
    } while (0)

/* Each key of the MCU's kbd_entries and btn_entries: the code it sends,
 * and the code it sends with Shift (0 if Shift gives nothing or the
 * same, or for a letter, its capital). Modifiers are listed apart. */
static const struct { uint8_t chr, symb; } s_keys[] = {
    { 0x85, 0x90 }, { 0x84, 0x89 }, { 0x83, 0x88 }, { 0x82, 0x87 }, { 0x81, 0x86 },
    { '`', '~' },   { '3', '#' },   { '2', '@' },
    { 0x08, 0 },    { 0xD4, 0xD5 }, { 0xC1, 0 },    { 0x09, 0xD2 }, { 0xB1, 0xD0 },
    { '4', '$' },   { 'e', 'E' },   { 'w', 'W' },
    { 'p', 'P' },   { '=', '+' },   { '-', '_' },   { '\\', '|' },  { '/', '?' },
    { 'r', 'R' },   { 's', 'S' },   { '1', '!' },
    { 0x0A, 0xD1 }, { '8', '*' },   { '7', '&' },   { '6', '^' },   { '5', '%' },
    { 'f', 'F' },   { 'x', 'X' },   { 'q', 'Q' },
    { '.', '>' },   { 'i', 'I' },   { 'u', 'U' },   { 'y', 'Y' },   { 't', 'T' },
    { 'v', 'V' },   { ';', ':' },   { 'a', 'A' },
    { 'l', 'L' },   { 'k', 'K' },   { 'j', 'J' },   { 'h', 'H' },   { 'g', 'G' },
    { 'c', 'C' },   { '\'', '"' },  { 'z', 'Z' },
    { 'o', 'O' },   { ',', '<' },   { 'm', 'M' },   { 'n', 'N' },   { 'b', 'B' },
    { 'd', 'D' },   { ' ', 0 },
    { '0', ')' },   { '9', '(' },   { ']', '}' },   { '[', '{' },
    { 0xB7, 0 },    { 0xB5, 0xD6 }, { 0xB6, 0xD7 }, { 0xB4, 0 },
};
static const uint8_t s_modifiers[] = { 0xA1, 0xA2, 0xA3, 0xA5 };

static keymap_t g_km;

/* One pair in; the first event out, if any, in *ev, and the count. */
static km_result_t feed_n(uint8_t state, uint8_t code, km_event_t ev[KM_EVENTS_MAX], unsigned *n) {
    return keymap_feed(&g_km, state, code, ev, n);
}

static km_result_t feed(uint8_t state, uint8_t code, km_event_t *ev) {
    km_event_t out[KM_EVENTS_MAX];
    unsigned n;
    km_result_t r = keymap_feed(&g_km, state, code, out, &n);
    if (r == KM_EVENT) {
        CHECK(n >= 1, "0x%02x: an event result with no event", code);
        if (ev) *ev = out[0];
    } else {
        CHECK(n == 0, "0x%02x: events with result %d", code, r);
    }
    return r;
}

static bool sendable(uint8_t code) {
    for (unsigned i = 0; i < sizeof s_keys / sizeof s_keys[0]; i++) {
        if (s_keys[i].chr == code || (s_keys[i].symb && s_keys[i].symb == code)) return true;
        /* Letters' capitals: Shift, Caps Lock, Alt. */
        if (s_keys[i].chr >= 'a' && s_keys[i].chr <= 'z' && s_keys[i].chr - 0x20 == code) return true;
    }
    for (unsigned i = 0; i < sizeof s_modifiers; i++) {
        if (s_modifiers[i] == code) return true;
    }
    return code == MCU_POWER || code == MCU_SYM;
}

static void test_every_code(void) {
    for (unsigned c = 0; c < 256; c++) {
        uint8_t code = (uint8_t)c;
        keymap_reset(&g_km);
        km_event_t down, up;
        km_result_t r = feed(MCU_PRESSED, code, &down);
        if (!sendable(code)) {
            CHECK(r == KM_UNKNOWN, "code 0x%02x is not the MCU's, but was accepted", c);
            CHECK(keymap_physical(code, false) == 0 && keymap_physical(code, true) == 0,
                  "code 0x%02x is not the MCU's, but has a physical key", c);
            continue;
        }
        CHECK(keymap_physical(code, false) != 0, "code 0x%02x has no physical key", c);
        if (code == MCU_CAPS || code == MCU_POWER || code == MCU_SYM ||
            code == MCU_SHIFT_L || code == MCU_SHIFT_R) {
            CHECK(r == KM_CONSUMED, "code 0x%02x should be accepted and not passed on", c);
            continue;
        }
        CHECK(r == KM_EVENT && down.down && down.key != 0, "code 0x%02x: a press gives a down", c);
        CHECK(down.key != DK_RSHIFT, "code 0x%02x gives Doom its run key, Shift", c);
        r = feed(MCU_RELEASED, code, &up);
        CHECK(r == KM_EVENT && !up.down && up.key == down.key,
              "code 0x%02x: its release gives the matching up", c);
        CHECK(feed(MCU_RELEASED, code, NULL) == KM_STRAY, "code 0x%02x: a second release is stray", c);
    }

    /* A key's codes, plain and shifted, are one physical key, and no two
     * keys share one. */
    unsigned n = sizeof s_keys / sizeof s_keys[0];
    for (unsigned i = 0; i < n; i++) {
        uint8_t p = keymap_physical(s_keys[i].chr, false);
        CHECK(p == s_keys[i].chr, "0x%02x names its own key, not 0x%02x", s_keys[i].chr, p);
        if (s_keys[i].symb) {
            CHECK(keymap_physical(s_keys[i].symb, false) == p,
                  "0x%02x is Shift on 0x%02x", s_keys[i].symb, s_keys[i].chr);
        }
        for (unsigned j = i + 1; j < n; j++) {
            CHECK(keymap_physical(s_keys[j].chr, false) != p,
                  "0x%02x and 0x%02x are one key", s_keys[i].chr, s_keys[j].chr);
        }
    }
    CHECK(feed(0, 'a', NULL) == KM_UNKNOWN && feed(4, 'a', NULL) == KM_UNKNOWN,
          "states other than 1-3 are refused");
}

/* Under every modifier the MCU has, no code gives Doom Shift. */
static void test_never_shift(void) {
    static const uint8_t mods[][2] = {
        { 0, 0 }, { MCU_SHIFT_L, 0 }, { MCU_SHIFT_R, 0 }, { MCU_CTRL, 0 }, { MCU_ALT, 0 },
        { MCU_CTRL, MCU_ALT }, { MCU_SHIFT_L, MCU_ALT }, { MCU_SHIFT_L, MCU_CTRL },
    };
    for (unsigned m = 0; m < sizeof mods / sizeof mods[0]; m++) {
        for (unsigned c = 0; c < 256; c++) {
            keymap_reset(&g_km);
            km_event_t out[KM_EVENTS_MAX];
            unsigned n;
            for (unsigned k = 0; k < 2; k++) {
                if (mods[m][k]) keymap_feed(&g_km, MCU_PRESSED, mods[m][k], out, &n);
            }
            keymap_feed(&g_km, MCU_PRESSED, (uint8_t)c, out, &n);
            for (unsigned i = 0; i < n; i++) {
                CHECK(out[i].key != DK_RSHIFT, "0x%02x with 0x%02x 0x%02x gives Doom Shift", c,
                      mods[m][0], mods[m][1]);
            }
        }
    }
}

static void expect(km_result_t r, const km_event_t *e, bool down, int key, const char *what) {
    CHECK(r == KM_EVENT, "%s: no event (%d)", what, r);
    if (r != KM_EVENT) return;
    CHECK(e->down == down, "%s: down %d", what, e->down);
    CHECK(e->key == key, "%s: key 0x%02x, want 0x%02x", what, e->key, key);
}

static void test_retranslated_releases(void) {
    km_event_t e;
    keymap_reset(&g_km);

    /* Shift let go first: press `A`, release `a`. */
    CHECK(feed(MCU_PRESSED, MCU_SHIFT_L, NULL) == KM_CONSUMED, "shift is nothing");
    expect(feed(MCU_PRESSED, 'A', &e), &e, true, 'a', "shift A is a");
    CHECK(feed(MCU_RELEASED, MCU_SHIFT_L, NULL) == KM_CONSUMED, "shift up is nothing");
    expect(feed(MCU_RELEASED, 'a', &e), &e, false, 'a', "a up is A's up");

    /* Press `!`, release `1`. */
    feed(MCU_PRESSED, MCU_SHIFT_R, NULL);
    expect(feed(MCU_PRESSED, '!', &e), &e, true, '!', "!");
    feed(MCU_RELEASED, MCU_SHIFT_R, NULL);
    expect(feed(MCU_RELEASED, '1', &e), &e, false, '!', "1 up is !'s up");

    /* The keys that exist only as alternates: F6-F10 and Break are
     * themselves; the rest are their base keys (§9.3). */
    static const struct { uint8_t press, release; int key; } alt[] = {
        { MCU_F1 + 5, MCU_F1, DK_F1 + 5 },     { MCU_F1 + 6, MCU_F1 + 1, DK_F1 + 6 },
        { MCU_F1 + 7, MCU_F1 + 2, DK_F1 + 7 }, { MCU_F1 + 8, MCU_F1 + 3, DK_F1 + 8 },
        { MCU_F10, MCU_F1 + 4, DK_F10 },       { MCU_BREAK, MCU_ESC, DK_PAUSE },
        { MCU_HOME, MCU_TAB, DK_TAB },         { MCU_END, MCU_DEL, DK_DEL },
        { MCU_PGUP, MCU_UP, DK_UPARROW },      { MCU_PGDN, MCU_DOWN, DK_DOWNARROW },
        { MCU_INSERT, MCU_ENTER, DK_ENTER },
    };
    for (unsigned i = 0; i < sizeof alt / sizeof alt[0]; i++) {
        keymap_reset(&g_km);
        feed(MCU_PRESSED, MCU_SHIFT_L, NULL);
        CHECK(feed(MCU_PRESSED, alt[i].press, &e) == KM_EVENT && e.down && e.key == alt[i].key,
              "0x%02x: down 0x%02x, want 0x%02x", alt[i].press, e.key, alt[i].key);
        feed(MCU_RELEASED, MCU_SHIFT_L, NULL);
        CHECK(feed(MCU_RELEASED, alt[i].release, &e) == KM_EVENT && !e.down &&
                  e.key == alt[i].key,
              "0x%02x released as 0x%02x: up 0x%02x", alt[i].press, alt[i].release, e.key);
    }
}

/* §9.3's sequences, each from a fresh map. */
static void test_shift_trap(void) {
    km_event_t ev[KM_EVENTS_MAX];
    unsigned n;

    /* Hold Left, press Shift: Left is let go at once. Let go of Left
     * under Shift, which the MCU never sends; let go of Shift. Nothing
     * is left down. */
    keymap_reset(&g_km);
    expect(feed(MCU_PRESSED, MCU_LEFT, &ev[0]), &ev[0], true, DK_LEFTARROW, "left");
    CHECK(feed_n(MCU_PRESSED, MCU_SHIFT_L, ev, &n) == KM_EVENT && n == 1 && !ev[0].down &&
              ev[0].key == DK_LEFTARROW,
          "shift lets go of left (%u events)", n);
    CHECK(!keymap_held(&g_km, MCU_LEFT), "left is not held after shift");
    feed(MCU_RELEASED, MCU_SHIFT_L, NULL);
    for (unsigned k = 0; k < 256; k++) {
        CHECK(!keymap_held(&g_km, (uint8_t)k), "0x%02x is stranded down", k);
    }

    /* Left and Right both held: both let go, by the right Shift too. */
    keymap_reset(&g_km);
    feed(MCU_PRESSED, MCU_LEFT, NULL);
    feed(MCU_PRESSED, MCU_RIGHT, NULL);
    CHECK(feed_n(MCU_PRESSED, MCU_SHIFT_R, ev, &n) == KM_EVENT && n == 2 && !ev[0].down &&
              !ev[1].down && ev[0].key != ev[1].key,
          "shift lets go of left and right (%u events)", n);

    /* Still holding the arrow when Shift comes up: the MCU's repeats are
     * holds, and the first brings the key back down. */
    keymap_reset(&g_km);
    feed(MCU_PRESSED, MCU_RIGHT, NULL);
    feed(MCU_PRESSED, MCU_SHIFT_L, NULL);
    feed(MCU_RELEASED, MCU_SHIFT_L, NULL);
    expect(feed(MCU_HELD, MCU_RIGHT, &ev[0]), &ev[0], true, DK_RIGHTARROW,
           "right's repeat after shift is a press");
    expect(feed(MCU_RELEASED, MCU_RIGHT, &ev[0]), &ev[0], false, DK_RIGHTARROW, "right up");

    /* A release that came anyway after Shift's is stray, not an error. */
    keymap_reset(&g_km);
    feed(MCU_PRESSED, MCU_LEFT, NULL);
    feed(MCU_PRESSED, MCU_SHIFT_L, NULL);
    feed(MCU_RELEASED, MCU_SHIFT_L, NULL);
    CHECK(feed(MCU_RELEASED, MCU_LEFT, NULL) == KM_STRAY, "left's late release is stray");

    /* Up held, then Shift: its repeats arrive as PgUp and are repeats;
     * its release, as PgUp or Up, lets go of Up. Shift is nothing. */
    for (int as_pgup = 0; as_pgup < 2; as_pgup++) {
        keymap_reset(&g_km);
        expect(feed(MCU_PRESSED, MCU_UP, &ev[0]), &ev[0], true, DK_UPARROW, "up");
        CHECK(feed(MCU_PRESSED, MCU_SHIFT_L, NULL) == KM_CONSUMED, "shift with up is nothing");
        CHECK(feed(MCU_HELD, MCU_PGUP, NULL) == KM_REPEAT, "pgup under shift is up's repeat");
        if (as_pgup) {
            expect(feed(MCU_RELEASED, MCU_PGUP, &ev[0]), &ev[0], false, DK_UPARROW, "pgup up");
            feed(MCU_RELEASED, MCU_SHIFT_L, NULL);
        } else {
            feed(MCU_RELEASED, MCU_SHIFT_L, NULL);
            expect(feed(MCU_RELEASED, MCU_UP, &ev[0]), &ev[0], false, DK_UPARROW, "up up");
        }
        CHECK(!keymap_held(&g_km, MCU_UP), "up is let go");
    }

    /* Shift first, then Down: PgDn, which is Down to Doom. */
    keymap_reset(&g_km);
    feed(MCU_PRESSED, MCU_SHIFT_R, NULL);
    expect(feed(MCU_PRESSED, MCU_PGDN, &ev[0]), &ev[0], true, DK_DOWNARROW, "shift+down");
    feed(MCU_RELEASED, MCU_SHIFT_R, NULL);
    expect(feed(MCU_RELEASED, MCU_DOWN, &ev[0]), &ev[0], false, DK_DOWNARROW, "down up");

    /* Shift with Up held and Left held lets go of Left only. */
    keymap_reset(&g_km);
    feed(MCU_PRESSED, MCU_UP, NULL);
    feed(MCU_PRESSED, MCU_LEFT, NULL);
    CHECK(feed_n(MCU_PRESSED, MCU_SHIFT_L, ev, &n) == KM_EVENT && n == 1 &&
              ev[0].key == DK_LEFTARROW,
          "shift lets go of left only");
    CHECK(keymap_held(&g_km, MCU_UP), "up stays held");
}

static void test_insert(void) {
    km_event_t e;

    /* Alt+I is Insert, which is I under Alt. Let go of Alt with I still
     * down, and the repeats and the release come as `i` (HW §6.2). */
    keymap_reset(&g_km);
    feed(MCU_PRESSED, MCU_ALT, NULL);
    expect(feed(MCU_PRESSED, MCU_INSERT, &e), &e, true, 'i', "alt+i");
    feed(MCU_RELEASED, MCU_ALT, NULL);
    CHECK(feed(MCU_PRESSED, 'i', NULL) == KM_REPEAT, "i's repeat");
    expect(feed(MCU_RELEASED, 'i', &e), &e, false, 'i', "i up");
    CHECK(feed(MCU_RELEASED, MCU_ENTER, NULL) == KM_STRAY, "Enter was never down");

    /* Shift+Enter is Insert on Enter's key, which is Enter. */
    keymap_reset(&g_km);
    feed(MCU_PRESSED, MCU_SHIFT_L, NULL);
    expect(feed(MCU_PRESSED, MCU_INSERT, &e), &e, true, DK_ENTER, "shift+enter");
    expect(feed(MCU_RELEASED, MCU_INSERT, &e), &e, false, DK_ENTER, "insert up under shift");
    CHECK(feed(MCU_RELEASED, 'i', NULL) == KM_STRAY, "I was never down");
}

static void test_stray_and_repeat(void) {
    km_event_t e;
    keymap_reset(&g_km);

    /* Alt, then a release of Space with no press (HW §6.2). */
    feed(MCU_PRESSED, MCU_ALT, NULL);
    feed(MCU_RELEASED, MCU_ALT, NULL);
    CHECK(feed(MCU_RELEASED, ' ', NULL) == KM_STRAY, "a release with no press is dropped");

    /* Holding j: a press, then more presses, then one release. */
    expect(feed(MCU_PRESSED, 'j', &e), &e, true, 'j', "j");
    for (int i = 0; i < 16; i++) CHECK(feed(MCU_PRESSED, 'j', NULL) == KM_REPEAT, "j repeat");
    expect(feed(MCU_RELEASED, 'j', &e), &e, false, 'j', "j up");

    /* Holding Up: holds, not presses. */
    expect(feed(MCU_PRESSED, MCU_UP, &e), &e, true, DK_UPARROW, "up");
    CHECK(feed(MCU_HELD, MCU_UP, NULL) == KM_REPEAT, "up held");

    /* A hold whose press was lost is a press. */
    expect(feed(MCU_HELD, MCU_ESC, &e), &e, true, DK_ESCAPE, "esc held");

    /* A modifier's holds give Doom nothing more. */
    expect(feed(MCU_PRESSED, MCU_CTRL, &e), &e, true, DK_FIRE, "ctrl is fire");
    CHECK(feed(MCU_HELD, MCU_CTRL, NULL) == KM_REPEAT, "ctrl held");
    CHECK(keymap_held(&g_km, MCU_CTRL), "ctrl is still down");

    keymap_reset(&g_km);
    CHECK(feed(MCU_RELEASED, 'j', NULL) == KM_STRAY, "reset lets go of every key");
}

static void test_codes(void) {
    km_event_t e;
    static const struct { uint8_t mod, code; int key; const char *what; } t[] = {
        { 0, 'a', 'a', "a" },
        { 0, 'A', 'a', "A under Caps Lock" },
        { MCU_SHIFT_R, 'A', 'a', "Shift+A" },
        { MCU_CTRL, 'c', 'c', "Ctrl+C" },
        { MCU_ALT, 'M', 'm', "Alt+M, sent as a capital" },
        { 0, '?', '?', "?" },
        { 0, ' ', DK_USE, "space is use" },
        { 0, ',', DK_STRAFE_L, ", strafes left" },
        { 0, '.', DK_STRAFE_R, ". strafes right" },
        { MCU_SHIFT_L, '<', '<', "<" },
        { MCU_SHIFT_L, '>', '>', ">" },
        { 0, '-', '-', "-" },
        { 0, '=', '=', "=" },
        { 0, MCU_ENTER, DK_ENTER, "Enter" },
        { 0, MCU_BACKSPACE, DK_BACKSPACE, "Backspace" },
        { 0, MCU_TAB, DK_TAB, "Tab" },
        { 0, MCU_ESC, DK_ESCAPE, "Esc" },
        { 0, MCU_DEL, DK_DEL, "Del" },
        { 0, MCU_F1, DK_F1, "F1" },
        { MCU_ALT, MCU_F1, DK_F11, "Alt+F1 is F11" },
        { MCU_ALT, MCU_F1 + 1, DK_F1 + 1, "Alt+F2 is F2" },
        { 0, MCU_F1 + 8, DK_F1 + 8, "F9" },
        { 0, MCU_F10, DK_F10, "F10" },
        { 0, MCU_UP, DK_UPARROW, "Up" },
        { 0, MCU_LEFT, DK_LEFTARROW, "Left" },
        { 0, MCU_DOWN, DK_DOWNARROW, "Down" },
        { 0, MCU_RIGHT, DK_RIGHTARROW, "Right" },
        { MCU_ALT, MCU_LEFT, DK_LEFTARROW, "Alt+Left is Left, with Alt down" },
        { 0, MCU_CTRL, DK_FIRE, "Ctrl is fire" },
        { 0, MCU_ALT, DK_RALT, "Alt is Doom's Alt, strafe" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        keymap_reset(&g_km);
        if (t[i].mod) feed(MCU_PRESSED, t[i].mod, NULL);
        expect(feed(MCU_PRESSED, t[i].code, &e), &e, true, t[i].key, t[i].what);
    }
}

/* Taking a save's name: Space, `,` and `.` type themselves, fixed at the
 * press. */
static void test_text(void) {
    km_event_t e;
    keymap_reset(&g_km);
    g_km.text = true;
    expect(feed(MCU_PRESSED, ' ', &e), &e, true, ' ', "space in a name");
    expect(feed(MCU_PRESSED, ',', &e), &e, true, ',', ", in a name");
    expect(feed(MCU_PRESSED, '.', &e), &e, true, '.', ". in a name");
    g_km.text = false;
    expect(feed(MCU_RELEASED, ' ', &e), &e, false, ' ', "space's up as its press");
    expect(feed(MCU_PRESSED, 'a', &e), &e, true, 'a', "letters the same");
    g_km.text = true;
    expect(feed(MCU_RELEASED, ',', &e), &e, false, ',', ",'s up");
    expect(feed(MCU_RELEASED, '.', &e), &e, false, '.', ".'s up");
}

/* The MCU's code for a physical key with Shift (its alternate) or not. */
static uint8_t mcu_code(uint8_t phys, bool shift) {
    for (unsigned i = 0; i < sizeof s_keys / sizeof s_keys[0]; i++) {
        if (s_keys[i].chr != phys) continue;
        if (!shift) return phys;
        if (phys >= 'a' && phys <= 'z') return (uint8_t)(phys - 0x20);
        return s_keys[i].symb;
    }
    for (unsigned i = 0; i < sizeof s_modifiers; i++) {
        if (s_modifiers[i] == phys) return phys;
    }
    return 0;
}

static void test_bindings(void) {
    for (unsigned i = 0; i < keymap_binding_count; i++) {
        const km_binding_t *b = &keymap_bindings[i];
        /* No binding needs a chord the MCU swallows (HW §6.3): Shift with
         * Left, Right, Space or Backspace sends nothing; Alt with , .
         * Space B is the MCU's own; Alt turns I into Insert. */
        if (b->shift) {
            CHECK(b->phys != MCU_LEFT && b->phys != MCU_RIGHT && b->phys != ' ' &&
                      b->phys != MCU_BACKSPACE,
                  "%s needs Shift with a key it swallows", b->what);
        }
        if (b->alt) {
            CHECK(b->phys != ',' && b->phys != '.' && b->phys != ' ' && b->phys != 'b' &&
                      b->phys != 'i',
                  "%s needs Alt with a key the MCU takes", b->what);
        }
        CHECK(b->key != DK_RSHIFT, "%s is bound to Shift", b->what);

        /* Reached the way the MCU sends it. */
        uint8_t code = mcu_code(b->phys, b->shift);
        CHECK(code != 0, "%s: the MCU sends nothing for 0x%02x%s", b->what, b->phys,
              b->shift ? " with Shift" : "");
        if (code == 0) continue;
        keymap_reset(&g_km);
        if (b->shift) feed(MCU_PRESSED, MCU_SHIFT_L, NULL);
        if (b->alt) feed(MCU_PRESSED, MCU_ALT, NULL);
        km_event_t e;
        km_result_t r = feed(MCU_PRESSED, code, &e);
        CHECK(r == KM_EVENT && e.down && e.key == b->key, "%s: 0x%02x gives 0x%02x, want 0x%02x",
              b->what, code, r == KM_EVENT ? e.key : 0, b->key);
    }

    /* Three keys at once, as play holds them (§9.5): forward, fire, use,
     * each down and up on its own. */
    km_event_t e;
    keymap_reset(&g_km);
    expect(feed(MCU_PRESSED, MCU_UP, &e), &e, true, DK_UPARROW, "up");
    expect(feed(MCU_PRESSED, MCU_CTRL, &e), &e, true, DK_FIRE, "ctrl");
    expect(feed(MCU_PRESSED, ' ', &e), &e, true, DK_USE, "space");
    expect(feed(MCU_RELEASED, MCU_CTRL, &e), &e, false, DK_FIRE, "ctrl up");
    expect(feed(MCU_RELEASED, ' ', &e), &e, false, DK_USE, "space up");
    expect(feed(MCU_RELEASED, MCU_UP, &e), &e, false, DK_UPARROW, "up up");
}

int main(void) {
    test_every_code();
    test_never_shift();
    test_retranslated_releases();
    test_shift_trap();
    test_insert();
    test_stray_and_repeat();
    test_codes();
    test_text();
    test_bindings();
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_keymap: ok, %u bindings\n", keymap_binding_count);
    return 0;
}
