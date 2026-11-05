#ifndef STORAGE_H
#define STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "sdcard.h"

/*
 * File service on the microSD card: create, open, read, write, list, rename and delete files.
 * Built on the sdcard module (block device) and the FatFs library (FAT file system).
 *
 * One card and one volume. Paths are absolute and start with '/', for example "/lessons/unit1.json".
 * Long file names are supported (up to 255 characters, code page 437), the whole path must be shorter
 * than STORAGE_PATH_MAX. There is no current directory.
 *
 * Not thread safe. FatFs is built without re-entrancy support and uses one shared name buffer,
 * so every call must come from one task (the storage task of the app layer) or be serialised by the caller.
 * Calls block while the card works. Do not call from the LVGL task or from an ISR.
 *
 * Files have a fixed time stamp because the library is built without a real time clock.
 * FatFs is built without file locking, so never remove, rename or open for writing a file that is already open.
 */

/* Return codes: 0 or a positive count on success, negative on failure. */
#define STORAGE_OK                 0
#define STORAGE_ERR_PARAM         (-1)   /* NULL pointer, bad path, bad handle, or bad argument */
#define STORAGE_ERR_STATE         (-2)   /* not initialised, not mounted, or already mounted */
#define STORAGE_ERR_NO_CARD       (-3)   /* the card is missing or did not answer */
#define STORAGE_ERR_IO            (-4)   /* the card or the file system reported an error */
#define STORAGE_ERR_NO_FS         (-5)   /* the card has no usable FAT file system */
#define STORAGE_ERR_NOT_FOUND     (-6)   /* file or directory does not exist */
#define STORAGE_ERR_EXISTS        (-7)   /* the name is already used */
#define STORAGE_ERR_DENIED        (-8)   /* directory not empty, directory full, or access not allowed */
#define STORAGE_ERR_FULL          (-9)   /* the card has no free space left */
#define STORAGE_ERR_NO_HANDLE     (-10)  /* too many open files */
#define STORAGE_ERR_TOO_BIG       (-11)  /* the file does not fit in the buffer */

/* Most files that can be open at the same time. Static memory, about 0.6 KiB per file. */
#ifndef STORAGE_MAX_OPEN_FILES
#define STORAGE_MAX_OPEN_FILES     4U
#endif

/* Longest path accepted, including the terminating zero. */
#define STORAGE_PATH_MAX           128U

typedef enum {
    STORAGE_MODE_READ   = 0,   /* file must exist */
    STORAGE_MODE_WRITE  = 1,   /* create the file, or empty it when it exists */
    STORAGE_MODE_APPEND = 2    /* create the file when missing, write at the end */
} storage_mode_t;

typedef struct storage_config {
    sdcard_t              *card;                    /* owned by the board layer, zero-initialised before first use */
    const sdcard_config_t *card_config;             /* used to bring the card up when it is not ready yet, may be NULL
                                                       when the owner calls sdcard_init itself */
    bool                   format_if_mount_failed;  /* true: create a new FAT file system when the card has none.
                                                       This ERASES the card, keep it false unless that is wanted */
} storage_config_t;

typedef struct storage_entry {
    const char *name;     /* file name, valid only during the list callback, NULL for storage_stat */
    uint32_t    size;     /* bytes, 0 for a directory */
    bool        is_dir;
} storage_entry_t;

/* Called once per directory entry. Return false to stop the listing. */
typedef bool (*storage_list_callback_t)(const storage_entry_t *entry, void *ctx);

/* Stores the configuration. Does not touch the card. The config must stay valid. */
int storage_init(const storage_config_t *config);

/*
 * Brings the card up when needed and mounts the FAT file system.
 * Returns STORAGE_ERR_NO_CARD without a card and STORAGE_ERR_NO_FS when the card has no file system
 * (and format_if_mount_failed is false). Can take a few seconds when no card is present.
 * When the card is not ready and card_config is NULL, returns STORAGE_ERR_STATE.
 */
int storage_mount(void);

/* Closes every open file and unmounts. Releases the card only when storage_mount brought it up. */
int storage_unmount(void);

bool storage_is_mounted(void);

/* Capacity and free space of the volume in bytes. Either pointer may be NULL. */
int storage_get_space(uint64_t *total_bytes, uint64_t *free_bytes);

/* Returns a file handle (0 or more) or a negative error code. */
int storage_open(const char *path, storage_mode_t mode);
int storage_close(int file);

/*
 * Return the number of bytes moved. storage_read returns 0 at the end of the file.
 * storage_write returns the bytes written, which can be smaller than len when the card fills up,
 * and STORAGE_ERR_FULL when nothing at all could be written. len must not exceed INT32_MAX.
 */
int storage_read(int file, void *buf, uint32_t len);
int storage_write(int file, const void *data, uint32_t len);

/*
 * Moves the file position. In a file opened for writing, a position past the end grows the file
 * (STORAGE_ERR_FULL when the card cannot hold it). In a file opened for reading it stops at the end.
 */
int storage_seek(int file, uint32_t offset);

/* Writes cached data to the card. storage_close does it too. */
int storage_sync(int file);
int storage_size(int file, uint32_t *size);

/*
 * Whole file helpers. storage_read_file sets *length to the file size and returns STORAGE_ERR_TOO_BIG
 * when it exceeds capacity (nothing is copied then). storage_write_file creates or replaces the file.
 * The file is not replaced atomically: a power loss during the write can leave a partial file.
 */
int storage_read_file(const char *path, void *buf, uint32_t capacity, uint32_t *length);
int storage_write_file(const char *path, const void *data, uint32_t length);

/* Fills entry for a file or directory. The name field of entry is NULL. The path "/" is the root directory. */
int storage_stat(const char *path, storage_entry_t *entry);

/* Returns the number of entries visited, or a negative error code. callback may be NULL to only count. */
int storage_list(const char *dir_path, storage_list_callback_t callback, void *ctx);

int storage_mkdir(const char *path);

/* Deletes a file or an empty directory. */
int storage_remove(const char *path);

/* Renames or moves a file or directory. Fails with STORAGE_ERR_EXISTS when new_path exists. */
int storage_rename(const char *old_path, const char *new_path);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_H */