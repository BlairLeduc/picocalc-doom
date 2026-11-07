/*
 * picocalc-doom — upstream's I2S API over PWM (design.md §5.3, §8.2), in
 * place of upstream's src/pico/audio.c, which is not compiled.
 *
 * M0: an empty shim. Upstream's sound module (i_picosound.c) calls these
 * unchanged; nothing plays, and its fill callback is never called. M6
 * implements them over the PWM ring on GP26/27 at 49,718 Hz (D11).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "audio.h"

static i2s_fill_cb_t fill_cb;

void i2s_init(i2s_config_t *config)
{
    (void)config;
}

void i2s_deinit(i2s_config_t *config)
{
    (void)config;
}

void i2s_set_fill_callback(i2s_fill_cb_t cb)
{
    fill_cb = cb;
}

void i2s_start(void)
{
    (void)fill_cb;
}

/* The push API, which the sound module does not use with a fill callback
 * set; here for the link (§8.2). */

void i2s_dma_write(i2s_config_t *config, const int16_t *samples)
{
    (void)config;
    (void)samples;
}

bool i2s_dma_write_nb(i2s_config_t *config, const int16_t *samples)
{
    (void)config;
    (void)samples;
    return true;
}

bool i2s_is_buffer_free(void)
{
    return true;
}

void i2s_volume(i2s_config_t *config, uint8_t volume)
{
    (void)config;
    (void)volume;
}

void i2s_increase_volume(i2s_config_t *config)
{
    (void)config;
}

void i2s_decrease_volume(i2s_config_t *config)
{
    (void)config;
}
