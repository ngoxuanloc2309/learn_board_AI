#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "storage.h"
#include "sx_time.h"
#include "storage_app.h"

#define TEST_CHUNK   512U
#define SMALL_FILE   "/lb_small.txt"
#define BIG_FILE     "/lb_big.bin"

static const char s_small_text[] = "AI Learning Device storage test, small file.\n";

/* Static buffers keep the task stack small. Only this task uses them. */
static char     s_small_read[64];
static uint8_t  s_chunk[TEST_CHUNK];
static uint8_t  s_check[TEST_CHUNK];
static unsigned s_failures;
static bool     s_small_created;
static bool     s_big_created;

/* The small text must fit the read buffer, otherwise the test reads nothing useful. */
typedef char small_text_fits_check[((sizeof(s_small_text) - 1U) <= sizeof(s_small_read)) ? 1 : -1];

static void check_fail(const char *what, int code)
{
    s_failures++;
    printf("[storage] FAIL %s: code %d\n", what, code);
}

/* Value of byte number offset of the test pattern. The block number is mixed in, so blocks differ. */
static uint8_t pattern_byte(uint32_t offset)
{
    return (uint8_t)((offset * 7U) ^ (offset >> 9) ^ 0x5AU);
}

static void fill_pattern(uint8_t *buf, uint32_t offset, uint32_t len)
{
    uint32_t i;

    for (i = 0U; i < len; i++) {
        buf[i] = pattern_byte(offset + i);
    }
}

/* Prints what the mount error most likely means. */
static void print_mount_hint(int code)
{
    printf("[storage] storage_mount failed: %d\n", code);
    if (code == STORAGE_ERR_NO_CARD) {
        printf("[storage] no card answered: check that a FAT32 card is inserted, and the SD pins in board.h (REF, not verified on V15)\n");
    } else if (code == STORAGE_ERR_NO_FS) {
        printf("[storage] the card has no FAT file system: format it as FAT32 on a PC (the board never formats)\n");
    } else if (code == STORAGE_ERR_IO) {
        printf("[storage] card or file system error: try another card, and check the SD pins and the wiring\n");
    } else {
        printf("[storage] unexpected code, see STORAGE_ERR_* in storage.h\n");
    }
}

static int bring_up(void)
{
    int ret = board_init();

    if (ret != BOARD_OK) {
        printf("[storage] board_init failed: %d\n", ret);
        return ret;
    }
    ret = board_init_storage();
    if (ret != BOARD_OK) {
        printf("[storage] board_init_storage failed: %d\n", ret);
        return ret;
    }
    printf("[storage] mounting, this can take a few seconds when no card is present\n");
    ret = storage_mount();
    if (ret != STORAGE_OK) {
        print_mount_hint(ret);
        return ret;
    }
    return STORAGE_OK;
}

static void show_space(void)
{
    uint64_t total = 0U;
    uint64_t free_bytes = 0U;
    int ret = storage_get_space(&total, &free_bytes);

    if (ret != STORAGE_OK) {
        check_fail("storage_get_space", ret);
        return;
    }
    printf("[storage] mounted: total %u MiB, free %u MiB\n",
           (unsigned)(total >> 20), (unsigned)(free_bytes >> 20));
}

static bool list_entry(const storage_entry_t *entry, void *ctx)
{
    unsigned *shown = (unsigned *)ctx;

    if (*shown >= APP_STORAGE_LIST_MAX) {
        return false;
    }
    printf("[storage]   %s %s size=%u\n", entry->is_dir ? "dir " : "file", entry->name,
           (unsigned)entry->size);
    (*shown)++;
    return true;
}

static void show_root(void)
{
    unsigned shown = 0U;
    int ret = storage_list("/", list_entry, &shown);

    if (ret < 0) {
        check_fail("storage_list", ret);
        return;
    }
    printf("[storage] root directory: %d entries visited, %u printed\n", ret, shown);
}

/* Returns true when the path is free. A file of the owner must never be replaced by the test. */
static bool path_is_free(const char *path)
{
    storage_entry_t entry;
    int ret = storage_stat(path, &entry);

    if (ret == STORAGE_ERR_NOT_FOUND) {
        return true;
    }
    if (ret == STORAGE_OK) {
        printf("[storage] %s already exists, the test stops so that it is not replaced\n", path);
        s_failures++;
    } else {
        check_fail("storage_stat before the test", ret);
    }
    return false;
}

static void test_small(void)
{
    uint32_t length = 0U;
    uint32_t text_len = (uint32_t)(sizeof(s_small_text) - 1U);
    int ret;

    ret = storage_write_file(SMALL_FILE, s_small_text, text_len);
    if (ret != STORAGE_OK) {
        check_fail("small file write", ret);
        return;
    }
    s_small_created = true;

    memset(s_small_read, 0, sizeof(s_small_read));
    ret = storage_read_file(SMALL_FILE, s_small_read, (uint32_t)sizeof(s_small_read), &length);
    if (ret != STORAGE_OK) {
        check_fail("small file read", ret);
        return;
    }
    if ((length != text_len) || (memcmp(s_small_read, s_small_text, text_len) != 0)) {
        printf("[storage] FAIL small file: read back %u bytes, expected %u, content differs or length is wrong\n",
               (unsigned)length, (unsigned)text_len);
        s_failures++;
        return;
    }
    printf("[storage] ok small file: %u bytes written and read back\n", (unsigned)text_len);
}

/* Writes the pattern in pieces. Returns STORAGE_OK or the first error. */
static int write_big(uint32_t total)
{
    uint32_t done = 0U;
    int file = storage_open(BIG_FILE, STORAGE_MODE_WRITE);
    int ret = STORAGE_OK;

    if (file < 0) {
        return file;
    }
    s_big_created = true;
    while ((done < total) && (ret == STORAGE_OK)) {
        uint32_t piece = ((total - done) < TEST_CHUNK) ? (total - done) : TEST_CHUNK;
        int put;

        fill_pattern(s_chunk, done, piece);
        put = storage_write(file, s_chunk, piece);
        if (put < 0) {
            ret = put;
        } else if ((uint32_t)put != piece) {
            ret = STORAGE_ERR_FULL;
        } else {
            done += piece;
        }
    }
    if (ret == STORAGE_OK) {
        ret = storage_close(file);
    } else {
        (void)storage_close(file);
    }
    return ret;
}

/* Reads the file back and compares it with the pattern. Returns STORAGE_OK, an error, or STORAGE_ERR_IO on a mismatch. */
static int verify_big(uint32_t total)
{
    uint32_t done = 0U;
    int file = storage_open(BIG_FILE, STORAGE_MODE_READ);
    int ret = STORAGE_OK;

    if (file < 0) {
        return file;
    }
    while ((done < total) && (ret == STORAGE_OK)) {
        uint32_t piece = ((total - done) < TEST_CHUNK) ? (total - done) : TEST_CHUNK;
        int got = storage_read(file, s_check, piece);

        if (got < 0) {
            ret = got;
        } else if (got == 0) {
            printf("[storage] FAIL big file: the file ends at %u bytes, expected %u\n",
                   (unsigned)done, (unsigned)total);
            ret = STORAGE_ERR_IO;
        } else {
            uint32_t i;

            fill_pattern(s_chunk, done, (uint32_t)got);
            for (i = 0U; i < (uint32_t)got; i++) {
                if (s_check[i] != s_chunk[i]) {
                    printf("[storage] FAIL big file: byte %u is 0x%02X, expected 0x%02X\n",
                           (unsigned)(done + i), (unsigned)s_check[i], (unsigned)s_chunk[i]);
                    ret = STORAGE_ERR_IO;
                    break;
                }
            }
            done += (uint32_t)got;
        }
    }
    (void)storage_close(file);
    return ret;
}

/* Bytes per second from a byte count and a time in ms, 0 when the time is too short to measure. */
static unsigned rate_kib_s(uint32_t bytes, uint32_t ms)
{
    if (ms == 0U) {
        return 0U;
    }
    return (unsigned)(((uint64_t)bytes * 1000U) / ((uint64_t)ms * 1024U));
}

static void test_big(void)
{
    const uint32_t total = APP_STORAGE_BIG_FILE_BYTES;
    storage_entry_t entry;
    uint32_t t0 = 0U;
    uint32_t t1 = 0U;
    uint32_t t2 = 0U;
    uint32_t t3 = 0U;
    int ret;

    if (total == 0U) {
        printf("[storage] big file test skipped (APP_STORAGE_BIG_FILE_BYTES is 0)\n");
        return;
    }
    sx_get_time_ms(&t0);
    ret = write_big(total);
    sx_get_time_ms(&t1);
    if (ret != STORAGE_OK) {
        check_fail("big file write", ret);
        return;
    }
    ret = storage_stat(BIG_FILE, &entry);
    if (ret != STORAGE_OK) {
        check_fail("big file stat", ret);
        return;
    }
    if (entry.size != total) {
        printf("[storage] FAIL big file: stat says %u bytes, expected %u\n",
               (unsigned)entry.size, (unsigned)total);
        s_failures++;
        return;
    }
    sx_get_time_ms(&t2);
    ret = verify_big(total);
    sx_get_time_ms(&t3);
    if (ret != STORAGE_OK) {
        check_fail("big file read", ret);
        return;
    }
    /* Times are measured by subtraction, so they stay correct when the counter wraps. */
    printf("[storage] ok big file: %u bytes, write %u ms (about %u KiB/s), read and compare %u ms (about %u KiB/s)\n",
           (unsigned)total, (unsigned)(t1 - t0), rate_kib_s(total, t1 - t0),
           (unsigned)(t3 - t2), rate_kib_s(total, t3 - t2));
}

/* Removes the files this test created, and only those. */
static void clean_up(void)
{
    int ret;

    if (s_small_created) {
        ret = storage_remove(SMALL_FILE);
        if (ret != STORAGE_OK) {
            check_fail("remove small file", ret);
        }
        s_small_created = false;
    }
    if (s_big_created) {
        ret = storage_remove(BIG_FILE);
        if (ret != STORAGE_OK) {
            check_fail("remove big file", ret);
        }
        s_big_created = false;
    }
}

static void run_tests(void)
{
    show_space();
    show_root();
    if (!path_is_free(SMALL_FILE) || !path_is_free(BIG_FILE)) {
        return;
    }
    test_small();
    test_big();
    clean_up();
}

/* Prints the lowest free stack, then ends the calling task. */
static void end_task(void)
{
    printf("[storage] task ends, lowest free stack %u of %u bytes\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)APP_STORAGE_STACK_SIZE);
    vTaskDelete(NULL);
}

static void storage_task(void *arg)
{
    int ret;

    (void)arg;
    s_failures = 0U;
    ret = bring_up();
    if (ret != STORAGE_OK) {
        end_task();
        return;
    }
    run_tests();
    ret = storage_unmount();
    if (ret != STORAGE_OK) {
        check_fail("storage_unmount", ret);
    }
    printf("[storage] RESULT: %s (%u failures)\n", (s_failures == 0U) ? "PASS" : "FAIL", s_failures);
    end_task();
}

int storage_app_start(void)
{
    if (xTaskCreate(storage_task, APP_STORAGE_TASK_NAME, APP_STORAGE_STACK_SIZE, NULL,
                    APP_STORAGE_TASK_PRIORITY, NULL) != pdPASS) {
        printf("[storage] cannot create the task\n");
        return -1;
    }
    return 0;
}