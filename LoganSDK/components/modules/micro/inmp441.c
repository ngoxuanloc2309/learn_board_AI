#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "inmp441.h"

/* The microphone sends 24 bit data MSB first in a 32 bit slot, so slots are read as 32 bit words. */
#define SLOT_BITS           32U
#define SLOT_BYTES          4U

static int map_i2s(int ret)
{
    switch (ret) {
    case SX_I2S_OK:             return INMP441_OK;
    case SX_I2S_ERR_PARAM:      return INMP441_ERR_PARAM;
    case SX_I2S_ERR_STATE:      return INMP441_ERR_STATE;
    case SX_I2S_ERR_TIMEOUT:    return INMP441_ERR_TIMEOUT;
    default:                    return INMP441_ERR_BUS;
    }
}

static bool valid_config(const inmp441_config_t *config)
{
    return (config != NULL) && (config->port >= 0) && (config->bclk_pin >= 0) &&
           (config->ws_pin >= 0) && (config->din_pin >= 0) && (config->sample_rate_hz != 0U);
}

int inmp441_init(inmp441_t *dev, const inmp441_config_t *config)
{
    if ((dev == NULL) || !valid_config(config)) {
        return INMP441_ERR_PARAM;
    }
    if (dev->ready) {
        return INMP441_ERR_STATE;
    }

    sx_i2s_config_t i2s = {
        .port = config->port,
        .bclk_pin = config->bclk_pin,
        .ws_pin = config->ws_pin,
        .dout_pin = SX_I2S_PIN_UNUSED,
        .din_pin = config->din_pin,
        .sample_rate_hz = config->sample_rate_hz,
        .bits_per_sample = (uint8_t)SLOT_BITS,
        .channels = SX_I2S_MONO,
    };

    int ret = map_i2s(sx_i2s_init(&i2s));
    if (ret != INMP441_OK) {
        return ret;
    }

    dev->i2s = i2s;
    dev->started = false;
    dev->ready = true;
    return INMP441_OK;
}

int inmp441_deinit(inmp441_t *dev)
{
    if (dev == NULL) {
        return INMP441_ERR_PARAM;
    }
    if (!dev->ready) {
        return INMP441_ERR_STATE;
    }

    /* sx_i2s_deinit stops a running port by itself. */
    int ret = map_i2s(sx_i2s_deinit(&dev->i2s));
    if (ret == INMP441_OK) {
        dev->started = false;
        dev->ready = false;
    }
    return ret;
}

int inmp441_start(inmp441_t *dev)
{
    if (dev == NULL) {
        return INMP441_ERR_PARAM;
    }
    if (!dev->ready || dev->started) {
        return INMP441_ERR_STATE;
    }

    int ret = map_i2s(sx_i2s_start(&dev->i2s));
    if (ret == INMP441_OK) {
        dev->started = true;
    }
    return ret;
}

int inmp441_stop(inmp441_t *dev)
{
    if (dev == NULL) {
        return INMP441_ERR_PARAM;
    }
    if (!dev->ready || !dev->started) {
        return INMP441_ERR_STATE;
    }

    int ret = map_i2s(sx_i2s_stop(&dev->i2s));
    if (ret == INMP441_OK) {
        dev->started = false;
    }
    return ret;
}

int inmp441_read(inmp441_t *dev, int16_t *samples, size_t max_samples, size_t *count,
                 uint32_t timeout_ms)
{
    if ((dev == NULL) || (samples == NULL) || (max_samples == 0U) || (count == NULL)) {
        return INMP441_ERR_PARAM;
    }
    *count = 0U;
    if (!dev->ready || !dev->started) {
        return INMP441_ERR_STATE;
    }

    size_t done = 0U;
    while (done < max_samples) {
        size_t want = max_samples - done;
        if (want > INMP441_CHUNK_SAMPLES) {
            want = INMP441_CHUNK_SAMPLES;
        }

        size_t got_bytes = 0U;
        int ret = map_i2s(sx_i2s_read(&dev->i2s, dev->raw, want * SLOT_BYTES, &got_bytes, timeout_ms));

        size_t got = got_bytes / SLOT_BYTES;
        if (got > want) {
            got = want;
        }

        /* Keep the top 16 bits of the 24 bit sample. Shifting a negative value right keeps the sign (GCC). */
        for (size_t i = 0U; i < got; i++) {
            samples[done + i] = (int16_t)(dev->raw[i] >> 16);
        }
        done += got;
        *count = done;

        if (ret != INMP441_OK) {
            return ret;
        }
        if ((got_bytes % SLOT_BYTES) != 0U) {
            return INMP441_ERR_BUS;      /* a partial slot means the stream lost alignment */
        }
        if (got == 0U) {
            return INMP441_ERR_TIMEOUT;  /* no progress, do not spin */
        }
    }
    return INMP441_OK;
}