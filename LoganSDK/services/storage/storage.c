#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ff.h"
#include "sdcard.h"
#include "storage.h"
#include "storage_diskio.h"

/* The only logical drive. An empty path means drive 0 in FatFs. */
#define VOLUME_PATH   ""

typedef struct file_slot {
    FIL  fil;
    bool used;
    bool writable;
} file_slot_t;

static const storage_config_t *s_config;
static FATFS                   s_fs;
static file_slot_t             s_files[STORAGE_MAX_OPEN_FILES];
static BYTE                    s_work[FF_MAX_SS];   /* work area for f_mkfs, used only when formatting */
static bool                    s_mounted;
static bool                    s_card_owned;        /* true when storage_mount called sdcard_init */

static int map_fresult(FRESULT fr)
{
    switch (fr) {
    case FR_OK:                  return STORAGE_OK;
    case FR_NOT_READY:           return STORAGE_ERR_NO_CARD;
    case FR_NO_FILE:
    case FR_NO_PATH:             return STORAGE_ERR_NOT_FOUND;
    case FR_INVALID_NAME:
    case FR_INVALID_DRIVE:
    case FR_NOT_ENOUGH_CORE:     return STORAGE_ERR_PARAM;
    case FR_DENIED:
    case FR_WRITE_PROTECTED:
    case FR_LOCKED:              return STORAGE_ERR_DENIED;
    case FR_EXIST:               return STORAGE_ERR_EXISTS;
    case FR_NOT_ENABLED:         return STORAGE_ERR_STATE;
    case FR_NO_FILESYSTEM:       return STORAGE_ERR_NO_FS;
    case FR_TOO_MANY_OPEN_FILES: return STORAGE_ERR_NO_HANDLE;
    default:                     return STORAGE_ERR_IO;   /* disk error, internal error, bad object, timeout, mkfs abort */
    }
}

static int map_card_error(int ret)
{
    switch (ret) {
    case SDCARD_OK:             return STORAGE_OK;
    case SDCARD_ERR_PARAM:      return STORAGE_ERR_PARAM;
    case SDCARD_ERR_STATE:      return STORAGE_ERR_STATE;
    case SDCARD_ERR_NO_CARD:    return STORAGE_ERR_NO_CARD;
    default:                    return STORAGE_ERR_IO;
    }
}

/* A path must start with '/' and be shorter than STORAGE_PATH_MAX. */
static bool path_is_valid(const char *path)
{
    size_t len;

    if (path == NULL) {
        return false;
    }
    len = strnlen(path, STORAGE_PATH_MAX);
    return (len > 0U) && (len < STORAGE_PATH_MAX) && (path[0] == '/');
}

static bool path_is_root(const char *path)
{
    return (path[0] == '/') && (path[1] == '\0');
}

static bool handle_is_valid(int file)
{
    return (file >= 0) && ((unsigned int)file < STORAGE_MAX_OPEN_FILES) && s_files[file].used;
}

static void close_all_files(int *first_error)
{
    unsigned int i;

    for (i = 0U; i < STORAGE_MAX_OPEN_FILES; i++) {
        if (s_files[i].used) {
            int ret = map_fresult(f_close(&s_files[i].fil));
            if ((ret != STORAGE_OK) && (*first_error == STORAGE_OK)) {
                *first_error = ret;
            }
            s_files[i].used = false;
        }
    }
}

/* Drops the volume and the card after a failed mount. */
static void abort_mount(void)
{
    (void)f_mount(NULL, VOLUME_PATH, 0);
    storage_diskio_detach();
    if (s_card_owned) {
        (void)sdcard_deinit(s_config->card);
        s_card_owned = false;
    }
}

int storage_init(const storage_config_t *config)
{
    if ((config == NULL) || (config->card == NULL)) {
        return STORAGE_ERR_PARAM;
    }
    if (s_mounted) {
        return STORAGE_ERR_STATE;
    }
    s_config = config;
    return STORAGE_OK;
}

int storage_mount(void)
{
    FRESULT fr;
    int ret;

    if ((s_config == NULL) || s_mounted) {
        return STORAGE_ERR_STATE;
    }

    s_card_owned = false;
    if (sdcard_block_count(s_config->card) == 0U) {
        if (s_config->card_config == NULL) {
            return STORAGE_ERR_STATE;
        }
        ret = map_card_error(sdcard_init(s_config->card, s_config->card_config));
        if (ret != STORAGE_OK) {
            return ret;
        }
        s_card_owned = true;
    }

    storage_diskio_attach(s_config->card);

    fr = f_mount(&s_fs, VOLUME_PATH, 1);
    if ((fr == FR_NO_FILESYSTEM) && s_config->format_if_mount_failed) {
        const MKFS_PARM opt = { FM_ANY, 0, 0, 0, 0 };

        fr = f_mkfs(VOLUME_PATH, &opt, s_work, (UINT)sizeof(s_work));
        if (fr == FR_OK) {
            fr = f_mount(&s_fs, VOLUME_PATH, 1);
        }
    }
    if (fr != FR_OK) {
        ret = map_fresult(fr);
        abort_mount();
        return ret;
    }

    s_mounted = true;
    return STORAGE_OK;
}

int storage_unmount(void)
{
    int first_error = STORAGE_OK;

    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }

    close_all_files(&first_error);
    (void)f_mount(NULL, VOLUME_PATH, 0);
    storage_diskio_detach();
    if (s_card_owned) {
        (void)sdcard_deinit(s_config->card);
        s_card_owned = false;
    }
    s_mounted = false;
    return first_error;
}

bool storage_is_mounted(void)
{
    return s_mounted;
}

int storage_get_space(uint64_t *total_bytes, uint64_t *free_bytes)
{
    FATFS *fs = NULL;
    DWORD free_clusters = 0;
    FRESULT fr;

    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    fr = f_getfree(VOLUME_PATH, &free_clusters, &fs);
    if (fr != FR_OK) {
        return map_fresult(fr);
    }
    if (total_bytes != NULL) {
        *total_bytes = (uint64_t)(fs->n_fatent - 2U) * fs->csize * FF_MAX_SS;
    }
    if (free_bytes != NULL) {
        *free_bytes = (uint64_t)free_clusters * fs->csize * FF_MAX_SS;
    }
    return STORAGE_OK;
}

int storage_open(const char *path, storage_mode_t mode)
{
    BYTE flags;
    unsigned int i;
    FRESULT fr;

    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!path_is_valid(path)) {
        return STORAGE_ERR_PARAM;
    }

    switch (mode) {
    case STORAGE_MODE_READ:   flags = FA_READ;                          break;
    case STORAGE_MODE_WRITE:  flags = FA_WRITE | FA_CREATE_ALWAYS;      break;
    case STORAGE_MODE_APPEND: flags = FA_WRITE | FA_OPEN_APPEND;        break;
    default:                  return STORAGE_ERR_PARAM;
    }

    for (i = 0U; i < STORAGE_MAX_OPEN_FILES; i++) {
        if (!s_files[i].used) {
            break;
        }
    }
    if (i == STORAGE_MAX_OPEN_FILES) {
        return STORAGE_ERR_NO_HANDLE;
    }

    fr = f_open(&s_files[i].fil, path, flags);
    if (fr != FR_OK) {
        return map_fresult(fr);
    }
    s_files[i].used = true;
    s_files[i].writable = (mode != STORAGE_MODE_READ);
    return (int)i;
}

int storage_close(int file)
{
    FRESULT fr;

    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!handle_is_valid(file)) {
        return STORAGE_ERR_PARAM;
    }
    fr = f_close(&s_files[file].fil);
    s_files[file].used = false;
    return map_fresult(fr);
}

int storage_read(int file, void *buf, uint32_t len)
{
    UINT done = 0;
    FRESULT fr;

    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!handle_is_valid(file) || (len > (uint32_t)INT32_MAX) || ((buf == NULL) && (len != 0U))) {
        return STORAGE_ERR_PARAM;
    }
    if (len == 0U) {
        return 0;
    }
    fr = f_read(&s_files[file].fil, buf, (UINT)len, &done);
    if (fr != FR_OK) {
        return map_fresult(fr);
    }
    return (int)done;
}

int storage_write(int file, const void *data, uint32_t len)
{
    UINT done = 0;
    FRESULT fr;

    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!handle_is_valid(file) || (len > (uint32_t)INT32_MAX) || ((data == NULL) && (len != 0U))) {
        return STORAGE_ERR_PARAM;
    }
    if (len == 0U) {
        return 0;
    }
    fr = f_write(&s_files[file].fil, data, (UINT)len, &done);
    if (fr != FR_OK) {
        return map_fresult(fr);
    }
    if (done == 0U) {
        return STORAGE_ERR_FULL;
    }
    return (int)done;
}

int storage_seek(int file, uint32_t offset)
{
    FRESULT fr;

    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!handle_is_valid(file)) {
        return STORAGE_ERR_PARAM;
    }
    fr = f_lseek(&s_files[file].fil, (FSIZE_t)offset);
    if (fr != FR_OK) {
        return map_fresult(fr);
    }
    /* FatFs cuts the new size short when the card is full. A reader is always cut at the end of the file. */
    if (s_files[file].writable && (f_tell(&s_files[file].fil) != (FSIZE_t)offset)) {
        return STORAGE_ERR_FULL;
    }
    return STORAGE_OK;
}

int storage_sync(int file)
{
    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!handle_is_valid(file)) {
        return STORAGE_ERR_PARAM;
    }
    return map_fresult(f_sync(&s_files[file].fil));
}

int storage_size(int file, uint32_t *size)
{
    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!handle_is_valid(file) || (size == NULL)) {
        return STORAGE_ERR_PARAM;
    }
    *size = (uint32_t)f_size(&s_files[file].fil);
    return STORAGE_OK;
}

int storage_read_file(const char *path, void *buf, uint32_t capacity, uint32_t *length)
{
    int file;
    int ret;
    uint32_t size = 0;

    if ((length == NULL) || ((buf == NULL) && (capacity != 0U))) {
        return STORAGE_ERR_PARAM;
    }
    file = storage_open(path, STORAGE_MODE_READ);
    if (file < 0) {
        return file;
    }

    (void)storage_size(file, &size);
    *length = size;
    if (size > capacity) {
        (void)storage_close(file);
        return STORAGE_ERR_TOO_BIG;
    }

    ret = STORAGE_OK;
    if (size > 0U) {
        int got = storage_read(file, buf, size);
        if (got < 0) {
            ret = got;
        } else if ((uint32_t)got != size) {
            ret = STORAGE_ERR_IO;
        }
    }
    if (ret == STORAGE_OK) {
        ret = storage_close(file);
    } else {
        (void)storage_close(file);
    }
    return ret;
}

int storage_write_file(const char *path, const void *data, uint32_t length)
{
    int file;
    int ret;

    if ((data == NULL) && (length != 0U)) {
        return STORAGE_ERR_PARAM;
    }
    file = storage_open(path, STORAGE_MODE_WRITE);
    if (file < 0) {
        return file;
    }

    ret = STORAGE_OK;
    if (length > 0U) {
        int put = storage_write(file, data, length);
        if (put < 0) {
            ret = put;
        } else if ((uint32_t)put != length) {
            ret = STORAGE_ERR_FULL;
        }
    }
    if (ret == STORAGE_OK) {
        ret = storage_close(file);
    } else {
        (void)storage_close(file);
    }
    return ret;
}

int storage_stat(const char *path, storage_entry_t *entry)
{
    FILINFO info;
    FRESULT fr;

    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!path_is_valid(path) || (entry == NULL)) {
        return STORAGE_ERR_PARAM;
    }

    entry->name = NULL;
    if (path_is_root(path)) {
        entry->size = 0U;
        entry->is_dir = true;
        return STORAGE_OK;
    }

    fr = f_stat(path, &info);
    if (fr != FR_OK) {
        return map_fresult(fr);
    }
    entry->is_dir = ((info.fattrib & AM_DIR) != 0U);
    entry->size = entry->is_dir ? 0U : (uint32_t)info.fsize;
    return STORAGE_OK;
}

int storage_list(const char *dir_path, storage_list_callback_t callback, void *ctx)
{
    DIR dir;
    FILINFO info;
    FRESULT fr;
    int count = 0;

    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!path_is_valid(dir_path)) {
        return STORAGE_ERR_PARAM;
    }

    fr = f_opendir(&dir, dir_path);
    if (fr != FR_OK) {
        return map_fresult(fr);
    }

    for (;;) {
        storage_entry_t entry;

        fr = f_readdir(&dir, &info);
        if ((fr != FR_OK) || (info.fname[0] == '\0')) {
            break;
        }
        count++;
        if (callback != NULL) {
            entry.name = info.fname;
            entry.is_dir = ((info.fattrib & AM_DIR) != 0U);
            entry.size = entry.is_dir ? 0U : (uint32_t)info.fsize;
            if (!callback(&entry, ctx)) {
                break;
            }
        }
    }
    (void)f_closedir(&dir);

    return (fr == FR_OK) ? count : map_fresult(fr);
}

int storage_mkdir(const char *path)
{
    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!path_is_valid(path) || path_is_root(path)) {
        return STORAGE_ERR_PARAM;
    }
    return map_fresult(f_mkdir(path));
}

int storage_remove(const char *path)
{
    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!path_is_valid(path) || path_is_root(path)) {
        return STORAGE_ERR_PARAM;
    }
    return map_fresult(f_unlink(path));
}

int storage_rename(const char *old_path, const char *new_path)
{
    if (!s_mounted) {
        return STORAGE_ERR_STATE;
    }
    if (!path_is_valid(old_path) || !path_is_valid(new_path) || path_is_root(old_path) || path_is_root(new_path)) {
        return STORAGE_ERR_PARAM;
    }
    return map_fresult(f_rename(old_path, new_path));
}