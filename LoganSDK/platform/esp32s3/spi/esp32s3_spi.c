#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sx_spi.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#define SPI_DEFAULT_MAX_TRANSFER   4092U   /* driver default when DMA is enabled */
#define SPI_MODE_MAX               3

/* Bus state shared by all devices of one host. */
typedef struct {
    uint8_t users;
    int     mosi_pin;
    int     miso_pin;
    int     sclk_pin;
} spi_bus_state_t;

static spi_bus_state_t s_bus[SPI_HOST_MAX];

static int map_err(esp_err_t err)
{
    switch (err) {
    case ESP_OK:              return SX_SPI_OK;
    case ESP_ERR_INVALID_ARG: return SX_SPI_ERR_PARAM;
    case ESP_ERR_TIMEOUT:     return SX_SPI_ERR_TIMEOUT;
    case ESP_ERR_NO_MEM:      return SX_SPI_ERR_NOMEM;
    default:                  return SX_SPI_ERR_BUS;
    }
}

static bool host_is_valid(int host)
{
    return (host >= (int)SPI2_HOST) && (host < (int)SPI_HOST_MAX);
}

static uint32_t max_transfer(const sx_spi_config_t *config)
{
    return (config->max_transfer_bytes != 0U) ? config->max_transfer_bytes : SPI_DEFAULT_MAX_TRANSFER;
}

static int transmit(sx_spi_t *spi, const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_transaction_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.length    = len * 8U;
    trans.tx_buffer = tx;
    trans.rx_buffer = rx;
    return map_err(spi_device_transmit((spi_device_handle_t)spi->dev, &trans));
}

int sx_spi_init(sx_spi_t *spi, const sx_spi_config_t *config)
{
    if ((spi == NULL) || (config == NULL) || !host_is_valid(config->host) ||
        (config->sclk_pin < 0) || (config->clock_hz == 0U) ||
        ((int)config->mode > SPI_MODE_MAX) || ((int)config->mode < 0)) {
        return SX_SPI_ERR_PARAM;
    }
    if (spi->ready) {
        return SX_SPI_ERR_BUS;
    }
    if ((config->cs != NULL) && (config->cs->mode != SX_GPIO_MODE_OUTPUT_PP)) {
        return SX_SPI_ERR_PARAM;
    }

    spi_bus_state_t *bus = &s_bus[config->host];
    bool bus_created = false;

    if (bus->users == 0U) {
        spi_bus_config_t bus_cfg = {
            .mosi_io_num     = config->mosi_pin,
            .miso_io_num     = config->miso_pin,
            .sclk_io_num     = config->sclk_pin,
            .quadwp_io_num   = -1,
            .quadhd_io_num   = -1,
            .max_transfer_sz = (int)config->max_transfer_bytes,
        };
        int ret = map_err(spi_bus_initialize((spi_host_device_t)config->host, &bus_cfg, SPI_DMA_CH_AUTO));
        if (ret != SX_SPI_OK) {
            return ret;
        }
        bus->mosi_pin = config->mosi_pin;
        bus->miso_pin = config->miso_pin;
        bus->sclk_pin = config->sclk_pin;
        bus_created = true;
    } else if ((bus->mosi_pin != config->mosi_pin) || (bus->miso_pin != config->miso_pin) ||
               (bus->sclk_pin != config->sclk_pin)) {
        return SX_SPI_ERR_PARAM;
    }

    /* Put CS in its idle level before it becomes an output. */
    if (config->cs != NULL) {
        sx_gpio_value_t idle = config->cs_active_high ? SX_GPIO_LOW : SX_GPIO_HIGH;
        if (sx_gpio_init(config->cs, idle) != SX_GPIO_OK) {
            if (bus_created) {
                spi_bus_free((spi_host_device_t)config->host);
            }
            return SX_SPI_ERR_BUS;
        }
    }

    spi_device_interface_config_t dev_cfg = {
        .mode           = (uint8_t)config->mode,
        .clock_speed_hz = (int)config->clock_hz,
        .spics_io_num   = -1,                       /* chip select is handled in software */
        .queue_size     = 1,
        .flags          = config->lsb_first ? SPI_DEVICE_BIT_LSBFIRST : 0U,
    };
    spi_device_handle_t handle = NULL;
    int ret = map_err(spi_bus_add_device((spi_host_device_t)config->host, &dev_cfg, &handle));
    if (ret != SX_SPI_OK) {
        if (bus_created) {
            spi_bus_free((spi_host_device_t)config->host);
        }
        return ret;
    }

    memset(spi->dummy_buf, config->dummy, sizeof(spi->dummy_buf));
    spi->config = config;
    spi->dev    = handle;
    spi->ready  = true;
    bus->users++;
    return SX_SPI_OK;
}

int sx_spi_deinit(sx_spi_t *spi)
{
    if (spi == NULL) {
        return SX_SPI_ERR_PARAM;
    }
    if (!spi->ready) {
        return SX_SPI_OK;
    }

    int ret = map_err(spi_bus_remove_device((spi_device_handle_t)spi->dev));
    if (ret != SX_SPI_OK) {
        return ret;
    }

    spi_bus_state_t *bus = &s_bus[spi->config->host];
    if (bus->users > 0U) {
        bus->users--;
    }
    if (bus->users == 0U) {
        ret = map_err(spi_bus_free((spi_host_device_t)spi->config->host));
    }

    spi->dev   = NULL;
    spi->ready = false;
    return ret;
}

int sx_spi_transfer(sx_spi_t *spi, const uint8_t *tx, uint8_t *rx, uint16_t len, uint32_t timeout_ms)
{
    (void)timeout_ms;   /* not enforced on this platform, see sx_spi.h */

    if ((spi == NULL) || !spi->ready) {
        return SX_SPI_ERR_PARAM;
    }
    if (len == 0U) {
        return SX_SPI_OK;
    }
    if ((tx != NULL) && (tx == rx)) {
        return SX_SPI_ERR_PARAM;
    }
    if ((uint32_t)len > max_transfer(spi->config)) {
        return SX_SPI_ERR_PARAM;
    }

    if (tx != NULL) {
        return transmit(spi, tx, rx, len);
    }

    /* Read-only: clock out the dummy byte, in pieces no longer than the dummy buffer. */
    uint16_t done = 0;
    while (done < len) {
        uint16_t chunk = (uint16_t)(len - done);
        if (chunk > SX_SPI_DUMMY_MAX) {
            chunk = SX_SPI_DUMMY_MAX;
        }
        int ret = transmit(spi, spi->dummy_buf, (rx != NULL) ? (rx + done) : NULL, chunk);
        if (ret != SX_SPI_OK) {
            return ret;
        }
        done = (uint16_t)(done + chunk);
    }
    return SX_SPI_OK;
}

int sx_spi_write(sx_spi_t *spi, const uint8_t *tx, uint16_t len, uint32_t timeout_ms)
{
    if ((tx == NULL) && (len != 0U)) {
        return SX_SPI_ERR_PARAM;
    }
    return sx_spi_transfer(spi, tx, NULL, len, timeout_ms);
}

int sx_spi_read(sx_spi_t *spi, uint8_t *rx, uint16_t len, uint32_t timeout_ms)
{
    if ((rx == NULL) && (len != 0U)) {
        return SX_SPI_ERR_PARAM;
    }
    return sx_spi_transfer(spi, NULL, rx, len, timeout_ms);
}

void sx_spi_select(sx_spi_t *spi)
{
    if ((spi == NULL) || !spi->ready || (spi->config->cs == NULL)) {
        return;
    }
    sx_gpio_write(spi->config->cs, spi->config->cs_active_high ? SX_GPIO_HIGH : SX_GPIO_LOW);
}

void sx_spi_deselect(sx_spi_t *spi)
{
    if ((spi == NULL) || !spi->ready || (spi->config->cs == NULL)) {
        return;
    }
    sx_gpio_write(spi->config->cs, spi->config->cs_active_high ? SX_GPIO_LOW : SX_GPIO_HIGH);
}