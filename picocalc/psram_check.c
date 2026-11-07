/* psram_check.c — a round trip through the PSRAM before the heap is put
 * on it (design.md §11.3).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdbool.h>

#include "psram_check.h"

#include "hardware/xip_cache.h"
#include "pico/time.h"

#define PSRAM_BASE  0x11000000u
#define PSRAM_BYTES (8u * 1024u * 1024u)   /* FRANK_DOOM_PSRAM_SIZE_BYTES */

/* 64 blocks of 1 KB, one every 128 KB, each at an odd word into its
 * stride so the blocks do not all start on the same cache set. */
#define SPREAD_BLOCKS 64u
#define SPREAD_WORDS  256u
#define SPREAD_STRIDE (PSRAM_BYTES / SPREAD_BLOCKS)
_Static_assert(SPREAD_STRIDE >= SPREAD_WORDS * 4u + 4u * 63u, "spread blocks overlap");

static volatile uint32_t *word_at(uint32_t offset) {
    return (volatile uint32_t *)(PSRAM_BASE + offset);
}

/* Every write is made to reach the chip, and every read to come from it. */
static void round_trip(void) {
    xip_cache_clean_all();
    xip_cache_invalidate_all();
}

static void check_word(psram_check_t *r, uint32_t offset, uint32_t want) {
    uint32_t got = *word_at(offset);
    r->words++;
    if (got != want) {
        if (!r->bad) {
            r->first_bad = offset;
            r->got = got;
            r->want = want;
        }
        r->bad++;
    }
}

/* Distinct, and different in many bits from its neighbours'. */
static uint32_t pattern(uint32_t offset) {
    uint32_t x = offset * 0x9e3779b1u + 0x7f4a7c15u;
    x ^= x >> 15;
    x *= 0x2c1b3c6du;
    x ^= x >> 12;
    return x;
}

static uint32_t spread_offset(unsigned block, unsigned i) {
    return block * SPREAD_STRIDE + 4u * (block % 64u) + 4u * i;
}

static void spread(psram_check_t *r, uint32_t invert) {
    for (unsigned b = 0; b < SPREAD_BLOCKS; b++)
        for (unsigned i = 0; i < SPREAD_WORDS; i++) {
            uint32_t o = spread_offset(b, i);
            *word_at(o) = pattern(o) ^ invert;
        }
    round_trip();
    for (unsigned b = 0; b < SPREAD_BLOCKS; b++)
        for (unsigned i = 0; i < SPREAD_WORDS; i++) {
            uint32_t o = spread_offset(b, i);
            check_word(r, o, pattern(o) ^ invert);
        }
}

bool psram_check(psram_check_t *r) {
    *r = (psram_check_t){ 0 };
    uint32_t t0 = time_us_32();

    /* Address lines: offset 0, then 4, 8, 16, ... 4 MB. Written low to
     * high, so an alias overwrites the lower word and is read back
     * wrong there. */
    *word_at(0) = pattern(0);
    for (uint32_t o = 4u; o < PSRAM_BYTES; o <<= 1) *word_at(o) = pattern(o);
    round_trip();
    check_word(r, 0, pattern(0));
    for (uint32_t o = 4u; o < PSRAM_BYTES; o <<= 1) check_word(r, o, pattern(o));

    spread(r, 0u);
    spread(r, 0xffffffffu);

    r->us = time_us_32() - t0;
    return r->bad == 0;
}
