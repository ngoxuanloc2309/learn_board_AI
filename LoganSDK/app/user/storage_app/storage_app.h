#ifndef STORAGE_APP_H
#define STORAGE_APP_H

#ifndef APP_STORAGE_TASK_NAME
#define APP_STORAGE_TASK_NAME "app_storage_task"
#endif

#ifndef APP_STORAGE_STACK_SIZE
#define APP_STORAGE_STACK_SIZE 4096
#endif

#ifndef APP_STORAGE_TASK_PRIORITY
#define APP_STORAGE_TASK_PRIORITY 4 //Can change if needed, because I don't know I need to set its value
#endif

void app_storage_init(void);

/*Other functions*/

#endif