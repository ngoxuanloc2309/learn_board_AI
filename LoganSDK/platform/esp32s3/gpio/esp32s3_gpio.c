#include <stdbool.h>
#include <stdint.h>
#include "sx_gpio.h"
#include "driver/gpio.h"

static bool is_input_mode(sx_gpio_mode_t mode)
{
    return (mode == SX_GPIO_MODE_INPUT) ||
           (mode == SX_GPIO_MODE_INPUT_PULLUP) ||
           (mode == SX_GPIO_MODE_INPUT_PULLDOWN);
}

static bool is_valid_pin(const sx_gpio_pin_t *pin)
{
    if (pin == NULL) {
        return false;
    }
    int num = pin->pin;
    return GPIO_IS_VALID_GPIO(num);
}

int sx_gpio_init(const sx_gpio_pin_t *pin, sx_gpio_value_t initial_value)
{
    if (!is_valid_pin(pin)) {
        return SX_GPIO_ERR_PARAM;
    }
    if ((initial_value != SX_GPIO_LOW) && (initial_value != SX_GPIO_HIGH)) {
        return SX_GPIO_ERR_PARAM;
    }

    int num = pin->pin;
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << num,
        .intr_type    = GPIO_INTR_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    switch (pin->mode) {
    case SX_GPIO_MODE_INPUT:
        io.mode = GPIO_MODE_INPUT;
        break;
    case SX_GPIO_MODE_INPUT_PULLUP:
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_ENABLE;
        break;
    case SX_GPIO_MODE_INPUT_PULLDOWN:
        io.mode = GPIO_MODE_INPUT;
        io.pull_down_en = GPIO_PULLDOWN_ENABLE;
        break;
    case SX_GPIO_MODE_OUTPUT_PP:
        if (!GPIO_IS_VALID_OUTPUT_GPIO(num)) {
            return SX_GPIO_ERR_PARAM;
        }
        io.mode = GPIO_MODE_OUTPUT;
        /* Load the output latch first so the pin never glitches to the wrong level. */
        if (gpio_set_level((gpio_num_t)num, (uint32_t)initial_value) != ESP_OK) {
            return SX_GPIO_ERR_HW;
        }
        break;
    default:
        return SX_GPIO_ERR_PARAM;
    }

    if (gpio_config(&io) != ESP_OK) {
        return SX_GPIO_ERR_HW;
    }
    return SX_GPIO_OK;
}

int sx_gpio_read(const sx_gpio_pin_t *pin)
{
    if (!is_valid_pin(pin) || !is_input_mode(pin->mode)) {
        return SX_GPIO_ERR_PARAM;
    }
    return (gpio_get_level((gpio_num_t)pin->pin) != 0) ? SX_GPIO_HIGH : SX_GPIO_LOW;
}

int sx_gpio_write(const sx_gpio_pin_t *pin, sx_gpio_value_t value)
{
    if (!is_valid_pin(pin) || (pin->mode != SX_GPIO_MODE_OUTPUT_PP)) {
        return SX_GPIO_ERR_PARAM;
    }
    if ((value != SX_GPIO_LOW) && (value != SX_GPIO_HIGH)) {
        return SX_GPIO_ERR_PARAM;
    }
    return (gpio_set_level((gpio_num_t)pin->pin, (uint32_t)value) == ESP_OK) ? SX_GPIO_OK : SX_GPIO_ERR_HW;
}