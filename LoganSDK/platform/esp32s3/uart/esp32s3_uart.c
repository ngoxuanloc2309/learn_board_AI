#include <stdbool.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "sx_uart.h"

static int map_error(esp_err_t error)
{
    if (error == ESP_OK) return SX_UART_OK;
    if (error == ESP_ERR_INVALID_ARG) return SX_UART_ERR_PARAM;
    if (error == ESP_ERR_TIMEOUT) return SX_UART_ERR_TIMEOUT;
    return SX_UART_ERR_BUS;
}

static bool valid_config(const sx_uart_config_t *config)
{
    return config != NULL && config->port >= 0 && config->port < UART_NUM_MAX &&
           config->tx_pin >= 0 && config->rx_pin >= 0 && config->baud_rate > 0U &&
           config->data_bits >= 5U && config->data_bits <= 8U &&
           config->stop_bits >= 1U && config->stop_bits <= 2U &&
           config->mode <= SX_UART_MODE_RS485_HALF_DUPLEX;
}

static TickType_t timeout_ticks(uint32_t timeout_ms)
{
    return timeout_ms == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
}

int sx_uart_init(sx_uart_t *uart, const sx_uart_config_t *config)
{
    if (uart == NULL || !valid_config(config) || uart->ready) {
        return uart != NULL && uart->ready ? SX_UART_ERR_STATE : SX_UART_ERR_PARAM;
    }
    uart_config_t driver_config = {
        .baud_rate = (int)config->baud_rate,
        .data_bits = (uart_word_length_t)(config->data_bits - 5U),
        .parity = (uart_parity_t)config->parity,
        .stop_bits = (uart_stop_bits_t)(config->stop_bits - 1U),
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };
    int ret = map_error(uart_param_config((uart_port_t)config->port, &driver_config));
    if (ret != SX_UART_OK) return ret;
    ret = map_error(uart_set_pin((uart_port_t)config->port, config->tx_pin, config->rx_pin,
                                 config->rts_pin, UART_PIN_NO_CHANGE));
    if (ret != SX_UART_OK) return ret;
    if (config->mode == SX_UART_MODE_RS485_HALF_DUPLEX) {
        ret = map_error(uart_set_mode((uart_port_t)config->port, UART_MODE_RS485_HALF_DUPLEX));
        if (ret != SX_UART_OK) return ret;
    }
    ret = map_error(uart_driver_install((uart_port_t)config->port, config->rx_buffer_size,
                                        config->tx_buffer_size, 0, NULL, 0));
    if (ret == SX_UART_OK) {
        uart->config = config;
        uart->ready = 1U;
    }
    return ret;
}

int sx_uart_write(sx_uart_t *uart, const uint8_t *data, size_t length, uint32_t timeout_ms)
{
    if (uart == NULL || !uart->ready || (data == NULL && length != 0U)) return SX_UART_ERR_PARAM;
    int written = uart_write_bytes((uart_port_t)uart->config->port, (const char *)data, length);
    if (written < 0) return SX_UART_ERR_BUS;
    if (written != (int)length) return SX_UART_ERR_TIMEOUT;
    return map_error(uart_wait_tx_done((uart_port_t)uart->config->port, timeout_ticks(timeout_ms)));
}

int sx_uart_read(sx_uart_t *uart, uint8_t *data, size_t length, size_t *received,
                 uint32_t timeout_ms)
{
    if (uart == NULL || !uart->ready || received == NULL || (data == NULL && length != 0U)) {
        return SX_UART_ERR_PARAM;
    }
    int count = uart_read_bytes((uart_port_t)uart->config->port, data, length, timeout_ticks(timeout_ms));
    if (count < 0) return SX_UART_ERR_BUS;
    *received = (size_t)count;
    return count == 0 && length != 0U ? SX_UART_ERR_TIMEOUT : SX_UART_OK;
}

int sx_uart_flush(sx_uart_t *uart)
{
    if (uart == NULL || !uart->ready) return SX_UART_ERR_PARAM;
    return map_error(uart_flush_input((uart_port_t)uart->config->port));
}

int sx_uart_deinit(sx_uart_t *uart)
{
    if (uart == NULL) return SX_UART_ERR_PARAM;
    if (!uart->ready) return SX_UART_OK;
    int ret = map_error(uart_driver_delete((uart_port_t)uart->config->port));
    if (ret == SX_UART_OK) {
        uart->config = NULL;
        uart->ready = 0U;
    }
    return ret;
}
