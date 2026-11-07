/*
 * picocalc-doom — host test: frame hashes (design.md §15).
 *
 * The nibble-table CRC-32 against a bitwise model and zlib's published
 * check value; the frame hash as pixels then palette; and the tic
 * bookkeeping: one hash a tic, the last frame of a wipe, a flush at the end.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "frame_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

/* An independent model: bit at a time. */
static uint32_t crc32_bitwise(uint32_t crc, const uint8_t *p, size_t len)
{
    crc = ~crc;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static void test_crc(void)
{
    /* The CRC-32 check value. */
    CHECK(frame_hash_crc32(0, "123456789", 9) == 0xCBF43926u);
    CHECK(frame_hash_crc32(0, "", 0) == 0);

    /* Random buffers, whole and in pieces. */
    static uint8_t buf[4099];
    unsigned seed = 12345;
    for (int round = 0; round < 50; round++) {
        size_t len = (size_t)(seed % sizeof buf);
        for (size_t i = 0; i < len; i++) {
            seed = seed * 1103515245u + 12345u;
            buf[i] = (uint8_t)(seed >> 16);
        }
        uint32_t want = crc32_bitwise(0, buf, len);
        CHECK(frame_hash_crc32(0, buf, len) == want);
        size_t cut = len / 3;
        CHECK(frame_hash_crc32(frame_hash_crc32(0, buf, cut), buf + cut, len - cut) == want);
    }
}

static void test_frame(void)
{
    static uint8_t pixels[320 * 240];
    static uint8_t rgb[256 * 3];
    static uint8_t joined[sizeof pixels + sizeof rgb];
    for (size_t i = 0; i < sizeof pixels; i++) {
        pixels[i] = (uint8_t)(i * 7 + (i >> 9));
    }
    for (size_t i = 0; i < sizeof rgb; i++) {
        rgb[i] = (uint8_t)(255 - i);
    }
    memcpy(joined, pixels, sizeof pixels);
    memcpy(joined + sizeof pixels, rgb, sizeof rgb);
    uint32_t h = frame_hash_frame(pixels, sizeof pixels, rgb);
    CHECK(h == crc32_bitwise(0, joined, sizeof joined));

    /* A palette change alone changes the hash. */
    rgb[3 * 200 + 1] ^= 1;
    CHECK(frame_hash_frame(pixels, sizeof pixels, rgb) != h);
}

static void test_tics(void)
{
    frame_hash_tic_t held = { 0 }, done = { 0 };

    CHECK(!frame_hash_flush(&held, &done));

    /* Tic 1: one frame, final when tic 2's arrives. */
    CHECK(!frame_hash_note(&held, 1, 0x11, &done));
    /* Tic 2: a wipe, three frames; the last stands. */
    CHECK(frame_hash_note(&held, 2, 0x21, &done));
    CHECK(done.tic == 1 && done.crc == 0x11 && done.frames == 1);
    CHECK(!frame_hash_note(&held, 2, 0x22, &done));
    CHECK(!frame_hash_note(&held, 2, 0x23, &done));
    CHECK(frame_hash_note(&held, 3, 0x31, &done));
    CHECK(done.tic == 2 && done.crc == 0x23 && done.frames == 3);

    /* A new game starts the count again: tic 0 after tic 3. */
    CHECK(frame_hash_note(&held, 0, 0x01, &done));
    CHECK(done.tic == 3 && done.crc == 0x31);

    CHECK(frame_hash_flush(&held, &done));
    CHECK(done.tic == 0 && done.crc == 0x01 && done.frames == 1);
    CHECK(!frame_hash_flush(&held, &done));
}

int main(void)
{
    test_crc();
    test_frame();
    test_tics();
    if (failures) {
        fprintf(stderr, "test_frame_hash: %d failed\n", failures);
        return 1;
    }
    printf("test_frame_hash: passed\n");
    return 0;
}
