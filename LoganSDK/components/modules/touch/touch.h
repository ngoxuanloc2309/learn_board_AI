#ifndef TOUCH_H
#define TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "sx_i2c.h"
#include "sx_gpio.h"

/*
 * Capacitive touch controller of the FocalTech FT6x36 and FT5x06 family (FT6336U on this board) on I2C.
 * Built only on the sx_i2c and sx_gpio contracts. The module only reads registers, it never writes to
 * the controller. It reads by polling: the interrupt line is not used, because its behavior is not
 * verified on this board. Call touch_read() periodically, for example from a UI input callback.
 *
 * The register map used here is the usual FocalTech one. It is NOT checked against the datasheet of the
 * controller on this board (see the unverified list in docs/HandOver.md).
 *
 * Not thread safe. The caller must serialise all calls (and any other user of the same I2C bus).
 */

/* Return codes: 0 on success, negative on failure. */
#define TOUCH_OK                0
#define TOUCH_ERR_PARAM        (-1)   /* NULL pointer or bad configuration */
#define TOUCH_ERR_STATE        (-2)   /* not initialised, or already initialised */
#define TOUCH_ERR_BUS          (-3)   /* the I2C layer reported an error */
#define TOUCH_ERR_NO_DEVICE    (-4)   /* nobody acknowledged the controller address */

/* Most points the module can report. The FT6336U reports at most two. */
#define TOUCH_MAX_POINTS        2U

/* Usual 7-bit address of the FT5x06 and FT6x36 family. NOT verified on this board. */
#define TOUCH_DEFAULT_ADDR      0x38U

/* Waits at least ms milliseconds. Provided by the board layer. */
typedef void (*touch_delay_fn_t)(uint32_t ms);

typedef struct touch_config {
    const sx_i2c_config_t *i2c;          /* bus, provided by the board layer. touch_init creates it, touch_deinit releases it */
    uint8_t                i2c_addr;     /* 7-bit address, 0 = TOUCH_DEFAULT_ADDR */
    uint32_t               timeout_ms;   /* limit of one I2C call, 0 = 20 ms */

    /*
     * Reset line, an output pin. NULL when the line is not driven by this module (no line, or shared with
     * the display and driven by it). touch_init waits boot_wait_ms before it talks to the controller in both cases.
     */
    const sx_gpio_pin_t   *reset;
    uint16_t               reset_low_ms;   /* time the line is held low, 0 = 10 ms */
    uint16_t               boot_wait_ms;   /* wait before the first access, 0 = 300 ms */

    /*
     * Size of the coordinate space reported to the caller, usually the display size.
     * Raw coordinates are first swapped (swap_xy), then limited to this size, then mirrored inside it.
     */
    uint16_t               width;
    uint16_t               height;
    bool                   swap_xy;
    bool                   mirror_x;
    bool                   mirror_y;

    touch_delay_fn_t       delay_ms;       /* required */
} touch_config_t;

typedef struct touch {
    const touch_config_t *config;
    uint8_t               addr;
    uint32_t              timeout_ms;
    bool                  ready;
} touch_t;

typedef struct touch_point {
    uint16_t x;
    uint16_t y;
    uint8_t  id;      /* finger number given by the controller */
} touch_point_t;

typedef struct touch_id {
    uint8_t chip_id;       /* register 0xA3 */
    uint8_t firmware_id;   /* register 0xA6 */
    uint8_t vendor_id;     /* register 0xA8 */
} touch_id_t;

/*
 * Creates the bus, resets the controller when a reset line is configured, waits for it to start and
 * checks that it acknowledges its address. The touch_t must be zero-initialised (for example a static
 * variable) before the first call. The config and everything it points to must stay valid and unchanged
 * while the module is in use. Blocks for several hundred milliseconds.
 */
int touch_init(touch_t *touch, const touch_config_t *config);

/* Releases the bus. */
int touch_deinit(touch_t *touch);

/*
 * Reads the current touches. On success *count is the number of points stored (0 when nobody touches),
 * at most max_points and at most TOUCH_MAX_POINTS. A frame that the controller marks as invalid is
 * reported as no touch. Blocks for the duration of one I2C transfer.
 */
int touch_read(touch_t *touch, touch_point_t *points, uint8_t max_points, uint8_t *count);

/* Reads the chip, firmware and vendor numbers. Useful at bring-up to confirm which controller answers. */
int touch_read_id(touch_t *touch, touch_id_t *id);

/* Recovers a stuck I2C bus (for example after a read error). */
int touch_recover(touch_t *touch);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_H */