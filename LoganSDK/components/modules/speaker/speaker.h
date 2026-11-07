#ifndef SPEAKER_H
#define SPEAKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sx_i2s.h"

/*
 * Speaker output through an I2S amplifier, send only. Built only on the sx_i2s contract.
 * The module owns one I2S port for the speaker: it creates a send-only channel with 16 bit
 * stereo frames and gives the caller a simple mono PCM interface (16 bit signed samples).
 *
 * Every mono sample is sent on both the left and the right slot. The channel select of the
 * amplifier on this board is NOT verified, so sending on both slots works whichever slot the
 * amplifier listens to. The cost is twice the I2S data rate, which is small at speech rates.
 *
 * Volume is a linear software gain from 0 to 100 percent applied to every sample. 100 percent
 * leaves the samples unchanged. It is not a loudness curve, so the steps are not even to the ear.
 *
 * When no data is queued while the channel runs, the sx_i2s layer outputs silence (see the
 * sx_i2s contract). A gap in the stream is therefore silent, not a repeated sound.
 * speaker_stop() cuts the audio that is still waiting in the hardware buffers (some tens of
 * milliseconds), so write a short stretch of zero samples before it when the end must be heard.
 *
 * The amplifier is NOT verified on this board (see docs/HandOver.md). Its enable pin, gain and
 * shutdown behavior are not handled here.
 *
 * Not thread safe. The caller must serialise all calls on one speaker_t, with one exception:
 * speaker_set_volume() may be called from another task while speaker_write() runs.
 */

/* Return codes: 0 on success, negative on failure. */
#define SPEAKER_OK              0
#define SPEAKER_ERR_PARAM      (-1)   /* NULL pointer or bad configuration */
#define SPEAKER_ERR_STATE      (-2)   /* not initialised, not started, or already in that state */
#define SPEAKER_ERR_BUS        (-3)   /* the I2S layer reported an error, or the stream lost alignment */
#define SPEAKER_ERR_TIMEOUT    (-4)   /* not all samples were accepted within the timeout */

/* Volume range and the value set by speaker_init(). The default is a design choice, not measured. */
#define SPEAKER_VOLUME_MAX      100U
#ifndef SPEAKER_DEFAULT_VOLUME
#define SPEAKER_DEFAULT_VOLUME  50U
#endif

/* Mono samples converted per I2S write call. Sets the size of the buffer inside speaker_t. */
#define SPEAKER_CHUNK_SAMPLES   128U

typedef struct speaker_config {
    int      port;             /* I2S port, provided by the board layer */
    int      bclk_pin;         /* GPIO wired to the BCLK pin of the amplifier */
    int      ws_pin;           /* GPIO wired to the LRCK (WS) pin of the amplifier */
    int      dout_pin;         /* GPIO wired to the data in pin of the amplifier */
    uint32_t sample_rate_hz;   /* sample rate in Hz, for example 16000 */
} speaker_config_t;

typedef struct speaker {
    sx_i2s_config_t  i2s;                                /* private: built by speaker_init */
    bool             ready;                              /* private */
    bool             started;                            /* private */
    volatile uint8_t volume;                             /* private: 0 to SPEAKER_VOLUME_MAX */
    int16_t          frames[SPEAKER_CHUNK_SAMPLES * 2U]; /* private: scratch buffer, left then right */
} speaker_t;

/*
 * Creates the send channel. The speaker_t must be zero-initialised (for example a static
 * variable) before the first call. The channel does not run until speaker_start().
 * The volume is set to SPEAKER_DEFAULT_VOLUME.
 */
int speaker_init(speaker_t *dev, const speaker_config_t *config);

/* Stops the clock if it runs and releases the I2S port. Can be repeated after a failure. */
int speaker_deinit(speaker_t *dev);

/* Starts the clock and the data flow. */
int speaker_start(speaker_t *dev);

/* Stops the clock and the data flow. Audio still waiting in the hardware buffers is lost. */
int speaker_stop(speaker_t *dev);

/*
 * Sets the volume, 0 to SPEAKER_VOLUME_MAX percent. Takes effect from the next chunk of
 * SPEAKER_CHUNK_SAMPLES samples. Allowed after speaker_init, started or not.
 */
int speaker_set_volume(speaker_t *dev, uint8_t percent);

/*
 * Plays count mono 16 bit samples. timeout_ms is the maximum block time of each underlying write,
 * in milliseconds. The call returns when all samples are queued, which is a little before they are
 * heard: the hardware buffers hold some tens of milliseconds of audio.
 * On success *written equals count. On SPEAKER_ERR_TIMEOUT or another error, *written is the number
 * of samples already queued (possibly 0). The samples after that point were not queued.
 * SPEAKER_ERR_BUS after a partial frame means left and right may now be swapped: call speaker_stop()
 * and speaker_start() again before the next write.
 */
int speaker_write(speaker_t *dev, const int16_t *samples, size_t count, size_t *written,
                  uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SPEAKER_H */