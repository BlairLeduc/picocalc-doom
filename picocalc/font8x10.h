/* font8x10.h — the 8x10 font for the pages core 1 draws (design.md
 * §5.5): FONT_GLYPHS glyphs of FONT_H bytes, a byte a row, bit 7 leftmost.
 * From picocalc-quest (D8), which took it from picocalc-text-starter.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef PICOCALC_DOOM_FONT8X10_H
#define PICOCALC_DOOM_FONT8X10_H

#include <stdint.h>

#define FONT_W 8u
#define FONT_H 10u
#define FONT_GLYPHS 130u   /* 0x00-0x81; ASCII and two more */

extern const uint8_t font_8x10[FONT_GLYPHS * FONT_H];

#endif /* PICOCALC_DOOM_FONT8X10_H */
