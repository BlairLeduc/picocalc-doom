/*
 * picocalc-doom — frame hashes (design.md §15). See frame_hash.h.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "frame_hash.h"

/* CRC-32's reflected polynomial 0xEDB88320, a nibble at a time. */
static const uint32_t crc_nibble[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
};

uint32_t frame_hash_crc32(uint32_t crc, const void *data, size_t len)
{
    const uint8_t *p = data;
    crc = ~crc;
    while (len--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ crc_nibble[crc & 0xF];
        crc = (crc >> 4) ^ crc_nibble[crc & 0xF];
    }
    return ~crc;
}

uint32_t frame_hash_frame(const uint8_t *pixels, size_t len, const uint8_t *rgb)
{
    uint32_t crc = frame_hash_crc32(0, pixels, len);
    return frame_hash_crc32(crc, rgb, 256 * 3);
}

int frame_hash_note(frame_hash_tic_t *held, int tic, uint32_t crc, frame_hash_tic_t *done)
{
    int finished = 0;
    if (held->pending && tic != held->tic) {
        *done = *held;
        finished = 1;
        held->pending = 0;
    }
    if (held->pending) {
        held->crc = crc;
        held->frames++;
    } else {
        held->tic = tic;
        held->crc = crc;
        held->frames = 1;
        held->pending = 1;
    }
    return finished;
}

int frame_hash_flush(frame_hash_tic_t *held, frame_hash_tic_t *done)
{
    if (!held->pending) {
        return 0;
    }
    *done = *held;
    held->pending = 0;
    return 1;
}
