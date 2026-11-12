/* settings.h — /doom/picocalc.ini, the port's settings (design.md §10.3, §9.6).
 *
 * A text file of `key = value` lines, read once at boot on core 0, after
 * the card is mounted and before the WAD list. Every default is in
 * settings_defaults() (EL §8.7). Keys and words are in any case; blank
 * lines, and lines whose first character is # or ;, are comments; so is
 * the rest of a value from a # or ; that follows a space. A key given
 * twice takes the later value. A line that cannot be used is named, with
 * why, and the rest of the file still applies:
 *
 *     iwad    = doom2.wad          start this IWAD without the list (D13)
 *     args    = -file sigil.wad    more Doom arguments, after -iwad (D15)
 *     run     = always             always, hold (on run_key) or off; Caps
 *                                  Lock changes it in play (§9.4)
 *     run_key = z                  the key held to run when run = hold
 *     fire    = z                  an action in play on a key (§9.6, D17):
 *     use     = ctrl               one per line, settings_action_name()'s
 *
 * A key is one character the PicoCalc types without Shift, or a name:
 * space, ctrl, alt, up, down, left, right, tab, del; or none. Once the
 * whole file is read, a key on two actions (counting the run key, and
 * §9.2's key for each action the file leaves alone) is refused on the
 * later line, and that action goes back to §9.2's key; so a swap needs
 * both lines, in either order.
 *
 * Volumes, screen size, gamma and messages are Doom's, in default.cfg.
 * The user writes the file; nothing here writes it.
 *
 * The parser is pure code, with no Pico SDK, so the host test can drive
 * it (§15). From picocalc-quest's settings.c (D8).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_SETTINGS_H
#define PICOCALC_DOOM_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SETTINGS_PATH "/doom/picocalc.ini"

/* "freedoom1.wad" is 13; room for a longer name than any IWAD's. */
#define SETTINGS_IWAD_MAX 16u
#define SETTINGS_ARGS_MAX 160u

enum { SETTINGS_RUN_ALWAYS, SETTINGS_RUN_HOLD, SETTINGS_RUN_OFF };

/* The actions in play the file binds (§9.6), in settings_t's key[]. */
enum {
    SETTINGS_FORWARD, SETTINGS_BACK, SETTINGS_TURN_LEFT, SETTINGS_TURN_RIGHT,
    SETTINGS_STRAFE_LEFT, SETTINGS_STRAFE_RIGHT, SETTINGS_STRAFE,
    SETTINGS_FIRE, SETTINGS_USE,
    SETTINGS_WEAPON1, SETTINGS_WEAPON8 = SETTINGS_WEAPON1 + 7,
    SETTINGS_PREV_WEAPON, SETTINGS_NEXT_WEAPON, SETTINGS_AUTOMAP,
    SETTINGS_ACTIONS
};

/* Keys are physical keys, by keymap_physical()'s name (a letter in lower
 * case, MCU_CTRL, MCU_UP), or 0 for none; keymap_play_code() gives the
 * Doom code each sends. */
typedef struct {
    char     iwad[SETTINGS_IWAD_MAX];   /* lower case, or "" for none   */
    unsigned iwad_line;                 /* the line that set it, or 0   */
    char     args[SETTINGS_ARGS_MAX];   /* as written, or ""            */
    uint8_t  run;                       /* SETTINGS_RUN_*               */
    uint8_t  run_key;                   /* a physical key, or 0         */
    uint8_t  key[SETTINGS_ACTIONS];     /* each action's physical key   */
} settings_t;

/* A line not used: its number from 1, where it is in the text (without
 * its line end), and why, a short literal. */
typedef struct {
    unsigned    line;
    size_t      at, len;
    const char *why;
} settings_error_t;

void settings_defaults(settings_t *s);

/* Apply the text's lines over what is in `s`. Returns the number of
 * lines not used; the first `max_errors` of them are described in
 * `errors`, in the order of the file. */
unsigned settings_parse(settings_t *s, const char *text, size_t len,
                        settings_error_t *errors, unsigned max_errors);

/* Why the character `c` cannot be a key in the file, the run key or an
 * action's, whatever else is bound, or NULL if it can (§9.3, §9.6): it
 * must be printable, typed without Shift, and not one Doom's menu or the
 * automap takes in play (`-` `=` `0` `f` `g` `m` `c`). */
const char *settings_key_refused(uint8_t c);

/* The action's setting name ("fire", "weapon3"), and its key in §9.2. */
const char *settings_action_name(unsigned action);
uint8_t settings_default_key(unsigned action);

/* A physical key as the file names it ("ctrl", "z", "none"); `buf` holds
 * a single character's name. */
const char *settings_key_name(uint8_t phys, char buf[2]);

/* "always", "hold" or "off". */
const char *settings_run_name(unsigned run);

#ifndef PICOCALC_DOOM_HOST
/* The settings in use, from settings_load(). */
extern settings_t g_settings;

/* Core 0, once the card is mounted: read SETTINGS_PATH, if it is there,
 * into g_settings, logging every line not used. */
void settings_load(void);

/* Core 0: a line the parser took that turned out unusable later (an
 * iwad that is not on the card), logged and counted with the rest. */
void settings_refuse(unsigned line, const char *why);

/* Core 0, as Doom starts: name the first line not used, if any, in the
 * bottom band (§7.5). */
void settings_announce(void);

/* Core 0, after Doom has loaded default.cfg (from I_InitJoystick()): set
 * every action's key_* variable to its binding, over default.cfg's
 * (§9.6). */
void settings_apply_bindings(void);

/* Core 0, after Doom has loaded default.cfg (from I_InitJoystick()): set
 * Doom's run variables to the setting (§9.4). */
void settings_apply_run(void);

/* Core 0, on Caps Lock in play: always-run on or off, and say which in
 * the band. Nothing before settings_apply_run(). */
void settings_toggle_run(void);
#endif

#endif /* PICOCALC_DOOM_SETTINGS_H */
