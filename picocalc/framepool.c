/* framepool.c — the core 0 -> core 1 frame handoff (design.md §7.2).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "framepool.h"

#include <string.h>

/* Hot code goes in SRAM on the device, out of the XIP cache core 0's
 * engine needs (§7.3); the SDK's __not_in_flash_func, without the SDK. */
#ifdef PICOCALC_DOOM_HOST
#define FRAME_IN_RAM(name) name
#else
#define FRAME_IN_RAM(name) __attribute__((section(".time_critical." #name))) name
#endif

void framepool_init(framepool_t *p, uint8_t *const pixels[FRAMEPOOL_COUNT]) {
    memset(p, 0, sizeof(*p));
    for (unsigned i = 0; i < FRAMEPOOL_COUNT; i++) {
        p->pixels[i] = pixels[i];
        p->state[i] = FRAME_FREE;
        p->margins_black[i] = 1;
    }
}

static int find(const framepool_t *p, frame_state_t s) {
    for (unsigned i = 0; i < FRAMEPOOL_COUNT; i++)
        if (p->state[i] == s) return (int)i;
    return -1;
}

int framepool_claim(framepool_t *p) {
    int i = find(p, FRAME_FREE);
    if (i >= 0) p->state[i] = FRAME_FILLING;
    return i;
}

void framepool_publish(framepool_t *p, int i) {
    if (i < 0 || i >= FRAMEPOOL_COUNT || p->state[i] != FRAME_FILLING) return;

    /* At most one buffer is ever ready, so "the newest ready one" is
     * just "the ready one", and core 1 needs no sequence numbers to pick
     * between them. */
    for (unsigned j = 0; j < FRAMEPOOL_COUNT; j++) {
        if (p->state[j] == FRAME_READY) {
            p->state[j] = FRAME_FREE;
            p->dropped++;
        }
    }
    p->state[i] = FRAME_READY;
    p->published++;
}

int framepool_take(framepool_t *p) {
    int i = find(p, FRAME_READY);
    if (i >= 0) p->state[i] = FRAME_RENDERING;
    return i;
}

void framepool_release(framepool_t *p, int i) {
    if (i >= 0 && i < FRAMEPOOL_COUNT && p->state[i] == FRAME_RENDERING) p->state[i] = FRAME_FREE;
}

int framepool_margins(framepool_t *p, int i, int in_level) {
    if (i < 0 || i >= FRAMEPOOL_COUNT) return 0;
    if (in_level) {
        p->margins_black[i] = 0;
        return 0;
    }
    if (p->margins_black[i]) return 0;
    p->margins_black[i] = 1;
    return 1;
}

uint16_t frame_rgb565(uint32_t rgb888) {
    uint32_t r = (rgb888 >> 16) & 0xffu;
    uint32_t g = (rgb888 >> 8) & 0xffu;
    uint32_t b = rgb888 & 0xffu;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void FRAME_IN_RAM(frame_expand_row)(const uint8_t *src, const uint16_t *pal, uint16_t *dst,
                                    unsigned n) {
    for (unsigned i = 0; i < n; i += 4u) {
        uint32_t w;
        memcpy(&w, src + i, sizeof(w));    /* one load; the bytes in memory order */
        dst[i]      = pal[w & 0xffu];
        dst[i + 1u] = pal[(w >> 8) & 0xffu];
        dst[i + 2u] = pal[(w >> 16) & 0xffu];
        dst[i + 3u] = pal[w >> 24];
    }
}
