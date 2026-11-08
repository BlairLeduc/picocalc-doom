/* hardware/watchdog.h on the host: see hardware/gpio.h here.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_FAKE_WATCHDOG_H
#define PICOCALC_DOOM_FAKE_WATCHDOG_H

#include <stdint.h>

void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms);

#endif
