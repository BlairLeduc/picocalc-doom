/*
 * picocalc-doom — frame hashes (design.md §15).
 *
 * A CRC-32 of each frame Doom hands the platform: its 320 × 240 indexed
 * pixels, then its palette as 256 red, green, blue triples. The host engine
 * writes the list for -timedemo demo1; the device writes the same list,
 * and the two agree when the engine on the board is the host's (EL §13.3).
 *
 * One hash per game tic, the last frame drawn in it. Under -timedemo Doom
 * runs one tic per frame, except during a screen wipe, which draws several
 * frames in one tic, as many as the real time it takes; the last of them is
 * the finished wipe, the same on any machine.
 *
 * Portable: no Doom or Pico SDK headers, so the host tests build it.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PICOCALC_DOOM_FRAME_HASH_H
#define PICOCALC_DOOM_FRAME_HASH_H

#include <stddef.h>
#include <stdint.h>

/* CRC-32 (IEEE 802.3, reflected, as zlib's crc32()): start with 0, pass
 * the result back to continue. A 16-entry table, 64 bytes, for the device's
 * SRAM; about two lookups a byte. */
uint32_t frame_hash_crc32(uint32_t crc, const void *data, size_t len);

/* The hash of one frame: pixels, then the palette, rgb[256 * 3]. */
uint32_t frame_hash_frame(const uint8_t *pixels, size_t len, const uint8_t *rgb);

/* One tic's hash, kept until a frame from a later tic shows it final. */
typedef struct {
    int tic;
    uint32_t crc;
    unsigned frames;    /* frames drawn in the tic; above 1 in a wipe */
    int pending;        /* nonzero once a frame is held */
} frame_hash_tic_t;

/* Note a frame drawn at game tic `tic`. When it belongs to a later tic than
 * the one held, the held tic is final: it is copied to *done and 1 is
 * returned. A frame from an earlier tic than the one held (a new game)
 * also finishes it. */
int frame_hash_note(frame_hash_tic_t *held, int tic, uint32_t crc, frame_hash_tic_t *done);

/* At the end: the held tic, if any, to *done, returning 1. */
int frame_hash_flush(frame_hash_tic_t *held, frame_hash_tic_t *done);

#endif
