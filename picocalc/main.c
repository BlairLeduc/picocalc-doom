/*
 * picocalc-doom — entry point (design.md §5.3, §13.1).
 *
 * M0: just enough to link the engine. The clocks, rail and flash timing
 * (§11.1), the log ring (§14.2), the banner and core 1 arrive in M1 from
 * Quest's main.c and board.c; until then this runs at the SDK's default
 * clock with the SDK's UART stdio.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "pico/stdlib.h"

#include "doomgeneric.h"
#include "picocalc_doom_version.h"

#include <stdio.h>

int main(void)
{
    stdio_init_all();
    printf("picocalc-doom %s\n", PICOCALC_DOOM_VERSION);

    static char *argv[] = { "doom", NULL };
    doomgeneric_Create(1, argv);

    for (;;) {
        doomgeneric_Tick();
    }
}
