/* kbd.h — the southbridge's key events, from core 1 to core 0 (design.md
 * §9.1).
 *
 * Core 1 owns the I2C bus and drains the MCU's FIFO; core 0 runs the
 * engine and turns the events into the engine's (keymap.h). Between them
 * is a single-producer, single-consumer ring of the MCU's own [state,
 * code] pairs, each with the time core 1 read it, so neither core waits
 * on the other and the latency to the engine can be measured.
 *
 * In the development build, core 1 also turns bytes received on UART0
 * into events in the same ring (§14.1), so a script can drive the
 * WAD list and Doom with nobody at the keyboard:
 *
 *   0x20-0x7E      that key, pressed and released
 *   CR or LF       Enter          BS or DEL   Backspace
 *   HT             Tab            ESC         Esc
 *   0x01 s c       one FIFO event as the MCU would send it, state s
 *                  (1 pressed, 2 held, 3 released) and code c: arrows,
 *                  function keys, modifiers held across other keys
 *   0x02           restart the program, as a card change does, to test
 *                  that path (kbd_uart_restart)
 *
 * scripts/keys.sh names them. From picocalc-quest's kbd.c (D8), polled
 * every 20 ms rather than 25 (§9.1).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_KBD_H
#define PICOCALC_DOOM_KBD_H

#include <stdbool.h>
#include <stdint.h>

/* Core 1, every KBD_POLL_US: drain the MCU's FIFO, up to its 31
 * entries, while the ring has room. Returns the events read. 50 Hz: the
 * MCU scans its matrix only every 16-26 ms (hardware-notes.md §6.2), so
 * faster buys little, and each read is 4.8 ms of core 1 (§9.1). */
#define KBD_POLL_US 20000u
unsigned kbd_poll(void);

/* Core 1, every loop, development build: the UART's bytes as above,
 * while the ring has room; the rest wait in the UART's FIFO. */
void kbd_uart(void);

/* Core 0: whether the UART asked for a restart (0x02 above). */
bool kbd_uart_restart(void);

/* Core 0: take one event, with time_us_32() when core 1 queued it. */
bool kbd_pop(uint8_t *state, uint8_t *code, uint32_t *queued_us);

/* Core 0: whether anything is queued. */
bool kbd_waiting(void);

/* Core 0: drop whatever is queued. */
void kbd_flush(void);

/* Events queued since boot; and the times core 1 found the ring full,
 * when it leaves the rest in the MCU's FIFO (which drops what it has no
 * room for) or the UART's. The ring holds two of the MCU's FIFOs, so
 * this stays zero unless core 0 stops taking. */
uint32_t kbd_events(void);
uint32_t kbd_overflows(void);

#endif /* PICOCALC_DOOM_KBD_H */
