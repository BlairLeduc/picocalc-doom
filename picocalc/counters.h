/* counters.h — the heartbeat's counters in one block at a fixed symbol,
 * for reading over SWD with both cores running (design.md §14.2, §16 M10;
 * hardware-notes.md §2.7). From picocalc-quest's counters.h (D8).
 *
 * The build that ships has no UART, so this block is how a soak of it is
 * watched: OpenOCD started with `init` and no `reset` or `halt` reads it
 * through the debug port (scripts/counters.sh), and finds it by its
 * symbol, picocalc_doom_counters, with nm. Core 1 fills it at each
 * heartbeat, in both builds. A field is a whole word and is written once
 * a second, so a read may mix two heartbeats but never tears a word.
 *
 * The layout is part of the interface with scripts/counters.sh: a field
 * is only ever added at the end, before the version, with COUNTERS_LAYOUT
 * raised.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_COUNTERS_H
#define PICOCALC_DOOM_COUNTERS_H

#include <stdint.h>

#define COUNTERS_MAGIC  0x4d4f4f44u   /* "DOOM", little-endian */
#define COUNTERS_LAYOUT 1u

typedef struct {
    uint32_t magic;          /* COUNTERS_MAGIC once core 1 is running    */
    uint32_t layout;         /* COUNTERS_LAYOUT                          */
    uint32_t seconds;        /* heartbeats since boot                    */
    uint32_t busy;           /* core 0's busy share, last second, 1/1000 */
    uint32_t busy_max;       /* the highest second's since boot          */
    uint32_t rendered;       /* frames Doom drew, since boot             */
    uint32_t published;      /* frames published to core 1, since boot   */
    uint32_t presents;       /* sent to the panel, since boot            */
    uint32_t dropped;        /* published and never presented            */
    uint32_t present_max_us; /* the longest present since boot           */
    uint32_t late;           /* late audio refills since boot            */
    uint32_t underruns;      /* mix slots upstream's ring was empty for  */
    uint32_t music_late;     /* halves silent for want of core 1's OPL   */
    uint32_t opl_dropped;    /* OPL writes the queue had no room for     */
    uint32_t keys;           /* MCU pairs taken from the ring            */
    uint32_t ring_full;      /* key events lost to a full ring           */
    uint32_t unknown;        /* keys not in the map                      */
    uint32_t stray;          /* releases with no press                   */
    uint32_t i2c_errors;     /* southbridge transactions that failed     */
    uint32_t log_dropped;    /* log lines dropped (development build)    */
    uint32_t core0_stack;    /* bytes of core 0's stack ever used        */
    uint32_t core1_stack;    /* bytes of core 1's stack ever used        */
    uint32_t heap;           /* bytes of the SRAM heap in use            */
    uint32_t tic;            /* Doom's game tic                          */
    uint32_t leveltime;      /* the level's tic                          */
    uint32_t gamestate;      /* Doom's gamestate_t                       */
    int32_t  temp_c;         /* die temperature                          */
    int32_t  battery;        /* the MCU's byte (bit 7 charging), or -1   */
    uint32_t loader;         /* 1 if UF2 Loader started the image        */
    uint32_t image_kb;       /* the image's size in flash                */
    uint32_t partition_kb;   /* UF2 Loader's app partition; 0 if none    */
    uint32_t hashes;         /* frame hashes in the list (-framehashes)  */
    uint32_t hash_list;      /* the list's address: TIC, CRC; 0 if none  */
    char     version[32];    /* git describe, NUL-terminated             */
} counters_t;

/* scripts/counters.sh reads 41 words: 33 fields and the version's 8. */
_Static_assert(sizeof(counters_t) == 41u * 4u, "counters.sh reads 41 words");

extern volatile counters_t picocalc_doom_counters;

#endif /* PICOCALC_DOOM_COUNTERS_H */
