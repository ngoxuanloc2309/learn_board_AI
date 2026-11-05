#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sdcard.h"

/* SD commands in SPI mode. */
#define CMD0     0U    /* GO_IDLE_STATE */
#define CMD8     8U    /* SEND_IF_COND */
#define CMD9     9U    /* SEND_CSD */
#define CMD16    16U   /* SET_BLOCKLEN */
#define CMD17    17U   /* READ_SINGLE_BLOCK */
#define CMD24    24U   /* WRITE_BLOCK */
#define CMD55    55U   /* APP_CMD, prefix of an application command */
#define CMD58    58U   /* READ_OCR */
#define ACMD41   41U   /* SD_SEND_OP_COND */

/* CRC byte of a command. Only CMD0 and CMD8 are checked by the card in SPI mode. */
#define CRC_CMD0     0x95U
#define CRC_CMD8     0x87U
#define CRC_DEFAULT  0x01U

#define R1_READY         0x00U
#define R1_IDLE          0x01U
#define R1_ILLEGAL_CMD   0x04U

#define TOKEN_START_BLOCK    0xFEU
#define DATA_RESP_MASK       0x1FU
#define DATA_RESP_ACCEPTED   0x05U

#define ARG_HCS              0x40000000U   /* host supports high capacity cards */
#define ARG_CMD8_PATTERN     0x000001AAU   /* 2.7 to 3.6 V, check pattern 0xAA */
#define OCR_CCS_BYTE0_MASK   0x40U         /* card capacity status, in the first OCR byte */

#define SPI_TIMEOUT_MS       100U          /* passed to sx_spi, not enforced on every platform */
#define CHUNK_BYTES          256U          /* largest piece moved in one sx_spi call */
#define BUSY_CHUNK_BYTES     16U           /* bytes read per busy poll */
#define R1_POLLS             10U           /* the card answers within 8 bytes */
#define CMD0_TRIES           10U
#define ACMD41_TRIES         2000U
#define POWER_UP_CLOCK_BYTES 10U           /* 80 clocks with the card deselected */

#define READY_WAIT_MS        500U
#define TOKEN_WAIT_MS        100U
#define POLLS_MIN            64U
#define POLLS_MAX            20000U

#define CSD_BYTES            16U
#define MAX_DATA_CLOCK_HZ    25000000U
#define MAX_INIT_CLOCK_HZ    400000U

static int map_spi(int ret)
{
    return (ret == SX_SPI_OK) ? SDCARD_OK : SDCARD_ERR_BUS;
}

static int spi_write(sdcard_t *sd, const uint8_t *data, uint16_t len)
{
    return map_spi(sx_spi_write(&sd->spi, data, len, SPI_TIMEOUT_MS));
}

static int spi_read(sdcard_t *sd, uint8_t *data, uint16_t len)
{
    return map_spi(sx_spi_read(&sd->spi, data, len, SPI_TIMEOUT_MS));
}

/*
 * Timeouts are counted in polls, not in milliseconds, because the module has no time source.
 * The count covers about ms milliseconds of bus time at the active clock, limited to a sane range,
 * so on a fast clock the real time is dominated by the cost of each SPI call.
 */
static uint32_t poll_limit(uint32_t clock_hz, uint32_t ms, uint32_t bytes_per_poll)
{
    uint64_t bytes = ((uint64_t)(clock_hz / 8U) * ms) / 1000U;
    uint64_t polls = bytes / bytes_per_poll;

    if (polls < POLLS_MIN) {
        polls = POLLS_MIN;
    }
    if (polls > POLLS_MAX) {
        polls = POLLS_MAX;
    }
    return (uint32_t)polls;
}

/* Waits until the card stops holding MISO low (busy). */
static int wait_ready(sdcard_t *sd, uint32_t ms)
{
    uint8_t buf[BUSY_CHUNK_BYTES];
    uint32_t polls = poll_limit(sd->active_clock_hz, ms, BUSY_CHUNK_BYTES);

    while (polls-- > 0U) {
        int ret = spi_read(sd, buf, BUSY_CHUNK_BYTES);
        if (ret != SDCARD_OK) {
            return ret;
        }
        if (buf[BUSY_CHUNK_BYTES - 1U] == 0xFFU) {
            return SDCARD_OK;
        }
    }
    return SDCARD_ERR_TIMEOUT;
}

/* Deselects the card and clocks one byte so the card releases MISO. */
static void release(sdcard_t *sd)
{
    const uint8_t idle = 0xFFU;

    sx_spi_deselect(&sd->spi);
    (void)sx_spi_write(&sd->spi, &idle, 1U, SPI_TIMEOUT_MS);
}

/* Sends one command and reads the R1 answer. The card must already be selected. */
static int command(sdcard_t *sd, uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t *r1)
{
    uint8_t frame[6];
    uint8_t answer = 0xFFU;
    int ret = wait_ready(sd, READY_WAIT_MS);

    if (ret != SDCARD_OK) {
        return ret;
    }

    frame[0] = (uint8_t)(0x40U | cmd);
    frame[1] = (uint8_t)(arg >> 24);
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >> 8);
    frame[4] = (uint8_t)arg;
    frame[5] = crc;
    ret = spi_write(sd, frame, (uint16_t)sizeof(frame));
    if (ret != SDCARD_OK) {
        return ret;
    }

    for (uint32_t i = 0U; i < R1_POLLS; i++) {
        ret = spi_read(sd, &answer, 1U);
        if (ret != SDCARD_OK) {
            return ret;
        }
        if ((answer & 0x80U) == 0U) {
            *r1 = answer;
            return SDCARD_OK;
        }
    }
    return SDCARD_ERR_TIMEOUT;
}

/* Sends CMD55 followed by an application command. A bad CMD55 answer is returned in r1. */
static int app_command(sdcard_t *sd, uint8_t cmd, uint32_t arg, uint8_t *r1)
{
    int ret = command(sd, CMD55, 0U, CRC_DEFAULT, r1);

    if ((ret != SDCARD_OK) || (*r1 > R1_IDLE)) {
        return ret;
    }
    return command(sd, cmd, arg, CRC_DEFAULT, r1);
}

/* Reads a data block of len bytes: waits for the start token, then data, then discards the CRC. */
static int read_data(sdcard_t *sd, uint8_t *buf, uint16_t len)
{
    uint8_t token = 0xFFU;
    uint8_t crc[2];
    uint32_t polls = poll_limit(sd->active_clock_hz, TOKEN_WAIT_MS, 1U);
    uint16_t done = 0U;
    int ret;

    while (polls-- > 0U) {
        ret = spi_read(sd, &token, 1U);
        if (ret != SDCARD_OK) {
            return ret;
        }
        if (token != 0xFFU) {
            break;
        }
    }
    if (token == 0xFFU) {
        return SDCARD_ERR_TIMEOUT;
    }
    if (token != TOKEN_START_BLOCK) {
        return SDCARD_ERR_IO;
    }

    while (done < len) {
        uint16_t piece = ((uint16_t)(len - done) > CHUNK_BYTES) ? (uint16_t)CHUNK_BYTES : (uint16_t)(len - done);

        ret = spi_read(sd, buf + done, piece);
        if (ret != SDCARD_OK) {
            return ret;
        }
        done = (uint16_t)(done + piece);
    }
    return spi_read(sd, crc, 2U);
}

/* Writes one 512 byte block with its start token and waits until the card finishes programming. */
static int write_data(sdcard_t *sd, const uint8_t *buf)
{
    const uint8_t start[2] = { 0xFFU, TOKEN_START_BLOCK };   /* one idle byte, then the token */
    const uint8_t crc[2]   = { 0xFFU, 0xFFU };               /* CRC is not checked in SPI mode */
    uint8_t response = 0xFFU;
    int ret = spi_write(sd, start, 2U);

    for (uint32_t done = 0U; (ret == SDCARD_OK) && (done < SDCARD_BLOCK_SIZE); done += CHUNK_BYTES) {
        ret = spi_write(sd, buf + done, (uint16_t)CHUNK_BYTES);
    }
    if (ret == SDCARD_OK) {
        ret = spi_write(sd, crc, 2U);
    }
    if (ret == SDCARD_OK) {
        ret = spi_read(sd, &response, 1U);
    }
    if (ret != SDCARD_OK) {
        return ret;
    }
    if ((response & DATA_RESP_MASK) != DATA_RESP_ACCEPTED) {
        return SDCARD_ERR_IO;
    }
    return wait_ready(sd, READY_WAIT_MS);
}

/* Computes the number of 512 byte blocks from the CSD register. */
static int blocks_from_csd(const uint8_t *csd, uint32_t *blocks)
{
    uint64_t count;
    uint32_t c_size;

    if ((csd[0] >> 6) == 1U) {
        /* CSD version 2: capacity is (C_SIZE + 1) * 512 KiB. */
        c_size = ((uint32_t)(csd[7] & 0x3FU) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
        count = ((uint64_t)c_size + 1U) * 1024U;
    } else if ((csd[0] >> 6) == 0U) {
        /* CSD version 1: capacity is (C_SIZE + 1) * 2^(C_SIZE_MULT + 2) * 2^READ_BL_LEN bytes. */
        uint32_t read_bl_len = csd[5] & 0x0FU;
        uint32_t c_size_mult = ((uint32_t)(csd[9] & 0x03U) << 1) | (csd[10] >> 7);

        c_size = ((uint32_t)(csd[6] & 0x03U) << 10) | ((uint32_t)csd[7] << 2) | (csd[8] >> 6);
        if (read_bl_len < 9U) {
            return SDCARD_ERR_UNSUPPORTED;
        }
        count = ((uint64_t)c_size + 1U) << (c_size_mult + 2U + read_bl_len - 9U);
    } else {
        return SDCARD_ERR_UNSUPPORTED;
    }

    if ((count == 0U) || (count > 0xFFFFFFFFULL)) {
        return SDCARD_ERR_UNSUPPORTED;
    }
    *blocks = (uint32_t)count;
    return SDCARD_OK;
}

/* Full identification sequence at the slow clock. The card is left selected, the caller releases it. */
static int identify(sdcard_t *sd)
{
    uint8_t power_up[POWER_UP_CLOCK_BYTES];
    uint8_t trailer[4];
    uint8_t csd[CSD_BYTES];
    uint8_t r1 = 0xFFU;
    bool idle = false;
    bool v2 = false;
    bool ready = false;
    int ret;

    memset(power_up, 0xFF, sizeof(power_up));
    sx_spi_deselect(&sd->spi);
    ret = spi_write(sd, power_up, (uint16_t)sizeof(power_up));
    if (ret != SDCARD_OK) {
        return ret;
    }
    sx_spi_select(&sd->spi);

    for (uint32_t i = 0U; (i < CMD0_TRIES) && !idle; i++) {
        ret = command(sd, CMD0, 0U, CRC_CMD0, &r1);
        if (ret == SDCARD_ERR_BUS) {
            return ret;
        }
        idle = (ret == SDCARD_OK) && (r1 == R1_IDLE);
    }
    if (!idle) {
        return SDCARD_ERR_NO_CARD;
    }

    ret = command(sd, CMD8, ARG_CMD8_PATTERN, CRC_CMD8, &r1);
    if (ret != SDCARD_OK) {
        return ret;
    }
    if (r1 == R1_IDLE) {
        ret = spi_read(sd, trailer, 4U);
        if (ret != SDCARD_OK) {
            return ret;
        }
        if ((trailer[2] != 0x01U) || (trailer[3] != 0xAAU)) {
            return SDCARD_ERR_UNSUPPORTED;
        }
        v2 = true;
    } else if ((r1 & R1_ILLEGAL_CMD) == 0U) {
        return SDCARD_ERR_IO;
    }

    for (uint32_t i = 0U; (i < ACMD41_TRIES) && !ready; i++) {
        ret = app_command(sd, ACMD41, v2 ? ARG_HCS : 0U, &r1);
        if (ret != SDCARD_OK) {
            return ret;
        }
        if ((r1 & R1_ILLEGAL_CMD) != 0U) {
            return SDCARD_ERR_UNSUPPORTED;   /* not an SD card, probably MMC */
        }
        ready = (r1 == R1_READY);
    }
    if (!ready) {
        return SDCARD_ERR_TIMEOUT;
    }

    sd->high_capacity = false;
    if (v2) {
        ret = command(sd, CMD58, 0U, CRC_DEFAULT, &r1);
        if (ret != SDCARD_OK) {
            return ret;
        }
        if (r1 != R1_READY) {
            return SDCARD_ERR_IO;
        }
        ret = spi_read(sd, trailer, 4U);
        if (ret != SDCARD_OK) {
            return ret;
        }
        sd->high_capacity = ((trailer[0] & OCR_CCS_BYTE0_MASK) != 0U);
    }

    if (!sd->high_capacity) {
        ret = command(sd, CMD16, SDCARD_BLOCK_SIZE, CRC_DEFAULT, &r1);
        if (ret != SDCARD_OK) {
            return ret;
        }
        if (r1 != R1_READY) {
            return SDCARD_ERR_IO;
        }
    }

    ret = command(sd, CMD9, 0U, CRC_DEFAULT, &r1);
    if (ret != SDCARD_OK) {
        return ret;
    }
    if (r1 != R1_READY) {
        return SDCARD_ERR_IO;
    }
    ret = read_data(sd, csd, CSD_BYTES);
    if (ret != SDCARD_OK) {
        return ret;
    }
    return blocks_from_csd(csd, &sd->block_count);
}

static bool config_is_valid(const sdcard_config_t *config)
{
    const sx_spi_config_t *spi;
    uint32_t init_clock;

    if ((config == NULL) || (config->spi == NULL)) {
        return false;
    }
    spi = config->spi;
    init_clock = (config->init_clock_hz != 0U) ? config->init_clock_hz : SDCARD_INIT_CLOCK_HZ;

    return (spi->cs != NULL) &&
           (spi->dummy == 0xFFU) &&
           (spi->mode == SX_SPI_MODE_0) &&
           !spi->lsb_first &&
           (spi->clock_hz != 0U) && (spi->clock_hz <= MAX_DATA_CLOCK_HZ) &&
           (init_clock <= MAX_INIT_CLOCK_HZ) &&
           ((spi->max_transfer_bytes == 0U) || (spi->max_transfer_bytes >= CHUNK_BYTES));
}

int sdcard_init(sdcard_t *sd, const sdcard_config_t *config)
{
    int ret;

    if ((sd == NULL) || !config_is_valid(config)) {
        return SDCARD_ERR_PARAM;
    }
    if (sd->ready) {
        return SDCARD_ERR_STATE;
    }

    sd->config = config;
    sd->init_spi_config = *config->spi;
    sd->init_spi_config.clock_hz = (config->init_clock_hz != 0U) ? config->init_clock_hz : SDCARD_INIT_CLOCK_HZ;
    sd->active_clock_hz = sd->init_spi_config.clock_hz;
    sd->block_count = 0U;
    sd->high_capacity = false;

    ret = map_spi(sx_spi_init(&sd->spi, &sd->init_spi_config));
    if (ret != SDCARD_OK) {
        return ret;
    }

    ret = identify(sd);
    release(sd);
    if (ret != SDCARD_OK) {
        (void)sx_spi_deinit(&sd->spi);
        return ret;
    }

    /* The device clock is fixed when the device is added, so the device is added again at the data clock. */
    ret = map_spi(sx_spi_deinit(&sd->spi));
    if (ret != SDCARD_OK) {
        return ret;
    }
    ret = map_spi(sx_spi_init(&sd->spi, config->spi));
    if (ret != SDCARD_OK) {
        return ret;
    }

    sd->active_clock_hz = config->spi->clock_hz;
    sd->ready = true;
    return SDCARD_OK;
}

int sdcard_deinit(sdcard_t *sd)
{
    int ret;

    if (sd == NULL) {
        return SDCARD_ERR_PARAM;
    }
    if (!sd->ready) {
        return SDCARD_OK;
    }

    ret = map_spi(sx_spi_deinit(&sd->spi));
    if (ret == SDCARD_OK) {
        sd->ready = false;
        sd->block_count = 0U;
    }
    return ret;
}

static bool range_is_valid(const sdcard_t *sd, uint32_t first_block, uint32_t count)
{
    return (first_block < sd->block_count) && (count <= (sd->block_count - first_block));
}

static uint32_t block_address(const sdcard_t *sd, uint32_t block)
{
    return sd->high_capacity ? block : (block * SDCARD_BLOCK_SIZE);
}

int sdcard_read_blocks(sdcard_t *sd, uint32_t first_block, uint8_t *buf, uint32_t count)
{
    int ret = SDCARD_OK;

    if (sd == NULL) {
        return SDCARD_ERR_PARAM;
    }
    if (!sd->ready) {
        return SDCARD_ERR_STATE;
    }
    if (count == 0U) {
        return SDCARD_OK;
    }
    if ((buf == NULL) || !range_is_valid(sd, first_block, count)) {
        return SDCARD_ERR_PARAM;
    }

    sx_spi_select(&sd->spi);
    for (uint32_t i = 0U; (i < count) && (ret == SDCARD_OK); i++) {
        uint8_t r1 = 0xFFU;

        ret = command(sd, CMD17, block_address(sd, first_block + i), CRC_DEFAULT, &r1);
        if (ret == SDCARD_OK) {
            ret = (r1 == R1_READY) ? read_data(sd, buf + (i * SDCARD_BLOCK_SIZE), (uint16_t)SDCARD_BLOCK_SIZE)
                                   : SDCARD_ERR_IO;
        }
    }
    release(sd);
    return ret;
}

int sdcard_write_blocks(sdcard_t *sd, uint32_t first_block, const uint8_t *buf, uint32_t count)
{
    int ret = SDCARD_OK;

    if (sd == NULL) {
        return SDCARD_ERR_PARAM;
    }
    if (!sd->ready) {
        return SDCARD_ERR_STATE;
    }
    if (count == 0U) {
        return SDCARD_OK;
    }
    if ((buf == NULL) || !range_is_valid(sd, first_block, count)) {
        return SDCARD_ERR_PARAM;
    }

    sx_spi_select(&sd->spi);
    for (uint32_t i = 0U; (i < count) && (ret == SDCARD_OK); i++) {
        uint8_t r1 = 0xFFU;

        ret = command(sd, CMD24, block_address(sd, first_block + i), CRC_DEFAULT, &r1);
        if (ret == SDCARD_OK) {
            ret = (r1 == R1_READY) ? write_data(sd, buf + (i * SDCARD_BLOCK_SIZE)) : SDCARD_ERR_IO;
        }
    }
    release(sd);
    return ret;
}

uint32_t sdcard_block_count(const sdcard_t *sd)
{
    return ((sd != NULL) && sd->ready) ? sd->block_count : 0U;
}