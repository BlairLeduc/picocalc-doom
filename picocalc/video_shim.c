/*
 * picocalc-doom — DG_DrawFrame() (design.md §5.3, §7.2).
 *
 * Upstream's I_FinishUpdate() has copied the frame into DG_ScreenBuffer;
 * M3 publishes it and its palette to the frame pool for core 1 to present
 * at y = 40, and claims the next buffer. Until then the frame is only
 * counted, with core 0's stack, for the heartbeat (§14.3, M1).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "doomgeneric.h"
#include "doomtype.h"

#include "core1.h"

extern boolean palette_changed;

void DG_DrawFrame(void)
{
    palette_changed = false;
    g_core0_frames++;
    g_core0_stack = core0_stack_used();
}
