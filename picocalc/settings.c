/* settings.c — /doom/picocalc.ini, the port's settings (design.md §10.3).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PICOCALC_DOOM_HOST
/* Doom's headers first (doomtype.h's boolean). */
#include "doomkeys.h"
#include "m_controls.h"
#include "frank_doom_log.h"

#include "ff.h"
#include "psram_allocator.h"

#include "status.h"

#include <stdio.h>
#endif

#include "settings.h"

#include "keymap.h"

#include <string.h>

void settings_defaults(settings_t *s) {
    memset(s, 0, sizeof *s);
    /* D12: running is always on, since Doom's run key, Shift, cannot be
     * used (§9.3). */
    s->run = SETTINGS_RUN_ALWAYS;
}

const char *settings_run_name(unsigned run) {
    return run == SETTINGS_RUN_HOLD ? "hold" : run == SETTINGS_RUN_OFF ? "off" : "always";
}

static bool is_space(char c) {
    return c == ' ' || c == '\t';
}

static char lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

/* A word against a lower-case name, in any case. */
static bool same(const char *p, size_t n, const char *name) {
    if (strlen(name) != n) return false;
    for (size_t i = 0; i < n; i++) {
        if (lower(p[i]) != name[i]) return false;
    }
    return true;
}

/* Keys Doom uses itself, from m_controls.c's defaults, beyond those the
 * key map binds (keymap_bindings): weapon 8, and the automap's zoom,
 * follow, grid and marks. A run key held there would act on each press. */
static const char s_doom_keys[] = "8-=0fgmc";

const char *settings_run_key_refused(uint8_t c) {
    if (c <= ' ' || c >= 0x7Fu) return "one printable key";
    c = (uint8_t)lower((char)c);
    /* Shift is never part of a binding in play (§9.3): a key typed only
     * with Shift (`!`, `?`) is refused with it. */
    if (keymap_physical(c, false) != c) return "a key typed without Shift";
    for (unsigned i = 0; i < keymap_binding_count; i++) {
        const km_binding_t *b = &keymap_bindings[i];
        if (b->phys == c && !b->shift) return "a key Doom already uses";
    }
    if (memchr(s_doom_keys, c, sizeof s_doom_keys - 1u) != NULL) return "a key Doom already uses";
    return NULL;
}

static const char *run(const char *p, size_t n, uint8_t *out) {
    if (same(p, n, "always")) *out = SETTINGS_RUN_ALWAYS;
    else if (same(p, n, "hold")) *out = SETTINGS_RUN_HOLD;
    else if (same(p, n, "off")) *out = SETTINGS_RUN_OFF;
    else return "always, hold or off";
    return NULL;
}

static const char *run_key(const char *p, size_t n, uint8_t *out) {
    if (n != 1) return "one key";
    const char *why = settings_run_key_refused((uint8_t)p[0]);
    if (why != NULL) return why;
    *out = (uint8_t)lower(p[0]);
    return NULL;
}

/* A file in /doom, by the name Doom's IWAD table knows it: a name with
 * no directory, ending .wad. Whether it is on the card is for the WAD
 * list to find out (settings_refuse). */
static const char *iwad(const char *p, size_t n, char *out) {
    if (n < 5 || n >= SETTINGS_IWAD_MAX || !same(p + n - 4, 4, ".wad")) {
        return "a file name ending .wad";
    }
    for (size_t i = 0; i < n; i++) {
        if (p[i] <= ' ' || p[i] >= 0x7F || p[i] == '/' || p[i] == '\\' || p[i] == ':') {
            return "a file in /doom, with no folder";
        }
    }
    for (size_t i = 0; i < n; i++) out[i] = lower(p[i]);
    out[n] = '\0';
    return NULL;
}

static const char *args(const char *p, size_t n, char *out) {
    if (n >= SETTINGS_ARGS_MAX) return "longer than 159 characters";
    for (size_t i = 0; i < n; i++) {
        if ((p[i] < ' ' && p[i] != '\t') || p[i] >= 0x7F) return "printable characters only";
    }
    for (size_t i = 0; i < n; i++) out[i] = p[i] == '\t' ? ' ' : p[i];
    out[n] = '\0';
    return NULL;
}

/* One key and value onto s: NULL if used, otherwise why not. */
static const char *apply(settings_t *s, const char *k, size_t kn, const char *v, size_t vn) {
    if (same(k, kn, "iwad")) return iwad(v, vn, s->iwad);
    if (same(k, kn, "args")) return args(v, vn, s->args);
    if (same(k, kn, "run")) return run(v, vn, &s->run);
    if (same(k, kn, "run_key")) return run_key(v, vn, &s->run_key);
    return "no such setting";
}

static void bad(unsigned *count, settings_error_t *errors, unsigned max, unsigned line,
                size_t at, size_t len, const char *why) {
    if (*count < max) errors[*count] = (settings_error_t){ line, at, len, why };
    (*count)++;
}

unsigned settings_parse(settings_t *s, const char *text, size_t len,
                        settings_error_t *errors, unsigned max_errors) {
    unsigned count = 0, line = 0;
    /* The last line that set run, to name if it asks to hold no key. */
    unsigned run_line = 0;
    size_t run_at = 0, run_len = 0;

    size_t pos = 0;
    if (len >= 3 && memcmp(text, "\xEF\xBB\xBF", 3) == 0) pos = 3;   /* a UTF-8 BOM */
    while (pos < len) {
        line++;
        size_t at = pos, end = pos;
        while (end < len && text[end] != '\n') end++;
        pos = end + 1;
        size_t n = end - at;
        if (n > 0 && text[at + n - 1] == '\r') n--;

        const char *p = text + at, *e = p + n;
        while (p < e && is_space(*p)) p++;
        if (p == e || *p == '#' || *p == ';') continue;

        const char *eq = memchr(p, '=', (size_t)(e - p));
        if (eq == NULL) {
            bad(&count, errors, max_errors, line, at, n, "no = in it");
            continue;
        }
        const char *k = p, *ke = eq;
        while (ke > k && is_space(ke[-1])) ke--;
        const char *v = eq + 1;
        while (v < e && is_space(*v)) v++;
        /* A comment: # or ; after a space, within the value. */
        const char *ve = v;
        while (ve < e && !((*ve == '#' || *ve == ';') && ve > v && is_space(ve[-1]))) ve++;
        while (ve > v && is_space(ve[-1])) ve--;
        if (ke == k) {
            bad(&count, errors, max_errors, line, at, n, "no name before the =");
            continue;
        }
        if (ve == v) {
            bad(&count, errors, max_errors, line, at, n, "no value after the =");
            continue;
        }

        /* Applied to a copy, so a bad value leaves the setting as it was. */
        settings_t t = *s;
        size_t kn = (size_t)(ke - k);
        const char *why = apply(&t, k, kn, v, (size_t)(ve - v));
        if (why != NULL) {
            bad(&count, errors, max_errors, line, at, n, why);
            continue;
        }
        *s = t;
        if (same(k, kn, "iwad")) s->iwad_line = line;
        if (same(k, kn, "run")) {
            run_line = line;
            run_at = at;
            run_len = n;
        }
    }

    /* Held, with no key to hold: the run line is not used, and running
     * stays always. */
    if (s->run == SETTINGS_RUN_HOLD && s->run_key == 0) {
        s->run = SETTINGS_RUN_ALWAYS;
        if (run_line != 0) {
            /* In the file's order among the rest. */
            unsigned i = count < max_errors ? count : max_errors;
            while (i > 0 && errors[i - 1].line > run_line) {
                if (i < max_errors) errors[i] = errors[i - 1];
                i--;
            }
            if (i < max_errors) {
                errors[i] = (settings_error_t){ run_line, run_at, run_len, "hold, with no run_key" };
            }
            count++;
        }
    }
    return count;
}

#ifndef PICOCALC_DOOM_HOST
/* Zero until settings_load(), which DG_Init() calls before anything reads
 * it, so the defaults stay in one place. */
settings_t g_settings;

/* The file is small; longer is cut at its last whole line. It is read
 * into the PSRAM for the parse and let go: the SRAM heap is short
 * (milestones.md, M6). */
#define SETTINGS_MAX 2048u
#define ERRORS_MAX   8u

static unsigned s_bad;
static settings_error_t s_first;
static char s_title[64];

void settings_load(void) {
    settings_defaults(&g_settings);
    FIL f;
    if (f_open(&f, SETTINGS_PATH, FA_READ) != FR_OK) {
        FRANK_DOOM_LOG("  settings     : no %s; the defaults\n", SETTINGS_PATH);
        return;
    }
    char *text = psram_malloc(SETTINGS_MAX);
    if (text == NULL) {
        f_close(&f);
        FRANK_DOOM_LOG("  settings     : no room to read %s; the defaults\n", SETTINGS_PATH);
        return;
    }
    FSIZE_t size = f_size(&f);
    UINT got = 0;
    if (f_read(&f, text, SETTINGS_MAX, &got) != FR_OK) got = 0;
    f_close(&f);
    size_t len = got;
    bool cut = size > SETTINGS_MAX;
    if (cut) {
        while (len > 0 && text[len - 1] != '\n') len--;
    }

    settings_error_t errors[ERRORS_MAX];
    s_bad = settings_parse(&g_settings, text, len, errors, ERRORS_MAX);
    for (unsigned i = 0; i < s_bad && i < ERRORS_MAX; i++) {
        FRANK_DOOM_LOG("  settings     : %s line %u not used, %s: \"%.*s\"\n", SETTINGS_PATH,
                       errors[i].line, errors[i].why, (int)errors[i].len, text + errors[i].at);
    }
    if (s_bad > ERRORS_MAX) FRANK_DOOM_LOG("  settings     : and %u more\n", s_bad - ERRORS_MAX);
    if (s_bad > 0) s_first = errors[0];
    if (cut) {
        FRANK_DOOM_LOG("  settings     : %s is %lu bytes; only the first %u read\n",
                       SETTINGS_PATH, (unsigned long)size, (unsigned)len);
        if (s_bad++ == 0) s_first = (settings_error_t){ 0, 0, 0, "the file is over 2 KB" };
    }
    psram_free(text);

    const settings_t *s = &g_settings;
    FRANK_DOOM_LOG("  settings     : iwad %s; args \"%s\"; run %s%s%c\n",
                   s->iwad[0] ? s->iwad : "(the list)", s->args, settings_run_name(s->run),
                   s->run == SETTINGS_RUN_HOLD ? " on " : "",
                   s->run == SETTINGS_RUN_HOLD ? (char)s->run_key : ' ');
    (void)s;   /* unused when FRANK_DOOM_QUIET compiles the log out */
}

void settings_refuse(unsigned line, const char *why) {
    FRANK_DOOM_LOG("  settings     : %s line %u not used, %s\n", SETTINGS_PATH, line, why);
    if (s_bad == 0 || (s_first.line != 0 && line < s_first.line)) {
        s_first = (settings_error_t){ line, 0, 0, why };
    }
    s_bad++;
}

void settings_announce(void) {
    static bool done;
    if (done || s_bad == 0) return;
    done = true;
    if (s_first.line == 0) {
        snprintf(s_title, sizeof s_title, "picocalc.ini: not all read");
    } else if (s_bad == 1) {
        snprintf(s_title, sizeof s_title, "picocalc.ini: line %u not used", s_first.line);
    } else {
        snprintf(s_title, sizeof s_title, "picocalc.ini: line %u, +%u, not used", s_first.line,
                 s_bad - 1u);
    }
    status_show_for(s_title, s_first.why, STATUS_LONG_US);
}

/* Doom's run test (G_BuildTiccmd): run while key_speed is held, or always
 * when joybspeed is past the last joystick button, Chocolate Doom's
 * "joyb_speed = 31" (§9.4). Upstream's defaults: key_speed Shift, which
 * the key map never sends; joybspeed 2, a button no joystick presses. */
#define JOYB_ALWAYS 31
#define JOYB_NONE   2

static bool s_in_game, s_always;
static char s_hold_title[24];

static void set_run(bool always) {
    s_always = always;
    joybspeed = always ? JOYB_ALWAYS : JOYB_NONE;
    key_speed = g_settings.run == SETTINGS_RUN_HOLD ? g_settings.run_key : KEY_RSHIFT;
}

void settings_apply_run(void) {
    set_run(g_settings.run == SETTINGS_RUN_ALWAYS);
    if (g_settings.run == SETTINGS_RUN_HOLD) {
        snprintf(s_hold_title, sizeof s_hold_title, "Running on %c, held", g_settings.run_key);
    }
    s_in_game = true;
    FRANK_DOOM_LOG("  run          : %s (key_speed %d, joyb_speed %d)\n",
                   settings_run_name(g_settings.run), key_speed, joybspeed);
}

void settings_toggle_run(void) {
    if (!s_in_game) return;
    set_run(!s_always);
    if (s_always) {
        status_show("Always running", "Caps Lock: walk");
    } else if (g_settings.run == SETTINGS_RUN_HOLD) {
        status_show(s_hold_title, "Caps Lock: always run");
    } else {
        status_show("Walking", "Caps Lock: always run");
    }
    FRANK_DOOM_LOG("picocalc-doom: Caps Lock, running %s\n",
                   s_always ? "always" : settings_run_name(g_settings.run == SETTINGS_RUN_HOLD
                                                           ? SETTINGS_RUN_HOLD : SETTINGS_RUN_OFF));
}
#endif
