#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "ff.h"
#include "diskio.h"
#include "sdcard.h"
#include "storage_diskio.h"

#define ERASE_BLOCK_UNKNOWN   1U   /* FatFs convention for "erase block size not known" */

static sdcard_t *s_card;

static bool drive_is_ready(BYTE pdrv)
{
    return (pdrv == 0U) && (s_card != NULL) && (sdcard_block_count(s_card) != 0U);
}

static DRESULT map_result(int ret)
{
    switch (ret) {
    case SDCARD_OK:         return RES_OK;
    case SDCARD_ERR_PARAM:  return RES_PARERR;
    case SDCARD_ERR_STATE:  return RES_NOTRDY;
    default:                return RES_ERROR;
    }
}

void storage_diskio_attach(sdcard_t *card)
{
    s_card = card;
}

void storage_diskio_detach(void)
{
    s_card = NULL;
}

DSTATUS disk_status(BYTE pdrv)
{
    return drive_is_ready(pdrv) ? (DSTATUS)0U : (DSTATUS)STA_NOINIT;
}

/* The card is brought up by the sdcard module before the volume is mounted, so there is nothing to start here. */
DSTATUS disk_initialize(BYTE pdrv)
{
    return disk_status(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (!drive_is_ready(pdrv)) {
        return RES_NOTRDY;
    }
    if ((buff == NULL) || (count == 0U)) {
        return RES_PARERR;
    }
    return map_result(sdcard_read_blocks(s_card, (uint32_t)sector, buff, (uint32_t)count));
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (!drive_is_ready(pdrv)) {
        return RES_NOTRDY;
    }
    if ((buff == NULL) || (count == 0U)) {
        return RES_PARERR;
    }
    return map_result(sdcard_write_blocks(s_card, (uint32_t)sector, buff, (uint32_t)count));
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (!drive_is_ready(pdrv)) {
        return RES_NOTRDY;
    }

    switch (cmd) {
    case CTRL_SYNC:
        /* sdcard_write_blocks returns only after the card finished programming, nothing is cached here. */
        return RES_OK;
    case GET_SECTOR_COUNT:
        if (buff == NULL) {
            return RES_PARERR;
        }
        *(LBA_t *)buff = (LBA_t)sdcard_block_count(s_card);
        return RES_OK;
    case GET_SECTOR_SIZE:
        if (buff == NULL) {
            return RES_PARERR;
        }
        *(WORD *)buff = (WORD)SDCARD_BLOCK_SIZE;
        return RES_OK;
    case GET_BLOCK_SIZE:
        if (buff == NULL) {
            return RES_PARERR;
        }
        *(DWORD *)buff = ERASE_BLOCK_UNKNOWN;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}