/* hardware/structs/watchdog.h on the host: see hardware/gpio.h here.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_FAKE_WATCHDOG_STRUCTS_H
#define PICOCALC_DOOM_FAKE_WATCHDOG_STRUCTS_H

#include <stdint.h>

typedef struct {
    uint32_t scratch[8];
} watchdog_hw_t;

extern watchdog_hw_t fake_watchdog;
#define watchdog_hw (&fake_watchdog)

#endif
