#ifndef SDCARD_H
#define SDCARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "sx_spi.h"

/*
 * microSD card in SPI mode, used as a block device (512 byte blocks).
 * Built only on the sx_spi contract. It does not know about any file system.
 * Supports SD v1 and SD v2 (SDSC, SDHC, SDXC) cards. MMC cards are rejected.
 *
 * Not thread safe. The caller must serialise all calls (and any other user of the same SPI host).
 */

/* Return codes: 0 on success, negative on failure. */
#define SDCARD_OK                0
#define SDCARD_ERR_PARAM        (-1)   /* NULL pointer, bad configuration, or block range outside the card */
#define SDCARD_ERR_STATE        (-2)   /* card not initialised, or already initialised */
#define SDCARD_ERR_BUS          (-3)   /* the SPI layer reported an error */
#define SDCARD_ERR_NO_CARD      (-4)   /* the card did not answer the reset command */
#define SDCARD_ERR_UNSUPPORTED  (-5)   /* not an SD card, wrong voltage range, or capacity too large */
#define SDCARD_ERR_TIMEOUT      (-6)   /* the card stayed busy or never sent its data */
#define SDCARD_ERR_IO           (-7)   /* the card rejected a command or a data block */

#define SDCARD_BLOCK_SIZE        512U

/* Clock used while the card is identified. The SD specification allows at most 400 kHz. */
#define SDCARD_INIT_CLOCK_HZ     400000U

typedef struct sdcard_config {
    /*
     * SPI setup of the card, provided by the board layer. Requirements:
     * cs must be set (software chip select), dummy must be 0xFF, mode must be SX_SPI_MODE_0,
     * lsb_first must be false, clock_hz is the data clock after identification (at most 25 MHz),
     * max_transfer_bytes must be 0 or at least 256.
     * It must stay valid and unchanged while the card is in use.
     */
    const sx_spi_config_t *spi;
    uint32_t               init_clock_hz;   /* identification clock, 0 = SDCARD_INIT_CLOCK_HZ, at most 400 kHz */
} sdcard_config_t;

typedef struct sdcard {
    const sdcard_config_t *config;
    sx_spi_config_t        init_spi_config;    /* copy of config->spi with the identification clock */
    sx_spi_t               spi;
    uint32_t               active_clock_hz;
    uint32_t               block_count;
    bool                   high_capacity;      /* true: block addressing (SDHC/SDXC) */
    bool                   ready;
} sdcard_t;

/*
 * Identifies the card at the slow clock, reads its size, then switches to the data clock.
 * The sdcard_t must be zero-initialised (for example a static variable) before the first call.
 * The call blocks for up to a few seconds when no card is present.
 * Returns SDCARD_ERR_NO_CARD when the card does not answer.
 */
int sdcard_init(sdcard_t *sd, const sdcard_config_t *config);

/* Releases the SPI device. Safe to call when the card is not initialised. */
int sdcard_deinit(sdcard_t *sd);

/*
 * Reads or writes count blocks starting at first_block. Blocking, one block per command.
 * count == 0 is a no-op. The block range must lie inside the card.
 * Buffers do not have to be DMA capable, the SPI driver copies them when needed, which is slower.
 */
int sdcard_read_blocks(sdcard_t *sd, uint32_t first_block, uint8_t *buf, uint32_t count);
int sdcard_write_blocks(sdcard_t *sd, uint32_t first_block, const uint8_t *buf, uint32_t count);

/* Number of 512 byte blocks, or 0 when the card is not initialised. */
uint32_t sdcard_block_count(const sdcard_t *sd);

#ifdef __cplusplus
}
#endif

#endif /* SDCARD_H */