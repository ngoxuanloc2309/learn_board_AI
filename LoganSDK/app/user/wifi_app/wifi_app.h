#ifndef WIFI_APP_H
#define WIFI_APP_H

#ifndef APP_WIFI_TASK_NAME
#define APP_WIFI_TASK_NAME "app_wifi_task"
#endif

/*
 * Stack in bytes. The task runs the whole Wi-Fi init, and the main task has only 3584 bytes,
 * so this is larger. Chosen by design, not measured.
 */
#ifndef APP_WIFI_STACK_SIZE
#define APP_WIFI_STACK_SIZE 6144
#endif

/* Chosen by design, not measured. Change if the task starves or starves others. */
#ifndef APP_WIFI_TASK_PRIORITY
#define APP_WIFI_TASK_PRIORITY 3
#endif

/* How often the task prints the Wi-Fi status. */
#ifndef APP_WIFI_STATUS_PERIOD_MS
#define APP_WIFI_STATUS_PERIOD_MS 2000U
#endif

/*
 * Creates a task that starts Wi-Fi in APSTA mode with the DEFAULT_* values from app_config.h,
 * then prints the status to the console every APP_WIFI_STATUS_PERIOD_MS.
 * Returns 0 when the task was created, -1 when it could not be created.
 * Errors of the Wi-Fi start itself are printed by the task, and the task then ends.
 * Call it once.
 */
int wifi_app_start(void);

#endif /* WIFI_APP_H */