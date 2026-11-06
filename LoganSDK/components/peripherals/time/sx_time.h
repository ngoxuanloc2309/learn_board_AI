#ifndef SX_SPI_H
#define SX_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

void sx_delay_ms(uint32_t ms);
void sx_get_time_ms(uint32_t *ms);


#ifdef __cplusplus
}
#endif

#endif /* SX_TIME_H */