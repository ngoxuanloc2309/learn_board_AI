#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "speaker.h"

/* One frame is a left and a right 16 bit sample. */
#define FRAME_BYTES         4U
#define SAMPLE_BITS         16U

static int map_i2s(int ret)
{
    switch (ret) {
    case SX_I2S_OK:             return SPEAKER_OK;
    case SX_I2S_ERR_PARAM:      return SPEAKER_ERR_PARAM;
    case SX_I2S_ERR_STATE:      return SPEAKER_ERR_STATE;
    case SX_I2S_ERR_TIMEOUT:    return SPEAKER_ERR_TIMEOUT;
    default:                    return SPEAKER_ERR_BUS;
    }
}

static bool valid_config(const speaker_config_t *config)
{
    return (config != NULL) && (config->port >= 0) && (config->bclk_pin >= 0) &&
           (config->ws_pin >= 0) && (config->dout_pin >= 0) && (config->sample_rate_hz != 0U);
}

int speaker_init(speaker_t *dev, const speaker_config_t *config)
{
    if ((dev == NULL) || !valid_config(config)) {
        return SPEAKER_ERR_PARAM;
    }
    if (dev->ready) {
        return SPEAKER_ERR_STATE;
    }

    sx_i2s_config_t i2s = {
        .port = config->port,
        .bclk_pin = config->bclk_pin,
        .ws_pin = config->ws_pin,
        .dout_pin = config->dout_pin,
        .din_pin = SX_I2S_PIN_UNUSED,
        .sample_rate_hz = config->sample_rate_hz,
        .bits_per_sample = (uint8_t)SAMPLE_BITS,
        .channels = SX_I2S_STEREO,
    };

    int ret = map_i2s(sx_i2s_init(&i2s));
    if (ret != SPEAKER_OK) {
        return ret;
    }

    dev->i2s = i2s;
    dev->volume = (uint8_t)SPEAKER_DEFAULT_VOLUME;
    dev->started = false;
    dev->ready = true;
    return SPEAKER_OK;
}

int speaker_deinit(speaker_t *dev)
{
    if (dev == NULL) {
        return SPEAKER_ERR_PARAM;
    }
    if (!dev->ready) {
        return SPEAKER_ERR_STATE;
    }

    /* sx_i2s_deinit stops a running port by itself. */
    int ret = map_i2s(sx_i2s_deinit(&dev->i2s));
    if (ret == SPEAKER_OK) {
        dev->started = false;
        dev->ready = false;
    }
    return ret;
}

int speaker_start(speaker_t *dev)
{
    if (dev == NULL) {
        return SPEAKER_ERR_PARAM;
    }
    if (!dev->ready || dev->started) {
        return SPEAKER_ERR_STATE;
    }

    int ret = map_i2s(sx_i2s_start(&dev->i2s));
    if (ret == SPEAKER_OK) {
        dev->started = true;
    }
    return ret;
}

int speaker_stop(speaker_t *dev)
{
    if (dev == NULL) {
        return SPEAKER_ERR_PARAM;
    }
    if (!dev->ready || !dev->started) {
        return SPEAKER_ERR_STATE;
    }

    int ret = map_i2s(sx_i2s_stop(&dev->i2s));
    if (ret == SPEAKER_OK) {
        dev->started = false;
    }
    return ret;
}

int speaker_set_volume(speaker_t *dev, uint8_t percent)
{
    if ((dev == NULL) || (percent > SPEAKER_VOLUME_MAX)) {
        return SPEAKER_ERR_PARAM;
    }
    if (!dev->ready) {
        return SPEAKER_ERR_STATE;
    }

    dev->volume = percent;
    return SPEAKER_OK;
}

int speaker_write(speaker_t *dev, const int16_t *samples, size_t count, size_t *written,
                  uint32_t timeout_ms)
{
    if ((dev == NULL) || (samples == NULL) || (count == 0U) || (written == NULL)) {
        return SPEAKER_ERR_PARAM;
    }
    *written = 0U;
    if (!dev->ready || !dev->started) {
        return SPEAKER_ERR_STATE;
    }

    size_t done = 0U;
    while (done < count) {
        size_t want = count - done;
        if (want > SPEAKER_CHUNK_SAMPLES) {
            want = SPEAKER_CHUNK_SAMPLES;
        }

        /* Read the volume once per chunk. It is at most 100, so the result always fits 16 bits. */
        int32_t gain = (int32_t)dev->volume;
        for (size_t i = 0U; i < want; i++) {
            int16_t value = (int16_t)(((int32_t)samples[done + i] * gain) / (int32_t)SPEAKER_VOLUME_MAX);
            dev->frames[2U * i] = value;
            dev->frames[(2U * i) + 1U] = value;
        }

        size_t sent_bytes = 0U;
        int ret = map_i2s(sx_i2s_write(&dev->i2s, dev->frames, want * FRAME_BYTES, &sent_bytes, timeout_ms));

        size_t got = sent_bytes / FRAME_BYTES;
        if (got > want) {
            got = want;
        }
        done += got;
        *written = done;

        if (ret != SPEAKER_OK) {
            return ret;
        }
        if ((sent_bytes % FRAME_BYTES) != 0U) {
            return SPEAKER_ERR_BUS;      /* a partial frame means left and right may be swapped */
        }
        if (got < want) {
            return SPEAKER_ERR_TIMEOUT;  /* the driver accepted less without an error, do not spin */
        }
    }
    return SPEAKER_OK;
}