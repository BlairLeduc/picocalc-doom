/*
 * picocalc-doom — DG_GetKey() (design.md §5.3, §9).
 *
 * M0: an empty shim; no key is ever pressed. M4 pops the key map's queue
 * of Doom key events, built on core 0 from the southbridge's events.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "doomgeneric.h"

int DG_GetKey(int *pressed, unsigned char *key)
{
    (void)pressed;
    (void)key;
    return 0;
}
