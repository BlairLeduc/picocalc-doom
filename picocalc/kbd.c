/* kbd.c — the southbridge's key events, from core 1 to core 0 (design.md
 * §9.1). From picocalc-quest (D8).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kbd.h"

#include "hardware/sync.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

#include "keymap.h"
#include "southbridge.h"

#define SB_FIFO_DEPTH 31u   /* hardware-notes.md §6.1 */

/* Power of two, so the indices can run free and wrap by mask (§12.1). */
#define RING 64u
_Static_assert((RING & (RING - 1u)) == 0, "the ring must be a power of two");

static uint16_t          s_pair[RING];  /* state << 8 | code */
static uint32_t          s_when[RING];
static volatile uint32_t s_head;        /* written by core 1 only */
static volatile uint32_t s_tail;        /* written by core 0 only */
static volatile uint32_t s_overflows;

static uint32_t room(void) {
    return RING - (s_head - s_tail);
}

static void push(uint8_t state, uint8_t code) {
    uint32_t head = s_head;
    if (head - s_tail >= RING) {
        s_overflows++;
        return;
    }
    s_pair[head % RING] = (uint16_t)(state << 8 | code);
    s_when[head % RING] = time_us_32();
    __dmb();                    /* the entry lands before the index moves */
    s_head = head + 1u;
}

unsigned kbd_poll(void) {
    unsigned n = 0;
    for (; n < SB_FIFO_DEPTH; n++) {
        /* A full ring leaves the rest in the MCU's FIFO for next time. */
        if (room() == 0) {
            s_overflows++;
            break;
        }
        uint8_t r[2];
        if (sb_read(SB_REG_FIF, r) != SB_OK) break;
        if (r[0] == 0 && r[1] == 0) break;          /* empty */
        push(r[0], r[1]);
    }
    return n;
}

#if PICOCALC_DOOM_UART

#define UART_RAW     0x01u
#define UART_RESTART 0x02u

static volatile bool s_restart;

bool kbd_uart_restart(void) {
    return s_restart;
}

void kbd_uart(void) {
    static uint8_t raw[2];
    static unsigned raw_n;       /* bytes of a raw event still to come */

    while (room() >= 2u && uart_is_readable(uart_default)) {
        uint8_t ch = (uint8_t)uart_getc(uart_default);
        if (raw_n) {
            raw[2u - raw_n] = ch;
            if (--raw_n == 0) push(raw[0], raw[1]);
            continue;
        }
        uint8_t code;
        switch (ch) {
        case UART_RAW: raw_n = 2; continue;
        case UART_RESTART: s_restart = true; continue;
        case '\r': case '\n': code = MCU_ENTER; break;
        case 0x08: case 0x7F: code = MCU_BACKSPACE; break;
        case '\t': code = MCU_TAB; break;
        case 0x1B: code = MCU_ESC; break;
        default:
            if (ch < 0x20u || ch > 0x7Eu) continue;
            code = ch;
            break;
        }
        push(MCU_PRESSED, code);
        push(MCU_RELEASED, code);
    }
}

#else

void kbd_uart(void) {}
bool kbd_uart_restart(void) { return false; }

#endif

bool kbd_pop(uint8_t *state, uint8_t *code, uint32_t *queued_us) {
    uint32_t tail = s_tail;
    if (tail == s_head) return false;
    __dmb();                    /* read the entry after seeing the index */
    uint16_t e = s_pair[tail % RING];
    *queued_us = s_when[tail % RING];
    __dmb();                    /* and finish reading before freeing it */
    s_tail = tail + 1u;
    *state = (uint8_t)(e >> 8);
    *code  = (uint8_t)e;
    return true;
}

bool kbd_waiting(void) {
    return s_tail != s_head;
}

void kbd_flush(void) {
    uint8_t s, c;
    uint32_t t;
    while (kbd_pop(&s, &c, &t)) {}
}

uint32_t kbd_events(void) {
    return s_head;
}

uint32_t kbd_overflows(void) {
    return s_overflows;
}
