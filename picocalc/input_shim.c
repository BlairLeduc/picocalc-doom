/*
 * picocalc-doom — DG_GetKey() (design.md §5.3, §9.1).
 *
 * Core 1 queues the MCU's [state, code] pairs with the time it read
 * them (kbd.h); here, on core 0, each goes through the key map
 * (keymap.h), and its Doom events are handed out one at a time. Doom's
 * I_GetEvent() calls this until it returns 0, but stops after a key's
 * release until its next tic; a release after which more was waiting is
 * counted, so the heartbeat says whether that matters (§9.1).
 *
 * In the development build every pair is logged with its code, the
 * modifiers held, its time in the ring and what Doom got (M4), and each
 * press's time to the first frame after it.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Doom's headers first (doomtype.h's boolean). */
#include "doomgeneric.h"
#include "doomkeys.h"

#include "pico/stdlib.h"

#include "core1.h"
#include "frank_doom_log.h"
#include "input.h"
#include "kbd.h"
#include "keymap.h"

#include <stdio.h>

/* keymap.h keeps its own copies of Doom's codes, for the host test. */
_Static_assert(DK_RIGHTARROW == KEY_RIGHTARROW && DK_LEFTARROW == KEY_LEFTARROW &&
               DK_UPARROW == KEY_UPARROW && DK_DOWNARROW == KEY_DOWNARROW, "arrows");
_Static_assert(DK_STRAFE_L == KEY_STRAFE_L && DK_STRAFE_R == KEY_STRAFE_R &&
               DK_USE == KEY_USE && DK_FIRE == KEY_FIRE, "doomgeneric's action keys");
_Static_assert(DK_ESCAPE == KEY_ESCAPE && DK_ENTER == KEY_ENTER && DK_TAB == KEY_TAB &&
               DK_BACKSPACE == KEY_BACKSPACE && DK_PAUSE == KEY_PAUSE, "editing keys");
_Static_assert(DK_F1 == KEY_F1 && DK_F1 + 5 == KEY_F6 && DK_F10 == KEY_F10 &&
               DK_F11 == KEY_F11, "function keys");
_Static_assert(DK_RSHIFT == KEY_RSHIFT && DK_RALT == KEY_RALT && DK_DEL == KEY_DEL,
               "modifiers");

/* m_menu.c's: Doom is taking a save's name. Not in m_menu.h, but not
 * static either. */
extern int saveStringEnter;

input_stats_t g_input;

static keymap_t s_keymap;

/* One pair can give two events (Shift's press, keymap.h). */
static km_event_t s_pending[KM_EVENTS_MAX];
static unsigned s_pending_n, s_pending_at;

/* A press Doom has taken, until a frame is published: the time core 1
 * read it. */
static bool s_frame_pending;
static uint32_t s_frame_from;
static uint8_t s_frame_key;

#if PICOCALC_DOOM_UART
/* Every pair, as the MCU sent it and as Doom gets it, so a run through
 * the whole keyboard can be checked from the log (M4). */
static void log_key(uint8_t state, uint8_t code, km_result_t r, const km_event_t *e,
                    unsigned n, uint32_t wait_us) {
    static const char *const states[] = { "?", "pressed", "held", "released" };
    char shown[4] = "";
    if (code > 0x20u && code < 0x7Fu) snprintf(shown, sizeof shown, " %c", code);
    printf("key %s 0x%02x%s [%s%s%s] %lu us: ", states[state <= 3u ? state : 0u], code, shown,
           keymap_held(&s_keymap, MCU_SHIFT_L) || keymap_held(&s_keymap, MCU_SHIFT_R)
               ? "shift " : "",
           keymap_held(&s_keymap, MCU_CTRL) ? "ctrl " : "",
           keymap_held(&s_keymap, MCU_ALT) ? "alt" : "", (unsigned long)wait_us);
    switch (r) {
    case KM_EVENT:
        for (unsigned i = 0; i < n; i++) {
            printf("%s%s 0x%02x", i ? ", " : "", e[i].down ? "down" : "up", e[i].key);
        }
        printf("\n");
        break;
    case KM_CONSUMED: printf("taken, not passed on\n"); break;
    case KM_REPEAT:   printf("repeat, dropped\n"); break;
    case KM_STRAY:    printf("dropped, the key was not down\n"); break;
    case KM_UNKNOWN:  printf("UNKNOWN to the key map\n"); break;
    }
}
#endif

/* Take pairs until one gives Doom something, or the ring is empty. */
static void refill(void) {
    uint8_t state, code;
    uint32_t queued;
    while (s_pending_n == 0 && kbd_pop(&state, &code, &queued)) {
        uint32_t wait = time_us_32() - queued;
        g_input.taken++;
        g_input.wait_sum_us += wait;
        if (wait > g_input.wait_max_us) g_input.wait_max_us = wait;

        s_keymap.text = saveStringEnter != 0;
        unsigned n;
        km_result_t r = keymap_feed(&s_keymap, state, code, s_pending, &n);
#if PICOCALC_DOOM_UART
        log_key(state, code, r, s_pending, n, wait);
#endif
        switch (r) {
        case KM_EVENT:   break;
        case KM_REPEAT:  g_input.repeats++; continue;
        case KM_STRAY:   g_input.stray++; continue;
        case KM_UNKNOWN: g_input.unknown++; continue;
        case KM_CONSUMED: continue;
        }
        s_pending_n = n;
        s_pending_at = 0;
        if (s_pending[0].down && !s_frame_pending) {
            s_frame_pending = true;
            s_frame_from = queued;
            s_frame_key = s_pending[0].key;
        }
    }
}

int DG_GetKey(int *pressed, unsigned char *key) {
    if (kbd_uart_restart()) {
        FRANK_DOOM_LOG("picocalc-doom: restart asked for over the UART\n");
        picocalc_restart();
    }
    if (s_pending_at == s_pending_n) {
        s_pending_n = s_pending_at = 0;
        refill();
        if (s_pending_n == 0) return 0;
    }
    const km_event_t *e = &s_pending[s_pending_at++];
    *pressed = e->down;
    *key = e->key;
    g_input.events++;
    if (!e->down && (s_pending_at < s_pending_n || kbd_waiting())) g_input.held_over++;
    return 1;
}

void input_frame_published(uint32_t now_us) {
    if (!s_frame_pending) return;
    s_frame_pending = false;
    uint32_t us = now_us - s_frame_from;
    g_input.frame_n++;
    g_input.frame_sum_us += us;
    if (us > g_input.frame_max_us) g_input.frame_max_us = us;
    FRANK_DOOM_LOG("key 0x%02x: a frame published %lu us after the FIFO read\n", s_frame_key,
                   (unsigned long)us);
}

int input_next_press(void) {
    int pressed;
    unsigned char key;
    while (DG_GetKey(&pressed, &key)) {
        if (pressed) {
            /* Not Doom's: no frame will close it. */
            s_frame_pending = false;
            return key;
        }
    }
    return -1;
}
