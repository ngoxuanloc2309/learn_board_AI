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

/*
 * One state per I2S port. The send and receive directions have their own lock
 * so a read and a write do not block each other. Calls that change the port
 * (init, deinit, start, stop) take both locks, always tx first, then rx.
 * The locks are created once at the first init of the port and never destroyed.
 */
typedef struct {
    bool locks_ready;
    bool used;
    bool started;
    i2s_chan_handle_t tx_channel;
    i2s_chan_handle_t rx_channel;
    SemaphoreHandle_t tx_lock;
    SemaphoreHandle_t rx_lock;
    StaticSemaphore_t tx_lock_storage;
    StaticSemaphore_t rx_lock_storage;
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

/* A pin is either unused (-1) or a GPIO number (0 or more). */
static bool pin_ok(int pin)
{
    return pin >= SX_I2S_PIN_UNUSED;
}

static bool valid_config(const sx_i2s_config_t *config)
{
    if ((config == NULL) || (config->port < 0) ||
        (config->port >= (int)SOC_I2S_NUM) ||
        (config->bclk_pin < 0) || (config->ws_pin < 0) ||
        !pin_ok(config->dout_pin) || !pin_ok(config->din_pin) ||
        ((config->dout_pin < 0) && (config->din_pin < 0)) ||
        (config->sample_rate_hz == 0U)) {
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

static void lock_all(i2s_port_state_t *port)
{
    xSemaphoreTake(port->tx_lock, portMAX_DELAY);
    xSemaphoreTake(port->rx_lock, portMAX_DELAY);
}

static void unlock_all(i2s_port_state_t *port)
{
    xSemaphoreGive(port->rx_lock);
    xSemaphoreGive(port->tx_lock);
}

/*
 * Returns the port state for a valid config whose locks exist, else NULL.
 * The used flag is checked later, under the lock.
 */
static i2s_port_state_t *get_port(const sx_i2s_config_t *config)
{
    if (!valid_config(config) || !s_ports[config->port].locks_ready) {
        return NULL;
    }
    return &s_ports[config->port];
}

/* A channel that is already disabled counts as stopped, so a stop can be repeated. */
static int disable_channel(i2s_chan_handle_t channel)
{
    esp_err_t error = i2s_channel_disable(channel);
    return (error == ESP_ERR_INVALID_STATE) ? SX_I2S_OK : map_error(error);
}

/* Disable both directions. Both are tried, the first error is returned. */
static int disable_all(i2s_port_state_t *port)
{
    int ret = SX_I2S_OK;
    if (port->rx_channel != NULL) {
        ret = disable_channel(port->rx_channel);
    }
    if (port->tx_channel != NULL) {
        int tx_ret = disable_channel(port->tx_channel);
        if (ret == SX_I2S_OK) {
            ret = tx_ret;
        }
    }
    return ret;
}

static void delete_channels(i2s_chan_handle_t tx_channel, i2s_chan_handle_t rx_channel)
{
    if (tx_channel != NULL) {
        i2s_del_channel(tx_channel);
    }
    if (rx_channel != NULL) {
        i2s_del_channel(rx_channel);
    }
}

int sx_i2s_init(const sx_i2s_config_t *config)
{
    if (!valid_config(config)) {
        return SX_I2S_ERR_PARAM;
    }

    i2s_port_state_t *port = &s_ports[config->port];
    if (!port->locks_ready) {
        port->tx_lock = xSemaphoreCreateMutexStatic(&port->tx_lock_storage);
        port->rx_lock = xSemaphoreCreateMutexStatic(&port->rx_lock_storage);
        port->locks_ready = true;
    }

    lock_all(port);
    if (port->used) {
        unlock_all(port);
        return SX_I2S_ERR_STATE;
    }

    bool want_tx = config->dout_pin >= 0;
    bool want_rx = config->din_pin >= 0;
    i2s_chan_handle_t tx_channel = NULL;
    i2s_chan_handle_t rx_channel = NULL;
    i2s_chan_config_t channel_config =
        I2S_CHANNEL_DEFAULT_CONFIG(config->port, I2S_ROLE_MASTER);
    /* Send silence when no data is queued. Without this the driver replays the last buffers. */
    channel_config.auto_clear_after_cb = true;

    int ret = map_error(i2s_new_channel(&channel_config,
                                        want_tx ? &tx_channel : NULL,
                                        want_rx ? &rx_channel : NULL));
    if (ret != SX_I2S_OK) {
        unlock_all(port);
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
            .dout = want_tx ? config->dout_pin : I2S_GPIO_UNUSED,
            .din = want_rx ? config->din_pin : I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    /*
     * On ESP32-S3 the default slot macro selects both slots even in mono mode, but the driver
     * (v5.4.3, hal/esp32s3/i2s_ll.h) says that in mono mode only one slot should be enabled.
     * Use the left slot: the INMP441 sends its data there when the L/R pin is tied to GND.
     */
    if (config->channels == SX_I2S_MONO) {
        standard_config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    }

    if (want_tx) {
        ret = map_error(i2s_channel_init_std_mode(tx_channel, &standard_config));
    }
    if ((ret == SX_I2S_OK) && want_rx) {
        ret = map_error(i2s_channel_init_std_mode(rx_channel, &standard_config));
    }
    if (ret != SX_I2S_OK) {
        delete_channels(tx_channel, rx_channel);
        unlock_all(port);
        return ret;
    }

    port->tx_channel = tx_channel;
    port->rx_channel = rx_channel;
    port->started = false;
    port->used = true;
    unlock_all(port);
    return SX_I2S_OK;
}

int sx_i2s_deinit(const sx_i2s_config_t *config)
{
    i2s_port_state_t *port = get_port(config);
    if (port == NULL) {
        return SX_I2S_ERR_PARAM;
    }

    lock_all(port);
    if (!port->used) {
        unlock_all(port);
        return SX_I2S_ERR_PARAM;
    }

    int ret = SX_I2S_OK;
    if (port->started) {
        ret = disable_all(port);
        if (ret != SX_I2S_OK) {
            unlock_all(port);
            return ret;
        }
        port->started = false;
    }

    /* Each handle is cleared once deleted, so a failed deinit can be repeated. */
    if (port->tx_channel != NULL) {
        int del_ret = map_error(i2s_del_channel(port->tx_channel));
        if (del_ret == SX_I2S_OK) {
            port->tx_channel = NULL;
        } else {
            ret = del_ret;
        }
    }
    if (port->rx_channel != NULL) {
        int del_ret = map_error(i2s_del_channel(port->rx_channel));
        if (del_ret == SX_I2S_OK) {
            port->rx_channel = NULL;
        } else if (ret == SX_I2S_OK) {
            ret = del_ret;
        }
    }
    if ((port->tx_channel == NULL) && (port->rx_channel == NULL)) {
        port->used = false;
    }
    unlock_all(port);
    return ret;
}

int sx_i2s_start(const sx_i2s_config_t *config)
{
    i2s_port_state_t *port = get_port(config);
    if (port == NULL) {
        return SX_I2S_ERR_PARAM;
    }

    lock_all(port);
    if (!port->used) {
        unlock_all(port);
        return SX_I2S_ERR_PARAM;
    }
    if (port->started) {
        unlock_all(port);
        return SX_I2S_ERR_STATE;
    }

    int ret = SX_I2S_OK;
    if (port->tx_channel != NULL) {
        ret = map_error(i2s_channel_enable(port->tx_channel));
    }
    if ((ret == SX_I2S_OK) && (port->rx_channel != NULL)) {
        ret = map_error(i2s_channel_enable(port->rx_channel));
        if ((ret != SX_I2S_OK) && (port->tx_channel != NULL)) {
            /* Undo the send direction so the port is not left half started. */
            i2s_channel_disable(port->tx_channel);
        }
    }
    if (ret == SX_I2S_OK) {
        port->started = true;
    }
    unlock_all(port);
    return ret;
}

int sx_i2s_stop(const sx_i2s_config_t *config)
{
    i2s_port_state_t *port = get_port(config);
    if (port == NULL) {
        return SX_I2S_ERR_PARAM;
    }

    lock_all(port);
    if (!port->used) {
        unlock_all(port);
        return SX_I2S_ERR_PARAM;
    }
    if (!port->started) {
        unlock_all(port);
        return SX_I2S_ERR_STATE;
    }

    int ret = disable_all(port);
    if (ret == SX_I2S_OK) {
        port->started = false;
    }
    unlock_all(port);
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

    xSemaphoreTake(port->tx_lock, portMAX_DELAY);
    int ret;
    if (!port->used || (port->tx_channel == NULL)) {
        ret = SX_I2S_ERR_PARAM;
    } else if (!port->started) {
        ret = SX_I2S_ERR_STATE;
    } else {
        /* The ESP-IDF driver takes milliseconds and converts to ticks itself. */
        ret = map_error(i2s_channel_write(port->tx_channel, data, size, written,
                                          timeout_ms));
    }
    xSemaphoreGive(port->tx_lock);
    return ret;
}

int sx_i2s_read(const sx_i2s_config_t *config, void *data, size_t size,
                size_t *read, uint32_t timeout_ms)
{
    if ((data == NULL) || (size == 0U) || (read == NULL)) {
        return SX_I2S_ERR_PARAM;
    }

    i2s_port_state_t *port = get_port(config);
    if (port == NULL) {
        return SX_I2S_ERR_PARAM;
    }

    xSemaphoreTake(port->rx_lock, portMAX_DELAY);
    int ret;
    if (!port->used || (port->rx_channel == NULL)) {
        ret = SX_I2S_ERR_PARAM;
    } else if (!port->started) {
        ret = SX_I2S_ERR_STATE;
    } else {
        ret = map_error(i2s_channel_read(port->rx_channel, data, size, read,
                                         timeout_ms));
    }
    xSemaphoreGive(port->rx_lock);
    return ret;
}