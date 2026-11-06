#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "sx_time.h"

#define TICK_MS         (1000U / (uint32_t)configTICK_RATE_HZ)
#define BUSY_STEP_MS    1000U   /* longest single busy wait, keeps the microsecond argument small */

static void busy_wait_ms(uint32_t ms)
{
    while (ms > 0U) {
        uint32_t step = (ms > BUSY_STEP_MS) ? BUSY_STEP_MS : ms;

        esp_rom_delay_us(step * 1000U);
        ms -= step;
    }
}

void sx_delay_ms(uint32_t ms)
{
    if (ms == 0U) {
        return;
    }

    /*
     * A wait shorter than two ticks, and any wait before the scheduler runs, is a busy wait. It is exact and
     * works in start-up code. A longer wait gives the processor to other tasks.
     */
    if ((ms < (2U * TICK_MS)) || (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)) {
        busy_wait_ms(ms);
        return;
    }

    /*
     * vTaskDelay(n) returns after more than n-1 and at most n ticks, because the first tick comes early.
     * Using ms / tick + 2 ticks keeps the wait at least ms and less than two ticks longer.
     */
    vTaskDelay((TickType_t)((ms / TICK_MS) + 2U));
}

void sx_get_time_ms(uint32_t *ms)
{
    if (ms == NULL) {
        return;
    }
    /* The timer counts microseconds since start-up. The cast keeps the low 32 bits of the millisecond count. */
    *ms = (uint32_t)((uint64_t)esp_timer_get_time() / 1000U);
}