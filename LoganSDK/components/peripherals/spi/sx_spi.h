#ifndef SX_SPI_H
#define SX_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "sx_gpio.h"

/* Return codes: 0 on success, negative on failure. */
#define SX_SPI_OK            0
#define SX_SPI_ERR_PARAM    (-1)
#define SX_SPI_ERR_BUS      (-2)
#define SX_SPI_ERR_TIMEOUT  (-3)
#define SX_SPI_ERR_NOMEM    (-4)

/*
 * Longest read-only transfer done in one piece (sx_spi_transfer with tx == NULL). The bytes clocked out
 * come from a buffer inside sx_spi_t. Longer read-only transfers are split into several pieces.
 */
#define SX_SPI_DUMMY_MAX    256U

typedef enum {
    SX_SPI_MODE_0 = 0,   /* CPOL 0, CPHA 0 */
    SX_SPI_MODE_1 = 1,   /* CPOL 0, CPHA 1 */
    SX_SPI_MODE_2 = 2,   /* CPOL 1, CPHA 0 */
    SX_SPI_MODE_3 = 3    /* CPOL 1, CPHA 1 */
} sx_spi_mode_t;

typedef struct sx_spi_config {
    int             host;                  /* SPI controller number of the chip, taken from the board layer */
    int             mosi_pin;
    int             miso_pin;
    int             sclk_pin;
    uint32_t        clock_hz;              /* wanted SCK frequency */
    sx_spi_mode_t   mode;
    bool            lsb_first;
    uint32_t        max_transfer_bytes;    /* largest single transfer, 0 = platform default */
    sx_gpio_pin_t  *cs;                    /* optional software chip select (output pin), NULL = none */
    bool            cs_active_high;
    uint8_t         dummy;                 /* byte clocked out on read-only transfers (a microSD card needs 0xFF) */
} sx_spi_config_t;

typedef struct sx_spi {
    const sx_spi_config_t *config;
    void                  *dev;            /* private to the platform, do not touch */
    bool                   ready;
    uint8_t                dummy_buf[SX_SPI_DUMMY_MAX];
} sx_spi_t;

/*
 * Brings up the bus of config->host (once, shared by all devices of that host) and adds one device.
 * The CS pin, if any, is put in its idle level before it becomes an output.
 * The config must stay valid and unchanged while spi is in use.
 * spi must be zero-initialised (for example a static variable) before the first call.
 * All devices of one host must use the same pins; the first init defines them.
 */
int sx_spi_init(sx_spi_t *spi, const sx_spi_config_t *config);

/* Removes the device and, when it was the last device of the host, frees the bus. */
int sx_spi_deinit(sx_spi_t *spi);

/*
 * Blocking full-duplex transfer. tx == NULL clocks out config->dummy, rx == NULL discards the answer.
 * len == 0 is a no-op. Does NOT touch CS. In-place transfer (tx == rx) is not supported.
 * len must not exceed max_transfer_bytes of the config (platform default when it is 0).
 * timeout_ms is not enforced on the ESP32-S3 platform, the call returns when the transfer is done.
 * Buffers do not have to be DMA capable, the driver copies them when needed, which is slower.
 */
int sx_spi_transfer(sx_spi_t *spi, const uint8_t *tx, uint8_t *rx, uint16_t len, uint32_t timeout_ms);
int sx_spi_write(sx_spi_t *spi, const uint8_t *tx, uint16_t len, uint32_t timeout_ms);
int sx_spi_read(sx_spi_t *spi, uint8_t *rx, uint16_t len, uint32_t timeout_ms);

/* Software CS. No-ops when config->cs is NULL. Honours cs_active_high. */
void sx_spi_select(sx_spi_t *spi);
void sx_spi_deselect(sx_spi_t *spi);

#ifdef __cplusplus
}
#endif

#endif /* SX_SPI_H */