#ifndef SX_I2S_H
#define SX_I2S_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define SX_I2S_OK           0
#define SX_I2S_ERR_PARAM   (-1)
#define SX_I2S_ERR_STATE   (-2)
#define SX_I2S_ERR_BUS     (-3)
#define SX_I2S_ERR_TIMEOUT (-4)
#define SX_I2S_ERR_NOMEM  (-5)

typedef enum {
    SX_I2S_MONO = 1,
    SX_I2S_STEREO = 2
} sx_i2s_channels_t;

typedef struct {
    int port;
    int bclk_pin;
    int ws_pin;
    int dout_pin;
    uint32_t sample_rate_hz;
    uint8_t bits_per_sample;
    sx_i2s_channels_t channels;
} sx_i2s_config_t;

int sx_i2s_init(const sx_i2s_config_t *config);
int sx_i2s_deinit(const sx_i2s_config_t *config);
int sx_i2s_start(const sx_i2s_config_t *config);
int sx_i2s_stop(const sx_i2s_config_t *config);
int sx_i2s_write(const sx_i2s_config_t *config, const void *data, size_t size,
                 size_t *written, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif