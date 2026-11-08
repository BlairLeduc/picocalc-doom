/* pico/time.h on the host: see hardware/gpio.h here.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_FAKE_TIME_H
#define PICOCALC_DOOM_FAKE_TIME_H

#include <stdint.h>

void busy_wait_us(uint64_t us);
#define tight_loop_contents() ((void)0)

#endif
