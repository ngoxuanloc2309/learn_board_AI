#ifndef SX_GPIO_H
#define SX_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Return codes: 0 or a positive level on success, negative on failure. */
#define SX_GPIO_OK           0
#define SX_GPIO_ERR_PARAM   (-1)   /* NULL pointer, invalid pin, or call not allowed for the pin mode */
#define SX_GPIO_ERR_HW      (-2)   /* the platform driver rejected the request */

typedef enum {
    SX_GPIO_LOW  = 0,
    SX_GPIO_HIGH = 1
} sx_gpio_value_t;

typedef enum {
    SX_GPIO_MODE_INPUT          = 0,
    SX_GPIO_MODE_INPUT_PULLUP   = 1,
    SX_GPIO_MODE_INPUT_PULLDOWN = 2,
    SX_GPIO_MODE_OUTPUT_PP      = 3
} sx_gpio_mode_t;

typedef struct sx_gpio_pin {
    int            pin;     /* GPIO number of the chip, taken from the board layer */
    sx_gpio_mode_t mode;
} sx_gpio_pin_t;

typedef void (*sx_gpio_isr_callback_t)(void *arg);

typedef enum {
    SX_GPIO_INTERRUPT_DISABLED = 0,
    SX_GPIO_INTERRUPT_POSEDGE,
    SX_GPIO_INTERRUPT_NEGEDGE,
    SX_GPIO_INTERRUPT_ANYEDGE,
    SX_GPIO_INTERRUPT_LOW_LEVEL,
    SX_GPIO_INTERRUPT_HIGH_LEVEL
} sx_gpio_interrupt_t;

/*
 * Configures the pin according to pin->mode.
 * initial_value is only applied when pin->mode is SX_GPIO_MODE_OUTPUT_PP and is
 * driven before the pin becomes an output, so the pin never glitches to the wrong level.
 * It is ignored for input modes (pass SX_GPIO_LOW).
 */
int sx_gpio_init(const sx_gpio_pin_t *pin, sx_gpio_value_t initial_value);

/* Returns SX_GPIO_LOW or SX_GPIO_HIGH, or a negative error code. Input modes only. */
int sx_gpio_read(const sx_gpio_pin_t *pin);

/* Output mode only. Returns SX_GPIO_OK or a negative error code. */
int sx_gpio_write(const sx_gpio_pin_t *pin, sx_gpio_value_t value);

int sx_gpio_set_interrupt(const sx_gpio_pin_t *pin, sx_gpio_interrupt_t type,
                          sx_gpio_isr_callback_t callback, void *arg);
int sx_gpio_remove_interrupt(const sx_gpio_pin_t *pin);

#ifdef __cplusplus
}
#endif

#endif /* SX_GPIO_H */