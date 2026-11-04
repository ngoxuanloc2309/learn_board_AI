#ifndef SX_UART_H
#define SX_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define SX_UART_OK           0
#define SX_UART_ERR_PARAM   (-1)
#define SX_UART_ERR_STATE   (-2)
#define SX_UART_ERR_BUS     (-3)
#define SX_UART_ERR_TIMEOUT (-4)

typedef enum {
    SX_UART_MODE_NORMAL = 0,
    SX_UART_MODE_RS485_HALF_DUPLEX = 1
} sx_uart_mode_t;

typedef struct {
    int port;
    int tx_pin;
    int rx_pin;
    int rts_pin;
    uint32_t baud_rate;
    uint8_t data_bits;
    uint8_t parity;
    uint8_t stop_bits;
    sx_uart_mode_t mode;
    uint32_t rx_buffer_size;
    uint32_t tx_buffer_size;
} sx_uart_config_t;

typedef struct {
    const sx_uart_config_t *config;
    uint8_t ready;
} sx_uart_t;

int sx_uart_init(sx_uart_t *uart, const sx_uart_config_t *config);
int sx_uart_write(sx_uart_t *uart, const uint8_t *data, size_t length, uint32_t timeout_ms);
int sx_uart_read(sx_uart_t *uart, uint8_t *data, size_t length, size_t *received,
                 uint32_t timeout_ms);
int sx_uart_flush(sx_uart_t *uart);
int sx_uart_deinit(sx_uart_t *uart);

#ifdef __cplusplus
}
#endif

#endif
