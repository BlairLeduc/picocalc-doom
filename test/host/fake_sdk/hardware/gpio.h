/* hardware/gpio.h on the host, for picocalc_shell.h: declarations only.
 * Its card and loader functions are not called on the host, so nothing
 * here is defined (test_shell_args uses only the arguments).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_FAKE_GPIO_H
#define PICOCALC_DOOM_FAKE_GPIO_H

#include <stdbool.h>

enum { GPIO_IN = 0, GPIO_OUT = 1 };
void gpio_init(unsigned gpio);
void gpio_set_dir(unsigned gpio, int out);
void gpio_pull_up(unsigned gpio);
bool gpio_get(unsigned gpio);

#endif
