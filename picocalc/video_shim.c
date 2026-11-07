/*
 * picocalc-doom — DG_DrawFrame() (design.md §5.3, §7.2).
 *
 * M0: an empty shim. upstream's I_FinishUpdate() has copied the frame into
 * DG_ScreenBuffer; M3 publishes it and its palette to the frame pool for
 * core 1 to present at y = 40, and claims the next buffer.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "doomgeneric.h"
#include "doomtype.h"

extern boolean palette_changed;

void DG_DrawFrame(void)
{
    palette_changed = false;
}
