#ifndef INMP441_H
#define INMP441_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "sx_i2s.h"

/*
 * INMP441 I2S MEMS microphone, receive only, mono. Built only on the sx_i2s contract.
 * The module owns one I2S port for the microphone: it creates a receive-only channel at 32 bit
 * slots (the microphone sends 24 bit data MSB first inside a 32 bit slot) and gives the caller
 * 16 bit signed PCM, which is the top 16 bits of the 24 bit sample.
 *
 * The microphone must send on the LEFT slot: tie its L/R pin to GND. With L/R tied high it sends
 * on the right slot and this module reads silence (all zero). The L/R pin must never float.
 *
 * Facts taken from the usual INMP441 datasheet, NOT checked on this board (see docs/HandOver.md):
 * supply 1.8 to 3.3 V (not 5 V), 24 bit data in a 32 bit slot, and a start-up time after the clock
 * starts during which samples are not valid. The first samples after inmp441_start() should be
 * discarded by the caller. The supported sample rate range is not checked here.
 *
 * Not thread safe. The caller must serialise all calls on one inmp441_t.
 */

/* Return codes: 0 on success, negative on failure. */
#define INMP441_OK              0
#define INMP441_ERR_PARAM      (-1)   /* NULL pointer or bad configuration */
#define INMP441_ERR_STATE      (-2)   /* not initialised, not started, or already in that state */
#define INMP441_ERR_BUS        (-3)   /* the I2S layer reported an error */
#define INMP441_ERR_TIMEOUT    (-4)   /* not enough samples arrived within the timeout */

/* Samples converted per I2S read call. Sets the size of the buffer inside inmp441_t. */
#define INMP441_CHUNK_SAMPLES   128U

typedef struct inmp441_config {
    int      port;             /* I2S port, provided by the board layer */
    int      bclk_pin;         /* GPIO wired to the SCK pin of the microphone */
    int      ws_pin;           /* GPIO wired to the WS pin of the microphone */
    int      din_pin;          /* GPIO wired to the SD pin (data out of the microphone) */
    uint32_t sample_rate_hz;   /* sample rate in Hz, for example 16000 */
} inmp441_config_t;

typedef struct inmp441 {
    sx_i2s_config_t i2s;                          /* private: built by inmp441_init */
    bool            ready;                        /* private */
    bool            started;                      /* private */
    int32_t         raw[INMP441_CHUNK_SAMPLES];   /* private: scratch buffer for 32 bit slots */
} inmp441_t;

/*
 * Creates the receive channel. The inmp441_t must be zero-initialised (for example a static
 * variable) before the first call. The channel does not run until inmp441_start().
 */
int inmp441_init(inmp441_t *dev, const inmp441_config_t *config);

/* Stops the clock if it runs and releases the I2S port. Can be repeated after a failure. */
int inmp441_deinit(inmp441_t *dev);

/* Starts the clock and the data flow. */
int inmp441_start(inmp441_t *dev);

/* Stops the clock and the data flow. */
int inmp441_stop(inmp441_t *dev);

/*
 * Reads up to max_samples 16 bit samples into samples. timeout_ms is the maximum block time of each
 * underlying read, in milliseconds. On success *count equals max_samples. On INMP441_ERR_TIMEOUT or
 * another error, *count is the number of valid samples already stored (possibly 0), so the caller can
 * still use them. Blocks until the samples arrive: 16000 samples take about one second at 16 kHz.
 */
int inmp441_read(inmp441_t *dev, int16_t *samples, size_t max_samples, size_t *count,
                 uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* INMP441_H */