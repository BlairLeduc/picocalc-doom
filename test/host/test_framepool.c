/* test_framepool.c — the frame pool and the row expander (design.md
 * §7.2, §15).
 *
 * The transitions one by one, then a long random interleaving of both
 * cores' calls (the state machine has no lock of its own, so they can
 * be interleaved here at will), then the margins rule against a model of
 * upstream's copy, then the colour conversion and the row expander
 * against independent models. From picocalc-quest's (D8).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <string.h>

#include "framepool.h"

static int failures;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (!(cond)) {                                      \
            failures++;                                     \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                   \
            fputc('\n', stderr);                            \
        }                                                   \
    } while (0)

static framepool_t g_pool;
static uint8_t g_bufs[FRAMEPOOL_COUNT][64];

static unsigned rng_state = 12345;
static unsigned rng(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFFu;
}

static unsigned count(const framepool_t *p, frame_state_t s) {
    unsigned n = 0;
    for (unsigned i = 0; i < FRAMEPOOL_COUNT; i++) n += p->state[i] == s;
    return n;
}

static void test_transitions(void) {
    framepool_t *p = &g_pool;
    uint8_t *px[FRAMEPOOL_COUNT] = { g_bufs[0], g_bufs[1], g_bufs[2] };
    framepool_init(p, px);
    for (unsigned i = 0; i < FRAMEPOOL_COUNT; i++) {
        CHECK(p->pixels[i] == g_bufs[i], "buffer %u's pixels are the slot given", i);
    }
    CHECK(framepool_take(p) < 0, "nothing to take from an empty pool");

    int a = framepool_claim(p);
    CHECK(a >= 0, "claim from an empty pool should succeed");
    CHECK(framepool_take(p) < 0, "a filling buffer is invisible to core 1");
    framepool_publish(p, a);

    int r = framepool_take(p);
    CHECK(r == a, "core 1 should take the published buffer");

    /* Core 1 is a frame behind: core 0 publishes twice while it
     * presents. The first is superseded, and core 0 never runs dry. */
    int b = framepool_claim(p);
    framepool_publish(p, b);
    int c = framepool_claim(p);
    CHECK(c >= 0, "core 0 must never run out of buffers (the third buffer)");
    framepool_publish(p, c);
    CHECK(p->dropped == 1, "the older ready frame should be dropped, dropped=%u",
          (unsigned)p->dropped);
    int d = framepool_claim(p);
    CHECK(d == b, "the superseded buffer should be free again for core 0");

    framepool_release(p, r);
    CHECK(framepool_take(p) == c, "core 1 should get the newest frame, not the oldest");

    /* Out-of-state and out-of-range calls change nothing. */
    framepool_publish(p, r);
    framepool_release(p, d);
    framepool_publish(p, -1);
    framepool_publish(p, FRAMEPOOL_COUNT);
    framepool_release(p, -1);
    framepool_release(p, FRAMEPOOL_COUNT);
    CHECK(p->state[r] == FRAME_FREE && p->state[d] == FRAME_FILLING &&
              p->state[c] == FRAME_RENDERING,
          "publishing a free buffer, releasing a filling one, or an index out of "
          "range must be ignored");
    CHECK(p->published == 3 && p->dropped == 1, "the counters moved: published %u dropped %u",
          (unsigned)p->published, (unsigned)p->dropped);
}

/* Core 0 always gets a buffer, never the one core 1 holds; at most one
 * is ever ready; every publish is taken, dropped or still pending; and
 * the palette core 1 sees is the one published with the pixels. */
static void test_interleaving(void) {
    framepool_t *p = &g_pool;
    uint8_t *px[FRAMEPOOL_COUNT] = { g_bufs[0], g_bufs[1], g_bufs[2] };
    framepool_init(p, px);

    uint32_t frame = 0, taken = 0, last_seen = 0;
    int filling = framepool_claim(p), rendering = -1;
    for (unsigned step = 0; step < 200000; step++) {
        unsigned what = rng() % 3u;
        if (what == 0) {
            /* Core 0 draws frame n: its number in the pixels and in the
             * palette, then publishes. */
            frame++;
            memcpy(p->pixels[filling], &frame, sizeof(frame));
            p->palette[filling][0] = (uint16_t)frame;
            p->palette[filling][255] = (uint16_t)~frame;
            framepool_publish(p, filling);
            filling = framepool_claim(p);
            CHECK(filling >= 0 && filling != rendering,
                  "step %u: core 0 claimed %d while core 1 presents %d", step, filling,
                  rendering);
        } else if (what == 1 && rendering < 0) {
            rendering = framepool_take(p);
            if (rendering >= 0) {
                taken++;
                uint32_t n;
                memcpy(&n, p->pixels[rendering], sizeof(n));
                CHECK(n > last_seen, "step %u: core 1 saw frame %u after %u", step,
                      (unsigned)n, (unsigned)last_seen);
                CHECK(p->palette[rendering][0] == (uint16_t)n &&
                          p->palette[rendering][255] == (uint16_t)~n,
                      "step %u: frame %u came with another frame's palette", step, (unsigned)n);
                last_seen = n;
            }
        } else if (rendering >= 0) {
            framepool_release(p, rendering);
            rendering = -1;
        }
        CHECK(count(p, FRAME_READY) <= 1, "step %u: %u buffers ready at once", step,
              count(p, FRAME_READY));
        CHECK(count(p, FRAME_FILLING) == 1, "step %u: core 0 holds %u buffers", step,
              count(p, FRAME_FILLING));
        CHECK(count(p, FRAME_RENDERING) == (rendering >= 0 ? 1u : 0u),
              "step %u: core 1 holds %u buffers", step, count(p, FRAME_RENDERING));
    }
    unsigned pending = count(p, FRAME_READY);
    CHECK(taken + p->dropped + pending == p->published,
          "published %u = taken %u + dropped %u + pending %u", (unsigned)p->published,
          (unsigned)taken, (unsigned)p->dropped, pending);
    CHECK(p->published == frame, "published %u of %u frames", (unsigned)p->published,
          (unsigned)frame);
    printf("%u published, %u taken, %u dropped\n", (unsigned)p->published, (unsigned)taken,
           (unsigned)p->dropped);
}

/* The margins rule (§7.2). Each buffer is FRAMEPOOL_ROWS rows of one
 * value each. Core 0's copy is upstream's: in a level every row gets the
 * frame's number; outside one only rows 20-219 do, and on the change from
 * a level upstream blacks the margins of the buffer it is filling (0),
 * once. Then the rule: framepool_margins() says whether to black them
 * before publishing. Every frame published outside a level must have
 * black margins, whichever buffer it was and whatever it held before. */
static void test_margins(void) {
    framepool_t *p = &g_pool;
    uint8_t *px[FRAMEPOOL_COUNT] = { g_bufs[0], g_bufs[1], g_bufs[2] };
    framepool_init(p, px);

    static uint32_t buf[FRAMEPOOL_COUNT][FRAMEPOOL_ROWS];   /* cleared: black */
    const unsigned lo = FRAMEPOOL_MARGIN_ROWS, hi = FRAMEPOOL_ROWS - FRAMEPOOL_MARGIN_ROWS;
    int in_level = 1, prev_in_level = 1, filling = framepool_claim(p), rendering = -1;
    uint32_t frame = 0, blacked = 0, outside = 0;
    for (unsigned step = 0; step < 100000; step++) {
        unsigned what = rng() % 8u;
        if (what == 0 && rng() % 4u == 0) {
            in_level = !in_level;    /* a level starts or ends */
        } else if (what <= 4) {
            frame++;
            uint32_t *b = buf[filling];
            if (in_level) {
                for (unsigned r = 0; r < FRAMEPOOL_ROWS; r++) b[r] = frame;
            } else {
                if (prev_in_level)
                    for (unsigned r = 0; r < FRAMEPOOL_ROWS; r++)
                        if (r < lo || r >= hi) b[r] = 0;
                for (unsigned r = lo; r < hi; r++) b[r] = frame;
            }
            prev_in_level = in_level;
            if (framepool_margins(p, filling, in_level)) {
                for (unsigned r = 0; r < FRAMEPOOL_ROWS; r++)
                    if (r < lo || r >= hi) b[r] = 0;
                blacked++;
            }
            if (!in_level) {
                outside++;
                for (unsigned r = 0; r < FRAMEPOOL_ROWS; r++) {
                    if ((r < lo || r >= hi) && b[r] != 0) {
                        CHECK(0, "step %u: frame %u outside a level, row %u holds frame %u",
                              step, (unsigned)frame, r, (unsigned)b[r]);
                        break;
                    }
                }
            }
            for (unsigned r = lo; r < hi; r++) {
                if (b[r] != frame) {
                    CHECK(0, "step %u: frame %u's row %u holds %u", step, (unsigned)frame, r,
                          (unsigned)b[r]);
                    break;
                }
            }
            framepool_publish(p, filling);
            filling = framepool_claim(p);
        } else if (what == 5 && rendering < 0) {
            rendering = framepool_take(p);
        } else if (rendering >= 0) {
            framepool_release(p, rendering);
            rendering = -1;
        }
    }
    /* Each change from a level costs at most one blacking per buffer. */
    CHECK(blacked > 0 && blacked < outside / 4u,
          "the margins were blacked %u times in %u frames outside a level",
          (unsigned)blacked, (unsigned)outside);
    printf("margins: %u frames outside a level, margins blacked %u times\n",
           (unsigned)outside, (unsigned)blacked);

    /* A fresh pool's buffers are black (the caller cleared them), and an
     * index out of range is ignored. */
    framepool_init(p, px);
    CHECK(!framepool_margins(p, 0, 0) && !framepool_margins(p, -1, 0) &&
              !framepool_margins(p, FRAMEPOOL_COUNT, 0),
          "a fresh buffer's margins are black; an index out of range is ignored");
    CHECK(!framepool_margins(p, 1, 1) && framepool_margins(p, 1, 0) &&
              !framepool_margins(p, 1, 0),
          "after a level, once; then not again");
}

/* RGB565 by a different route: scale each channel down by division. */
static uint16_t model_rgb565(unsigned r, unsigned g, unsigned b) {
    return (uint16_t)((r * 32u / 256u) * 2048u + (g * 64u / 256u) * 32u + b * 32u / 256u);
}

static void test_colour(void) {
    for (unsigned r = 0; r < 256; r += 5) {
        for (unsigned g = 0; g < 256; g += 3) {
            for (unsigned b = 0; b < 256; b += 7) {
                uint16_t got = frame_rgb565((r << 16) | (g << 8) | b);
                CHECK(got == model_rgb565(r, g, b), "%02x%02x%02x -> %04x, want %04x", r, g, b,
                      got, model_rgb565(r, g, b));
            }
        }
    }
    CHECK(frame_rgb565(0xFFFFFFu) == 0xFFFF && frame_rgb565(0) == 0, "white and black");
    CHECK(frame_rgb565(0xFF000000u | 0x0000FFu) == 0x001F, "the top byte is ignored");
    CHECK(frame_rgb565(0xFF0000u) == 0xF800 && frame_rgb565(0x00FF00u) == 0x07E0 &&
              frame_rgb565(0x0000FFu) == 0x001F,
          "red, green and blue each land in their own field");
}

static void test_expand(void) {
    uint16_t pal[256];
    for (unsigned i = 0; i < 256; i++) pal[i] = (uint16_t)(i * 257u ^ 0x5A5Au);

    /* A row of every index, from offsets in a larger buffer that the
     * device's word loads must not care about on the host. */
    static uint8_t src[320 + 8];
    uint16_t dst[320 + 4];
    for (unsigned off = 0; off < 4; off++) {
        for (unsigned i = 0; i < 320; i++) src[off + i] = (uint8_t)(i * 7u + off);
        dst[320] = 0xBEEF;
        frame_expand_row(src + off, pal, dst, 320);
        for (unsigned i = 0; i < 320; i++) {
            if (dst[i] != pal[src[off + i]]) {
                CHECK(0, "offset %u, pixel %u: %04x, want %04x", off, i, dst[i],
                      pal[src[off + i]]);
                break;
            }
        }
        CHECK(dst[320] == 0xBEEF, "offset %u: wrote past the row", off);
    }

    /* Four pixels, the smallest row. */
    const uint8_t four[4] = { 0, 255, 1, 128 };
    uint16_t out[4];
    frame_expand_row(four, pal, out, 4);
    CHECK(out[0] == pal[0] && out[1] == pal[255] && out[2] == pal[1] && out[3] == pal[128],
          "four pixels in order");
}

int main(void) {
    test_transitions();
    test_interleaving();
    test_margins();
    test_colour();
    test_expand();
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_framepool: ok\n");
    return 0;
}
