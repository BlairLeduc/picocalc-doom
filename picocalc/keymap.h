/* keymap.h — the southbridge's key events to Doom's (design.md §9.1,
 * §9.2, §9.3; D12).
 *
 * The MCU sends [state, code] pairs: pressed 1, held 2, released 3, with
 * Shift already applied and some keys only reachable as another's
 * shifted alternate (hardware-notes.md §6.2, §6.3). Doom wants a key
 * down and a key up per key, in its own codes (doomkeys.h), with no
 * repeats.
 *
 * So the map keeps the set of physical keys held, each with the Doom
 * key its press sent. Everything about a held key is fixed at its press,
 * and its release sends the matching up however the MCU translates the
 * release (press `A` / release `a`, press PgUp / release Up). The MCU's
 * repeats are dropped; a release of a key that is not down is dropped.
 *
 * Shift is bound to nothing (§9.3): it never reaches Doom, so Doom's
 * run-on-Shift cannot fire, and its press lets go of a held Left or
 * Right, whose release the MCU swallows while Shift is down. Keys the
 * PicoCalc has only as Shift's alternates are their base keys (PgUp is
 * Up, Home is Tab), except F6-F10 and Break, which are Doom's F6-F10 and
 * Pause.
 *
 * Pure code, with no Pico SDK, so the host test can drive it (§15).
 * Core 0 owns the map; core 1 only queues the MCU's pairs (kbd.h). From
 * picocalc-quest's keymap.c (D8), with Doom's codes.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_KEYMAP_H
#define PICOCALC_DOOM_KEYMAP_H

#include <stdbool.h>
#include <stdint.h>

/* The MCU's codes (hardware-notes.md §6.2; keyboard.h in the MCU source,
 * clockworkpi/PicoCalc f915198). Printable keys are ASCII. */
#define MCU_BACKSPACE 0x08u
#define MCU_TAB       0x09u
#define MCU_ENTER     0x0Au
#define MCU_F1        0x81u   /* F1-F5 0x81-0x85; F6-F9 0x86-0x89 */
#define MCU_F10       0x90u
#define MCU_POWER     0x91u   /* the power button, a press with no release */
#define MCU_ALT       0xA1u
#define MCU_SHIFT_L   0xA2u
#define MCU_SHIFT_R   0xA3u
#define MCU_SYM       0xA4u   /* defined, never sent */
#define MCU_CTRL      0xA5u
#define MCU_ESC       0xB1u
#define MCU_LEFT      0xB4u
#define MCU_UP        0xB5u
#define MCU_DOWN      0xB6u
#define MCU_RIGHT     0xB7u
#define MCU_CAPS      0xC1u
#define MCU_BREAK     0xD0u   /* Shift+Esc */
#define MCU_INSERT    0xD1u   /* Shift+Enter, or Alt+I */
#define MCU_HOME      0xD2u   /* Shift+Tab */
#define MCU_DEL       0xD4u
#define MCU_END       0xD5u   /* Shift+Del */
#define MCU_PGUP      0xD6u   /* Shift+Up */
#define MCU_PGDN      0xD7u   /* Shift+Down */

#define MCU_PRESSED   1u
#define MCU_HELD      2u
#define MCU_RELEASED  3u

/* Doom's key codes (doomkeys.h), here so the host test needs no
 * upstream; input_shim.c checks each against Doom's own. */
#define DK_RIGHTARROW 0xAEu
#define DK_LEFTARROW  0xACu
#define DK_UPARROW    0xADu
#define DK_DOWNARROW  0xAFu
#define DK_STRAFE_L   0xA0u
#define DK_STRAFE_R   0xA1u
#define DK_USE        0xA2u
#define DK_FIRE       0xA3u
#define DK_ESCAPE     27u
#define DK_ENTER      13u
#define DK_TAB        9u
#define DK_F1         (0x80u + 0x3Bu)   /* F1-F10 are consecutive */
#define DK_F10        (0x80u + 0x44u)
#define DK_F11        (0x80u + 0x57u)
#define DK_BACKSPACE  0x7Fu
#define DK_PAUSE      0xFFu
#define DK_RSHIFT     (0x80u + 0x36u)
#define DK_RALT       (0x80u + 0x38u)
#define DK_DEL        (0x80u + 0x53u)

typedef struct {
    bool    down;   /* key down, or key up                   */
    uint8_t key;    /* Doom's code (DK_*, or lower-case ASCII) */
    uint8_t phys;   /* the physical key, keymap_physical()'s   */
} km_event_t;

/* The most events one MCU pair gives: Shift's press lets go of Left and
 * Right. */
#define KM_EVENTS_MAX 2u

typedef enum {
    KM_EVENT,     /* out[] holds *n events for Doom                       */
    KM_CONSUMED,  /* accepted, nothing for Doom: Shift with no arrow held,
                     Caps Lock, the power button                          */
    KM_REPEAT,    /* the MCU's repeat of a key already down; dropped      */
    KM_STRAY,     /* a release of a key that is not down; dropped (§6.2)  */
    KM_UNKNOWN,   /* a state or code the MCU's keymap does not send       */
} km_result_t;

typedef struct {
    bool    down;
    uint8_t key;    /* what its press sent */
} km_held_t;

typedef struct {
    km_held_t key[256];   /* by physical key, keymap_physical() */
    /* Doom is taking a save's name (m_menu.c's saveStringEnter): Space,
     * `,` and `.` are typed as themselves, not Use and strafe. Set by
     * the caller before each feed; a key keeps what its press chose. */
    bool text;
} keymap_t;

void keymap_reset(keymap_t *km);

/* One MCU pair in; up to KM_EVENTS_MAX Doom events out, *n of them. */
km_result_t keymap_feed(keymap_t *km, uint8_t state, uint8_t code,
                        km_event_t out[KM_EVENTS_MAX], unsigned *n);

/* The physical key a code comes from, named by the code that key sends
 * unshifted (letters in lower case): `!` is `1`, Home is Tab, F6 is F1.
 * Insert is Enter's alternate, or I's under Alt (hardware-notes.md
 * §6.2), so it needs Alt's state. 0 for a code the MCU does not send. */
uint8_t keymap_physical(uint8_t code, bool alt);

/* Whether a physical key is held now. */
bool keymap_held(const keymap_t *km, uint8_t phys);

/* What Doom does with each key the port binds in play (§9.2), for the
 * host test: a Doom key, and the physical key and modifier that reach
 * it. */
typedef struct {
    uint8_t     key;    /* Doom's code                                */
    uint8_t     phys;   /* keymap_physical()'s name                    */
    bool        shift;  /* with Shift held: F6-F10 and Break only      */
    bool        alt;    /* with Alt held                               */
    const char *what;
} km_binding_t;

extern const km_binding_t keymap_bindings[];
extern const unsigned     keymap_binding_count;

#endif /* PICOCALC_DOOM_KEYMAP_H */
