#ifndef STORAGE_DISKIO_H
#define STORAGE_DISKIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sdcard.h"

/*
 * Glue between the FatFs low level disk interface (disk_read, disk_write, ...) and the sdcard module.
 * Internal to the storage service. FatFs is built with one volume, so there is one card.
 */

/* Selects the card that FatFs reads and writes. The card must already be initialised. */
void storage_diskio_attach(sdcard_t *card);

/* Forgets the card. FatFs reports the drive as not initialised afterwards. */
void storage_diskio_detach(void);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_DISKIO_H */