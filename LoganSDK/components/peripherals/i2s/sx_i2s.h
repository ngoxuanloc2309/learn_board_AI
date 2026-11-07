#ifndef SX_I2S_H
#define SX_I2S_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define SX_I2S_OK           0
#define SX_I2S_ERR_PARAM    (-1)
#define SX_I2S_ERR_STATE    (-2)
#define SX_I2S_ERR_BUS      (-3)
#define SX_I2S_ERR_TIMEOUT  (-4)
#define SX_I2S_ERR_NOMEM    (-5)

/* Value for dout_pin or din_pin when that direction is not used. */
#define SX_I2S_PIN_UNUSED   (-1)

typedef enum {
    SX_I2S_MONO = 1,
    SX_I2S_STEREO = 2
} sx_i2s_channels_t;

/*
 * Master mode, Philips format, one config per I2S port.
 * dout_pin >= 0 creates a send direction (speaker).
 * din_pin >= 0 creates a receive direction (microphone).
 * Set the unused one to SX_I2S_PIN_UNUSED. At least one must be used.
 * Both used on one port means full duplex: they share BCLK, WS, rate and format.
 * The same config must be passed to every call for that port.
 * init and deinit of one port must not run at the same time as other calls on it.
 */
typedef struct {
    int port;
    int bclk_pin;
    int ws_pin;
    int dout_pin;
    int din_pin;
    uint32_t sample_rate_hz;
    uint8_t bits_per_sample;
    sx_i2s_channels_t channels;
} sx_i2s_config_t;

int sx_i2s_init(const sx_i2s_config_t *config);
int sx_i2s_deinit(const sx_i2s_config_t *config);
int sx_i2s_start(const sx_i2s_config_t *config);
int sx_i2s_stop(const sx_i2s_config_t *config);

/*
 * Send data. timeout_ms is the maximum block time in milliseconds.
 * UINT32_MAX waits practically forever.
 * Returns SX_I2S_ERR_PARAM when the port has no send direction.
 */
int sx_i2s_write(const sx_i2s_config_t *config, const void *data, size_t size,
                 size_t *written, uint32_t timeout_ms);

/*
 * Receive data. Same timeout rules as sx_i2s_write.
 * Returns SX_I2S_ERR_PARAM when the port has no receive direction.
 * A read and a write on a full duplex port do not block each other.
 */
int sx_i2s_read(const sx_i2s_config_t *config, void *data, size_t size,
                size_t *read, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif