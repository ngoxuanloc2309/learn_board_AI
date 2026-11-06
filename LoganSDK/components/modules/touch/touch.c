#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "touch.h"

/* FocalTech register map (usual layout, not checked against the datasheet of this board). */
#define REG_TD_STATUS       0x02U   /* low nibble: number of touch points */
#define REG_POINT_FIRST     0x03U   /* first point, each point uses POINT_STRIDE bytes */
#define REG_CHIP_ID         0xA3U
#define REG_FIRMWARE_ID     0xA6U
#define REG_VENDOR_ID       0xA8U

#define POINT_STRIDE        6U      /* XH, XL, YH, YL, weight, misc */
#define FRAME_LEN           (1U + (POINT_STRIDE * TOUCH_MAX_POINTS))

#define EVENT_PUT_UP        1U      /* the finger was lifted, the point is no longer touching */
#define EVENT_NONE          3U

#define DEFAULT_TIMEOUT_MS      20U
#define DEFAULT_RESET_LOW_MS    10U
#define DEFAULT_BOOT_WAIT_MS    300U

static int map_i2c(int ret)
{
    switch (ret) {
    case SX_I2C_OK:         return TOUCH_OK;
    case SX_I2C_ERR_PARAM:  return TOUCH_ERR_PARAM;
    case SX_I2C_ERR_NACK:   return TOUCH_ERR_NO_DEVICE;
    default:                return TOUCH_ERR_BUS;
    }
}

static int read_reg(touch_t *touch, uint8_t reg, uint8_t *data, uint16_t len)
{
    return map_i2c(sx_i2c_mem_read(touch->config->i2c, touch->addr, reg, 1U, data, len, touch->timeout_ms));
}

static int reset_controller(const touch_config_t *cfg)
{
    uint16_t low_ms = (cfg->reset_low_ms != 0U) ? cfg->reset_low_ms : (uint16_t)DEFAULT_RESET_LOW_MS;

    if (sx_gpio_init(cfg->reset, SX_GPIO_HIGH) != SX_GPIO_OK) {
        return TOUCH_ERR_BUS;
    }
    if (sx_gpio_write(cfg->reset, SX_GPIO_LOW) != SX_GPIO_OK) {
        return TOUCH_ERR_BUS;
    }
    cfg->delay_ms(low_ms);
    if (sx_gpio_write(cfg->reset, SX_GPIO_HIGH) != SX_GPIO_OK) {
        return TOUCH_ERR_BUS;
    }
    return TOUCH_OK;
}

static void transform(const touch_config_t *cfg, uint16_t raw_x, uint16_t raw_y, uint16_t *x, uint16_t *y)
{
    uint32_t px = raw_x;
    uint32_t py = raw_y;
    uint32_t max_x = (uint32_t)cfg->width - 1U;
    uint32_t max_y = (uint32_t)cfg->height - 1U;

    if (cfg->swap_xy) {
        uint32_t tmp = px;
        px = py;
        py = tmp;
    }
    if (px > max_x) {
        px = max_x;
    }
    if (py > max_y) {
        py = max_y;
    }
    if (cfg->mirror_x) {
        px = max_x - px;
    }
    if (cfg->mirror_y) {
        py = max_y - py;
    }
    *x = (uint16_t)px;
    *y = (uint16_t)py;
}

int touch_init(touch_t *touch, const touch_config_t *config)
{
    int ret;

    if ((touch == NULL) || (config == NULL) || (config->i2c == NULL) || (config->delay_ms == NULL) ||
        (config->width == 0U) || (config->height == 0U)) {
        return TOUCH_ERR_PARAM;
    }
    if ((config->i2c_addr > 0x7FU) ||
        ((config->reset != NULL) && (config->reset->mode != SX_GPIO_MODE_OUTPUT_PP))) {
        return TOUCH_ERR_PARAM;
    }
    if (touch->ready) {
        return TOUCH_ERR_STATE;
    }

    touch->config = config;
    touch->addr = (config->i2c_addr != 0U) ? config->i2c_addr : (uint8_t)TOUCH_DEFAULT_ADDR;
    touch->timeout_ms = (config->timeout_ms != 0U) ? config->timeout_ms : DEFAULT_TIMEOUT_MS;

    ret = map_i2c(sx_i2c_init(config->i2c));
    if (ret != TOUCH_OK) {
        return ret;
    }

    if (config->reset != NULL) {
        ret = reset_controller(config);
        if (ret != TOUCH_OK) {
            (void)sx_i2c_deinit(config->i2c);
            return ret;
        }
    }
    config->delay_ms((config->boot_wait_ms != 0U) ? config->boot_wait_ms : (uint16_t)DEFAULT_BOOT_WAIT_MS);

    ret = map_i2c(sx_i2c_probe(config->i2c, touch->addr, touch->timeout_ms));
    if (ret != TOUCH_OK) {
        (void)sx_i2c_deinit(config->i2c);
        return ret;
    }

    touch->ready = true;
    return TOUCH_OK;
}

int touch_deinit(touch_t *touch)
{
    if ((touch == NULL) || !touch->ready) {
        return TOUCH_ERR_STATE;
    }
    (void)sx_i2c_deinit(touch->config->i2c);
    touch->ready = false;
    return TOUCH_OK;
}

int touch_read(touch_t *touch, touch_point_t *points, uint8_t max_points, uint8_t *count)
{
    uint8_t frame[FRAME_LEN];
    uint8_t found;
    uint8_t out = 0U;
    uint8_t i;
    int ret;

    if ((touch == NULL) || !touch->ready) {
        return TOUCH_ERR_STATE;
    }
    if ((points == NULL) || (count == NULL)) {
        return TOUCH_ERR_PARAM;
    }

    *count = 0U;
    ret = read_reg(touch, REG_TD_STATUS, frame, (uint16_t)FRAME_LEN);
    if (ret != TOUCH_OK) {
        return ret;
    }

    found = frame[0] & 0x0FU;
    if (found > TOUCH_MAX_POINTS) {
        return TOUCH_OK;   /* invalid frame, for example all ones while the controller is busy */
    }

    for (i = 0U; (i < found) && (out < max_points); i++) {
        const uint8_t *p = &frame[(REG_POINT_FIRST - REG_TD_STATUS) + ((uint32_t)i * POINT_STRIDE)];
        uint8_t event = (uint8_t)(p[0] >> 6);
        uint16_t raw_x;
        uint16_t raw_y;

        if ((event == EVENT_PUT_UP) || (event == EVENT_NONE)) {
            continue;
        }
        raw_x = (uint16_t)((((uint16_t)p[0] & 0x0FU) << 8) | p[1]);
        raw_y = (uint16_t)((((uint16_t)p[2] & 0x0FU) << 8) | p[3]);
        transform(touch->config, raw_x, raw_y, &points[out].x, &points[out].y);
        points[out].id = (uint8_t)(p[2] >> 4);
        out++;
    }

    *count = out;
    return TOUCH_OK;
}

int touch_read_id(touch_t *touch, touch_id_t *id)
{
    int ret;

    if ((touch == NULL) || !touch->ready) {
        return TOUCH_ERR_STATE;
    }
    if (id == NULL) {
        return TOUCH_ERR_PARAM;
    }

    ret = read_reg(touch, REG_CHIP_ID, &id->chip_id, 1U);
    if (ret != TOUCH_OK) {
        return ret;
    }
    ret = read_reg(touch, REG_FIRMWARE_ID, &id->firmware_id, 1U);
    if (ret != TOUCH_OK) {
        return ret;
    }
    return read_reg(touch, REG_VENDOR_ID, &id->vendor_id, 1U);
}

int touch_recover(touch_t *touch)
{
    if ((touch == NULL) || !touch->ready) {
        return TOUCH_ERR_STATE;
    }
    return map_i2c(sx_i2c_reset(touch->config->i2c));
}