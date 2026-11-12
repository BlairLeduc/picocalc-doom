/* keymap.c — the southbridge's key events to Doom's (design.md §9.1,
 * §9.2, §9.3; D12).
 *
 * The codes and their shifted alternates are read from the MCU's keymap,
 * Code/picocalc_keyboard/keyboard.ino at clockworkpi/PicoCalc
 * f91519806d4b2e0a62c4638a9f695cd5162c5479. Doom's codes are doomkeys.h's;
 * the bindings they meet are upstream's defaults in m_controls.c.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "keymap.h"

#include <string.h>

/* Shifted punctuation and the key it is on (keyboard.ino's kbd_entries
 * and btn_entries, chr then symb). */
static const struct { char shifted, base; } s_alternates[] = {
    { '~', '`' },  { '#', '3' },  { '@', '2' },  { '$', '4' },  { '+', '=' },
    { '_', '-' },  { '|', '\\' }, { '?', '/' },  { '!', '1' },  { '*', '8' },
    { '&', '7' },  { '^', '6' },  { '%', '5' },  { '>', '.' },  { ':', ';' },
    { '"', '\'' }, { '<', ',' },  { ')', '0' },  { '(', '9' },  { '}', ']' },
    { '{', '[' },
};

static const char s_unshifted[] = "`=-\\/.;',[] ";

/* §9.2's table, in play. */
const km_binding_t keymap_bindings[] = {
    { DK_UPARROW,    MCU_UP,        false, false, "forward" },
    { DK_DOWNARROW,  MCU_DOWN,      false, false, "back" },
    { DK_LEFTARROW,  MCU_LEFT,      false, false, "turn left; strafe with Alt" },
    { DK_RIGHTARROW, MCU_RIGHT,     false, false, "turn right; strafe with Alt" },
    { DK_RALT,       MCU_ALT,       false, false, "strafe, held with an arrow (key_strafe)" },
    { DK_FIRE,       MCU_CTRL,      false, false, "fire" },
    { DK_USE,        ' ',           false, false, "use" },
    { DK_STRAFE_L,   ',',           false, false, "strafe left" },
    { DK_STRAFE_R,   '.',           false, false, "strafe right" },
    { '1',           '1',           false, false, "fist, chainsaw" },
    { '2',           '2',           false, false, "pistol" },
    { '3',           '3',           false, false, "shotgun" },
    { '4',           '4',           false, false, "chaingun" },
    { '5',           '5',           false, false, "rocket launcher" },
    { '6',           '6',           false, false, "plasma rifle" },
    { '7',           '7',           false, false, "BFG 9000" },
    { DK_ESCAPE,     MCU_ESC,       false, false, "menu" },
    { DK_ENTER,      MCU_ENTER,     false, false, "menu select; messages" },
    { DK_BACKSPACE,  MCU_BACKSPACE, false, false, "menu back" },
    { 'y',           'y',           false, false, "confirm" },
    { 'n',           'n',           false, false, "cancel" },
    { DK_TAB,        MCU_TAB,       false, false, "automap" },
    { '-',           '-',           false, false, "screen smaller; automap zoom out" },
    { '=',           '=',           false, false, "screen larger; automap zoom in" },
    { DK_F1,         MCU_F1,        false, false, "help" },
    { DK_F1 + 1,     MCU_F1 + 1,    false, false, "save" },
    { DK_F1 + 2,     MCU_F1 + 2,    false, false, "load" },
    { DK_F1 + 3,     MCU_F1 + 3,    false, false, "volume" },
    { DK_F1 + 4,     MCU_F1 + 4,    false, false, "detail" },
    { DK_F1 + 5,     MCU_F1,        true,  false, "quicksave (F6)" },
    { DK_F1 + 6,     MCU_F1 + 1,    true,  false, "end game (F7)" },
    { DK_F1 + 7,     MCU_F1 + 2,    true,  false, "messages (F8)" },
    { DK_F1 + 8,     MCU_F1 + 3,    true,  false, "quickload (F9)" },
    { DK_F10,        MCU_F1 + 4,    true,  false, "quit (F10)" },
    { DK_F11,        MCU_F1,        false, true,  "gamma (F11)" },
    { DK_PAUSE,      MCU_ESC,       true,  false, "pause (Break)" },
};
const unsigned keymap_binding_count = sizeof keymap_bindings / sizeof keymap_bindings[0];

uint8_t keymap_physical(uint8_t code, bool alt) {
    if (code >= 'A' && code <= 'Z') return (uint8_t)(code + ('a' - 'A'));
    if ((code >= 'a' && code <= 'z') || (code >= '0' && code <= '9')) return code;
    if (code != 0 && strchr(s_unshifted, code)) return code;
    for (unsigned i = 0; i < sizeof s_alternates / sizeof s_alternates[0]; i++) {
        if ((uint8_t)s_alternates[i].shifted == code) return (uint8_t)s_alternates[i].base;
    }
    switch (code) {
    case MCU_BACKSPACE: case MCU_TAB: case MCU_ENTER: case MCU_ESC: case MCU_DEL:
    case MCU_LEFT: case MCU_UP: case MCU_DOWN: case MCU_RIGHT:
    case MCU_ALT: case MCU_SHIFT_L: case MCU_SHIFT_R: case MCU_SYM: case MCU_CTRL:
    case MCU_CAPS: case MCU_POWER:
        return code;
    case MCU_F1: case MCU_F1 + 1: case MCU_F1 + 2: case MCU_F1 + 3: case MCU_F1 + 4:
        return code;
    case MCU_F1 + 5: case MCU_F1 + 6: case MCU_F1 + 7: case MCU_F1 + 8:
        return (uint8_t)(code - 5u);               /* F6-F9 are Shift+F1-F4 */
    case MCU_F10:   return MCU_F1 + 4u;            /* Shift+F5 */
    case MCU_BREAK: return MCU_ESC;
    case MCU_HOME:  return MCU_TAB;
    case MCU_END:   return MCU_DEL;
    case MCU_PGUP:  return MCU_UP;
    case MCU_PGDN:  return MCU_DOWN;
    case MCU_INSERT: return alt ? (uint8_t)'i' : MCU_ENTER;
    default:        return 0;
    }
}

bool keymap_held(const keymap_t *km, uint8_t phys) {
    return km->key[phys].down;
}

/* Doom's code for a key as the MCU sent it at its press, with Alt as it
 * was then, and whether Doom was taking text. 0 for a key that gives Doom
 * nothing. */
static uint8_t translate(bool text, uint8_t code, uint8_t phys, bool alt) {
    /* Letters in lower case whatever made them capitals (Shift, Caps
     * Lock, or Alt, under which the MCU leaves them capitals): Doom's
     * bindings, cheats and menus are lower case, and a save's name is
     * put in capitals by Doom. */
    if (code >= 'A' && code <= 'Z') return (uint8_t)(code | 0x20u);
    switch (code) {
    case ' ': return text ? (uint8_t)' ' : DK_USE;
    case ',': return text ? (uint8_t)',' : DK_STRAFE_L;
    case '.': return text ? (uint8_t)'.' : DK_STRAFE_R;
    default: break;
    }
    if (code > ' ' && code < 0x7Fu) return code;

    /* F1-F5, F6-F9 (Shift+F1-F4) and F10 (Shift+F5): Doom's F1-F10.
     * Alt+F1 is F11, gamma (§9.2). */
    if (code >= MCU_F1 && code <= MCU_F1 + 8u) {
        if (code == MCU_F1 && alt) return DK_F11;
        return (uint8_t)(DK_F1 + (code - MCU_F1));
    }
    switch (code) {
    case MCU_F10:       return DK_F10;
    case MCU_BREAK:     return DK_PAUSE;
    case MCU_ENTER:     return DK_ENTER;
    case MCU_BACKSPACE: return DK_BACKSPACE;
    case MCU_TAB:       return DK_TAB;
    case MCU_ESC:       return DK_ESCAPE;
    case MCU_DEL:       return DK_DEL;
    case MCU_UP:        return DK_UPARROW;
    case MCU_DOWN:      return DK_DOWNARROW;
    case MCU_LEFT:      return DK_LEFTARROW;
    case MCU_RIGHT:     return DK_RIGHTARROW;
    case MCU_CTRL:      return DK_FIRE;
    case MCU_ALT:       return DK_RALT;
    /* Shift's other alternates are their base keys, since Shift is
     * nothing in play (§9.3): PgUp is Up, so holding Shift does not stop
     * the player; Home is Tab, End is Del, Insert is Enter, or I under
     * Alt. */
    case MCU_PGUP: case MCU_PGDN: case MCU_HOME: case MCU_END: case MCU_INSERT:
        return translate(text, phys, phys, alt);
    default:            return 0;
    }
}

uint8_t keymap_play_code(uint8_t phys) {
    return translate(false, phys, phys, false);
}

void keymap_reset(keymap_t *km) {
    memset(km, 0, sizeof *km);
}

km_result_t keymap_feed(keymap_t *km, uint8_t state, uint8_t code,
                        km_event_t out[KM_EVENTS_MAX], unsigned *n) {
    *n = 0;
    if (state < MCU_PRESSED || state > MCU_RELEASED) return KM_UNKNOWN;
    bool alt = km->key[MCU_ALT].down;
    uint8_t phys = keymap_physical(code, alt);
    if (phys == 0) return KM_UNKNOWN;
    /* Accepted and not passed on (§9.2): Caps Lock, which the MCU
     * applies to the letters itself; the power button; Symbol. */
    if (phys == MCU_CAPS || phys == MCU_POWER || phys == MCU_SYM) return KM_CONSUMED;

    km_held_t *h = &km->key[phys];
    if (state == MCU_RELEASED) {
        if (!h->down) return KM_STRAY;
        h->down = false;
        if (h->key == 0) return KM_CONSUMED;
        out[(*n)++] = (km_event_t){ false, h->key, phys };
        return KM_EVENT;
    }

    /* The MCU's auto-repeat comes as more presses, and as holds for the
     * keys that are not text (§6.2). Doom makes no repeats of its own in
     * play and its menus step on each press: one down per key. */
    if (h->down) return KM_REPEAT;

    /* A press, or a hold whose press was lost: either way the key is
     * down from here. */
    h->down = true;
    h->key = (phys == MCU_SHIFT_L || phys == MCU_SHIFT_R) ? 0 : translate(km->text, code, phys, alt);

    if (phys == MCU_SHIFT_L || phys == MCU_SHIFT_R) {
        /* The Shift trap (§9.3): with Shift down the MCU sends nothing
         * for Left or Right, not even the release, so let go of them
         * now. A finger still on the arrow brings it back with the
         * MCU's repeats once Shift is up. */
        static const uint8_t arrows[] = { MCU_LEFT, MCU_RIGHT };
        for (unsigned i = 0; i < sizeof arrows; i++) {
            km_held_t *a = &km->key[arrows[i]];
            if (!a->down) continue;
            a->down = false;
            out[(*n)++] = (km_event_t){ false, a->key, arrows[i] };
        }
        return *n ? KM_EVENT : KM_CONSUMED;
    }
    if (h->key == 0) return KM_CONSUMED;
    out[(*n)++] = (km_event_t){ true, h->key, phys };
    return KM_EVENT;
}
