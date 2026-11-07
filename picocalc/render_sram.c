/*
 * picocalc-doom — the gate's renderer levers in SRAM (design.md §16 M5).
 *
 * Lever 2, PICOCALC_DOOM_COLORMAPS_SRAM: the light tables in SRAM. Every
 * pixel Doom draws is looked up in COLORMAP (34 maps of 256 bytes), which
 * upstream caches in the zone, in the PSRAM, where it competes with the
 * code and the textures for the 16 KB XIP cache. R_Init() is wrapped at
 * link time: after it, the lump is copied into SRAM, `colormaps` pointed
 * at the copy, and R_InitLightTables() run again, since it stored
 * pointers into the old copy in zlight[][]. The other tables built from
 * `colormaps` (scalelight[][], the fixed colormap, the sprites') are built
 * later, each frame or at R_ExecuteSetViewSize(), from the new pointer.
 * The zone's copy stays allocated, so the zone's layout is the host
 * engine's and the frame hashes compare (§15).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Doom's headers first (doomtype.h's boolean). */
#include "doomtype.h"
#include "r_data.h"
#include "r_main.h"
#include "r_state.h"
#include "w_wad.h"

#include "frank_doom_log.h"

#include <string.h>

#ifndef PICOCALC_DOOM_COLORMAPS_SRAM
#define PICOCALC_DOOM_COLORMAPS_SRAM 0
#endif

#if PICOCALC_DOOM_COLORMAPS_SRAM

/* NUMCOLORMAPS light levels, the invulnerability map and an all-black
 * one: 34 maps, as every IWAD's COLORMAP. */
#define COLORMAP_BYTES (34u * 256u)

extern lighttable_t *colormaps;
void R_InitLightTables(void);   /* r_main.c's; in no header */

static lighttable_t s_colormaps[COLORMAP_BYTES] __attribute__((aligned(4)));

void __real_R_Init(void);

void __wrap_R_Init(void)
{
    __real_R_Init();
    int len = W_LumpLength(W_GetNumForName("COLORMAP"));
    if (len < (int)COLORMAP_BYTES) {
        FRANK_DOOM_LOG("picocalc-doom: COLORMAP is %d bytes; left in the zone\n", len);
        (void)len;
        return;
    }
    memcpy(s_colormaps, colormaps, COLORMAP_BYTES);
    colormaps = s_colormaps;
    R_InitLightTables();
    FRANK_DOOM_LOG("picocalc-doom: COLORMAP in SRAM, %u bytes\n", COLORMAP_BYTES);
}

#endif
