#ifndef STORAGE_APP_H
#define STORAGE_APP_H

#ifndef APP_STORAGE_TASK_NAME
#define APP_STORAGE_TASK_NAME "app_storage_task"
#endif

/*
 * Stack in bytes. FatFs and the SD driver run inside this task, and storage_list keeps a directory
 * entry with a long file name on the stack. Chosen by design, not measured. The task prints the
 * lowest free stack when it ends, use that number to tune this value.
 */
#ifndef APP_STORAGE_STACK_SIZE
#define APP_STORAGE_STACK_SIZE 6144
#endif

/*
 * Below the Wi-Fi task (3), same as the HTTP and screen tasks. Chosen by design, not measured.
 * Session 16 lowered it from the earlier placeholder 4: mounting without a card can block this
 * task for seconds and it should not compete with Wi-Fi.
 */
#ifndef APP_STORAGE_TASK_PRIORITY
#define APP_STORAGE_TASK_PRIORITY 2
#endif

/* Size of the speed test file in bytes. Chosen by design. Use 0 to skip the big file test. */
#ifndef APP_STORAGE_BIG_FILE_BYTES
#define APP_STORAGE_BIG_FILE_BYTES 65536U
#endif

/* Most root directory entries printed by the test. Chosen by design. */
#ifndef APP_STORAGE_LIST_MAX
#define APP_STORAGE_LIST_MAX 16U
#endif

/*
 * Creates a task for the microSD bring-up test. The task is the only caller of storage_*, as
 * storage.h requires. It calls board_init (safe to call twice, screen_app calls it too) and
 * board_init_storage, then storage_mount. Mounting can take a few seconds when no card is present.
 *
 * The card must hold a FAT file system (FAT32 for 4 GB to 32 GB cards). The board does not format
 * the card (format_if_mount_failed is false in board.c), so a new card may need a format on a PC.
 * Cards of 64 GB and more use exFAT and are not supported, because FatFs is built with FF_FS_EXFAT 0.
 *
 * When the mount works, the task prints the capacity and the free space, lists the root directory
 * (at most APP_STORAGE_LIST_MAX entries), then runs these checks and prints one result line each:
 *   small file: writes a short text file with storage_write_file and reads it back.
 *   big file: writes APP_STORAGE_BIG_FILE_BYTES bytes of a fixed pattern in 512 byte pieces, checks
 *   the size with storage_stat, reads the pattern back and compares it. The write and read times
 *   are printed, so the speed can be seen.
 * The test files are /lb_small.txt and /lb_big.bin. If one of them already exists, the test stops
 * before it writes anything, so no file of the owner is replaced. The test removes the files it
 * created, unmounts, prints PASS or FAIL with the lowest free stack, and the task ends.
 * If the mount fails, the task prints the code with a hint and ends. Wi-Fi and HTTP are not affected.
 *
 * Returns 0 when the task was created, -1 when it could not be created. Call it once, from
 * app_start in app.c.
 */
int storage_app_start(void);

#endif /* STORAGE_APP_H */