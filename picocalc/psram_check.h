/* psram_check.h — a round trip through the PSRAM before the heap is put
 * on it (design.md §11.3; hardware-notes.md §2.4).
 *
 * Upstream's psram_init() sets the QMI up and never checks that the chip
 * answers at speed. A marginal timing does not fault: it returns
 * occasional bad data. So before the allocator gets the region, this
 * writes patterns through the cached window, cleans and invalidates the
 * XIP cache so every word makes the trip to the chip and back, and reads
 * them back: one word at every power-of-two offset, which catches a chip
 * smaller than it should be or an address line that aliases, and a block
 * of pseudo-random words at offsets spread over the whole 8 MB, then the
 * same inverted. It overwrites what was there, so it runs before anything
 * is stored in the PSRAM.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_PSRAM_CHECK_H
#define PICOCALC_DOOM_PSRAM_CHECK_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t words;       /* checked                                   */
    uint32_t bad;         /* that came back different                  */
    uint32_t first_bad;   /* the first bad word's offset, if any       */
    uint32_t got, want;   /* what it held, and what it should have     */
    uint32_t us;          /* how long the whole check took             */
} psram_check_t;

/* Returns true when every word came back as written. */
bool psram_check(psram_check_t *r);

#endif /* PICOCALC_DOOM_PSRAM_CHECK_H */
