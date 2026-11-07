/* log.c — stdio into a ring that core 1 drains to UART0 (design.md §14.2).
 *
 * picocalc-quest's log ring (D8): the producer side is a stdio driver, so
 * Doom's and upstream's printf go through it unchanged.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "log.h"

#include <stdbool.h>

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "pico/stdio/driver.h"
#include "pico/stdlib.h"

#if PICOCALC_DOOM_UART

/* Power of two, so the indices can run free and wrap by mask (§12.1). */
#define LOG_RING 4096u
_Static_assert((LOG_RING & (LOG_RING - 1u)) == 0, "the log ring must be a power of two");

#define LOG_BAUD 115200u

static char              s_ring[LOG_RING];
static volatile uint32_t s_head;      /* written under the lock */
static volatile uint32_t s_tail;      /* written by core 1 only */
static volatile uint32_t s_dropped;
static volatile bool     s_ring_on;
static spin_lock_t      *s_lock;

static void log_out_chars(const char *buf, int len) {
    if (len <= 0) return;
    if (!s_ring_on) {
        uart_write_blocking(uart_default, (const uint8_t *)buf, (size_t)len);
        return;
    }
    uint32_t irq = spin_lock_blocking(s_lock);
    uint32_t head = s_head;
    if (LOG_RING - (head - s_tail) < (uint32_t)len) {
        s_dropped++;
    } else {
        for (int i = 0; i < len; i++) s_ring[head++ & (LOG_RING - 1u)] = buf[i];
        __dmb();                  /* the bytes land before the index moves */
        s_head = head;
    }
    spin_unlock(s_lock, irq);
}

static stdio_driver_t s_driver = {
    .out_chars = log_out_chars,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = true,         /* a terminal wants CR LF */
#endif
};

void log_init(void) {
    s_lock = spin_lock_init((uint)spin_lock_claim_unused(true));
    uart_init(uart_default, LOG_BAUD);
    /* GP2/GP3 are UART0's CTS and RTS pins; this picks their UART_AUX
     * function, which carries TX and RX there (§14.1). */
    gpio_set_function(PICO_DEFAULT_UART_TX_PIN,
                      UART_FUNCSEL_NUM(uart_default, PICO_DEFAULT_UART_TX_PIN));
    gpio_set_function(PICO_DEFAULT_UART_RX_PIN,
                      UART_FUNCSEL_NUM(uart_default, PICO_DEFAULT_UART_RX_PIN));
    stdio_set_driver_enabled(&s_driver, true);
}

void log_start_ring(void) {
    s_ring_on = true;
}

void log_pump(void) {
    uint32_t tail = s_tail;
    uint32_t head = s_head;
    if (tail == head) return;
    __dmb();                      /* read the bytes after seeing the index */
    while (tail != head && uart_is_writable(uart_default)) {
        uart_putc_raw(uart_default, s_ring[tail++ & (LOG_RING - 1u)]);
    }
    __dmb();
    s_tail = tail;
}

uint32_t log_dropped(void) {
    return s_dropped;
}

uint32_t log_free(void) {
    return s_ring_on ? LOG_RING - (s_head - s_tail) : LOG_RING;
}

#else /* no UART: nowhere to log */

void log_init(void) {}
void log_start_ring(void) {}
void log_pump(void) {}
uint32_t log_dropped(void) { return 0; }
uint32_t log_free(void) { return 0; }

#endif
