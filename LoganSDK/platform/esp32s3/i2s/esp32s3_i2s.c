#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driver/i2s_std.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "soc/soc_caps.h"

#include "sx_i2s.h"

typedef struct {
    bool used;
    bool started;
    i2s_chan_handle_t tx_channel;
    SemaphoreHandle_t lock;
    StaticSemaphore_t lock_storage;
} i2s_port_state_t;

static i2s_port_state_t s_ports[SOC_I2S_NUM];

static int map_error(esp_err_t error)
{
    switch (error) {
    case ESP_OK:
        return SX_I2S_OK;
    case ESP_ERR_INVALID_ARG:
        return SX_I2S_ERR_PARAM;
    case ESP_ERR_NO_MEM:
        return SX_I2S_ERR_NOMEM;
    case ESP_ERR_TIMEOUT:
        return SX_I2S_ERR_TIMEOUT;
    case ESP_ERR_INVALID_STATE:
        return SX_I2S_ERR_STATE;
    default:
        return SX_I2S_ERR_BUS;
    }
}

static TickType_t timeout_ticks(uint32_t timeout_ms)
{
    if (timeout_ms == UINT32_MAX) {
        return portMAX_DELAY;
    }
    return pdMS_TO_TICKS(timeout_ms);
}

static bool valid_config(const sx_i2s_config_t *config)
{
    if ((config == NULL) || (config->port < 0) ||
        (config->port >= (int)SOC_I2S_NUM) ||
        (config->bclk_pin < 0) || (config->ws_pin < 0) ||
        (config->dout_pin < 0) || (config->sample_rate_hz == 0U)) {
        return false;
    }

    if ((config->channels != SX_I2S_MONO) &&
        (config->channels != SX_I2S_STEREO)) {
        return false;
    }

    return (config->bits_per_sample == 8U) ||
           (config->bits_per_sample == 16U) ||
           (config->bits_per_sample == 24U) ||
           (config->bits_per_sample == 32U);
}

static i2s_port_state_t *get_port(const sx_i2s_config_t *config)
{
    if (!valid_config(config) || !s_ports[config->port].used) {
        return NULL;
    }
    return &s_ports[config->port];
}

int sx_i2s_init(const sx_i2s_config_t *config)
{
    if (!valid_config(config)) {
        return SX_I2S_ERR_PARAM;
    }

    i2s_port_state_t *port = &s_ports[config->port];
    if (port->used) {
        return SX_I2S_ERR_STATE;
    }

    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(config->port, I2S_ROLE_MASTER);
    i2s_chan_handle_t tx_channel = NULL;
    int ret = map_error(i2s_new_channel(&channel_config, &tx_channel, NULL));
    if (ret != SX_I2S_OK) {
        return ret;
    }

    i2s_std_config_t standard_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate_hz),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            config->bits_per_sample,
            config->channels == SX_I2S_MONO ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = config->bclk_pin,
            .ws = config->ws_pin,
            .dout = config->dout_pin,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = map_error(i2s_channel_init_std_mode(tx_channel, &standard_config));
    if (ret != SX_I2S_OK) {
        i2s_del_channel(tx_channel);
        return ret;
    }

    memset(port, 0, sizeof(*port));
    port->tx_channel = tx_channel;
    port->lock = xSemaphoreCreateMutexStatic(&port->lock_storage);
    port->used = true;
    return SX_I2S_OK;
}

int sx_i2s_deinit(const sx_i2s_config_t *config)
{
    i2s_port_state_t *port = get_port(config);
    if (port == NULL) {
        return SX_I2S_ERR_PARAM;
    }

    xSemaphoreTake(port->lock, portMAX_DELAY);
    if (port->started) {
        int ret = map_error(i2s_channel_disable(port->tx_channel));
        if (ret != SX_I2S_OK) {
            xSemaphoreGive(port->lock);
            return ret;
        }
        port->started = false;
    }

    int ret = map_error(i2s_del_channel(port->tx_channel));
    if (ret == SX_I2S_OK) {
        port->tx_channel = NULL;
        port->used = false;
    }
    xSemaphoreGive(port->lock);
    return ret;
}

int sx_i2s_start(const sx_i2s_config_t *config)
{
    i2s_port_state_t *port = get_port(config);
    if (port == NULL) {
        return SX_I2S_ERR_PARAM;
    }

    xSemaphoreTake(port->lock, portMAX_DELAY);
    if (port->started) {
        xSemaphoreGive(port->lock);
        return SX_I2S_ERR_STATE;
    }

    int ret = map_error(i2s_channel_enable(port->tx_channel));
    if (ret == SX_I2S_OK) {
        port->started = true;
    }
    xSemaphoreGive(port->lock);
    return ret;
}

int sx_i2s_stop(const sx_i2s_config_t *config)
{
    i2s_port_state_t *port = get_port(config);
    if (port == NULL) {
        return SX_I2S_ERR_PARAM;
    }

    xSemaphoreTake(port->lock, portMAX_DELAY);
    if (!port->started) {
        xSemaphoreGive(port->lock);
        return SX_I2S_ERR_STATE;
    }

    int ret = map_error(i2s_channel_disable(port->tx_channel));
    if (ret == SX_I2S_OK) {
        port->started = false;
    }
    xSemaphoreGive(port->lock);
    return ret;
}

int sx_i2s_write(const sx_i2s_config_t *config, const void *data, size_t size,
                 size_t *written, uint32_t timeout_ms)
{
    if ((data == NULL) || (size == 0U) || (written == NULL)) {
        return SX_I2S_ERR_PARAM;
    }

    i2s_port_state_t *port = get_port(config);
    if (port == NULL) {
        return SX_I2S_ERR_PARAM;
    }

    xSemaphoreTake(port->lock, portMAX_DELAY);
    if (!port->started) {
        xSemaphoreGive(port->lock);
        return SX_I2S_ERR_STATE;
    }

    esp_err_t error = i2s_channel_write(port->tx_channel, data, size, written,
                                        timeout_ticks(timeout_ms));
    xSemaphoreGive(port->lock);
    return map_error(error);
}