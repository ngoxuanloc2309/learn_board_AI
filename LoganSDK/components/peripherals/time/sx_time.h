#ifndef SX_TIME_H
#define SX_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*
 * Time contract: a blocking delay and a millisecond counter that starts at zero when the firmware starts.
 * Both functions may be called from any task. Do not call them from an interrupt service routine.
 */

/*
 * Blocks the calling task for at least ms milliseconds and returns at once when ms is 0.
 * The wait can be longer than ms, by less than two scheduler ticks on the ESP32-S3 platform.
 * Short waits are exact but keep the processor busy, so use long waits for slow operations.
 */
void sx_delay_ms(uint32_t ms);

/*
 * Stores the milliseconds elapsed since start-up in *ms. The value wraps to zero after about 49.7 days,
 * so measure an interval by subtracting two readings as unsigned numbers. Does nothing when ms is NULL.
 */
void sx_get_time_ms(uint32_t *ms);

#ifdef __cplusplus
}
#endif

#endif /* SX_TIME_H */