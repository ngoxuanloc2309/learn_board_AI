#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "screenHMI.h"

/* Standard MIPI DCS commands. */
#define CMD_SWRESET        0x01U
#define CMD_SLPOUT         0x11U
#define CMD_INVOFF         0x20U
#define CMD_INVON          0x21U
#define CMD_DISPOFF        0x28U
#define CMD_DISPON         0x29U
#define CMD_CASET          0x2AU
#define CMD_RASET          0x2BU
#define CMD_RAMWR          0x2CU
#define CMD_MADCTL         0x36U
#define CMD_COLMOD         0x3AU

#define COLMOD_RGB565      0x55U   /* 16 bits per pixel on both the RGB and the MCU interface */
#define BYTES_PER_PIXEL    2U

/*
 * Waits below are conservative values from common practice. They are NOT checked against the datasheet
 * of the panel used on this board (see the unverified list in docs/HandOver.md).
 */
#define DEFAULT_RESET_LOW_MS    10U
#define DEFAULT_RESET_WAIT_MS   120U
#define SWRESET_WAIT_MS         120U
#define SLPOUT_WAIT_MS          120U
#define DISPON_WAIT_MS          20U
#define FILL_TIMEOUT_MS         200U   /* longest wait for one fill chunk, polled in 1 ms steps */

static int map_i80(int ret)
{
    switch (ret) {
    case SX_I80_OK:         return SCREEN_HMI_OK;
    case SX_I80_ERR_PARAM:  return SCREEN_HMI_ERR_PARAM;
    default:                return SCREEN_HMI_ERR_BUS;
    }
}

static int send_cmd(screen_hmi_t *hmi, uint8_t cmd, const uint8_t *param, size_t len)
{
    return map_i80(sx_i80_write_cmd(&hmi->i80, cmd, param, len));
}

static int set_window(screen_hmi_t *hmi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t col[4];
    uint8_t row[4];
    int ret;

    col[0] = (uint8_t)(x0 >> 8);
    col[1] = (uint8_t)(x0 & 0xFFU);
    col[2] = (uint8_t)(x1 >> 8);
    col[3] = (uint8_t)(x1 & 0xFFU);
    row[0] = (uint8_t)(y0 >> 8);
    row[1] = (uint8_t)(y0 & 0xFFU);
    row[2] = (uint8_t)(y1 >> 8);
    row[3] = (uint8_t)(y1 & 0xFFU);

    ret = send_cmd(hmi, CMD_CASET, col, sizeof(col));
    if (ret != SCREEN_HMI_OK) {
        return ret;
    }
    return send_cmd(hmi, CMD_RASET, row, sizeof(row));
}

static bool area_is_valid(const screen_hmi_t *hmi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    return (x0 <= x1) && (y0 <= y1) && (x1 < hmi->config->width) && (y1 < hmi->config->height);
}

/* Runs in interrupt context. Forwards the completion to the user, unless a blocking fill is waiting for it. */
static bool on_color_done(void *ctx)
{
    screen_hmi_t *hmi = (screen_hmi_t *)ctx;

    if (hmi->fill_active) {
        hmi->fill_done = true;
        return false;
    }
    if (hmi->config->on_flush_done != NULL) {
        return hmi->config->on_flush_done(hmi->config->user_ctx);
    }
    return false;
}

static void release_resources(screen_hmi_t *hmi)
{
    (void)sx_i80_deinit(&hmi->i80);
    if (hmi->backlight_ready) {
        (void)sx_pwm_deinit(&hmi->pwm);
        hmi->backlight_ready = false;
    }
}

static int hardware_reset(screen_hmi_t *hmi)
{
    const screen_hmi_config_t *cfg = hmi->config;
    uint16_t low_ms = (cfg->reset_low_ms != 0U) ? cfg->reset_low_ms : (uint16_t)DEFAULT_RESET_LOW_MS;
    uint16_t wait_ms = (cfg->reset_wait_ms != 0U) ? cfg->reset_wait_ms : (uint16_t)DEFAULT_RESET_WAIT_MS;

    if (sx_gpio_init(cfg->reset, SX_GPIO_HIGH) != SX_GPIO_OK) {
        return SCREEN_HMI_ERR_HW;
    }
    if (sx_gpio_write(cfg->reset, SX_GPIO_LOW) != SX_GPIO_OK) {
        return SCREEN_HMI_ERR_HW;
    }
    cfg->delay_ms(low_ms);
    if (sx_gpio_write(cfg->reset, SX_GPIO_HIGH) != SX_GPIO_OK) {
        return SCREEN_HMI_ERR_HW;
    }
    cfg->delay_ms(wait_ms);
    return SCREEN_HMI_OK;
}

static int run_init_sequence(screen_hmi_t *hmi)
{
    const screen_hmi_config_t *cfg = hmi->config;
    uint8_t value;
    size_t i;
    int ret;

    ret = send_cmd(hmi, CMD_SWRESET, NULL, 0U);
    if (ret != SCREEN_HMI_OK) {
        return ret;
    }
    cfg->delay_ms(SWRESET_WAIT_MS);

    ret = send_cmd(hmi, CMD_SLPOUT, NULL, 0U);
    if (ret != SCREEN_HMI_OK) {
        return ret;
    }
    cfg->delay_ms(SLPOUT_WAIT_MS);

    for (i = 0U; i < cfg->init_cmd_count; i++) {
        const screen_hmi_init_cmd_t *item = &cfg->init_cmds[i];

        ret = send_cmd(hmi, item->cmd, item->param, item->param_len);
        if (ret != SCREEN_HMI_OK) {
            return ret;
        }
        if (item->delay_ms != 0U) {
            cfg->delay_ms(item->delay_ms);
        }
    }

    value = COLMOD_RGB565;
    ret = send_cmd(hmi, CMD_COLMOD, &value, 1U);
    if (ret != SCREEN_HMI_OK) {
        return ret;
    }

    value = cfg->madctl;
    ret = send_cmd(hmi, CMD_MADCTL, &value, 1U);
    if (ret != SCREEN_HMI_OK) {
        return ret;
    }

    ret = send_cmd(hmi, cfg->invert ? CMD_INVON : CMD_INVOFF, NULL, 0U);
    if (ret != SCREEN_HMI_OK) {
        return ret;
    }

    ret = send_cmd(hmi, CMD_DISPON, NULL, 0U);
    if (ret != SCREEN_HMI_OK) {
        return ret;
    }
    cfg->delay_ms(DISPON_WAIT_MS);
    return SCREEN_HMI_OK;
}

int screen_hmi_init(screen_hmi_t *hmi, const screen_hmi_config_t *config)
{
    size_t i;
    int ret;

    if ((hmi == NULL) || (config == NULL) || (config->bus == NULL) || (config->delay_ms == NULL) ||
        (config->width == 0U) || (config->height == 0U)) {
        return SCREEN_HMI_ERR_PARAM;
    }
    if ((config->init_cmds == NULL) != (config->init_cmd_count == 0U)) {
        return SCREEN_HMI_ERR_PARAM;
    }
    for (i = 0U; i < config->init_cmd_count; i++) {
        if ((config->init_cmds[i].param_len != 0U) && (config->init_cmds[i].param == NULL)) {
            return SCREEN_HMI_ERR_PARAM;
        }
    }
    if ((config->reset != NULL) && (config->reset->mode != SX_GPIO_MODE_OUTPUT_PP)) {
        return SCREEN_HMI_ERR_PARAM;
    }
    if (hmi->ready) {
        return SCREEN_HMI_ERR_STATE;
    }

    hmi->config = config;
    hmi->bus_config = *config->bus;
    hmi->bus_config.on_color_done = on_color_done;
    hmi->bus_config.user_ctx = hmi;
    hmi->fill_active = false;
    hmi->fill_done = false;
    hmi->backlight_ready = false;

    if (config->backlight != NULL) {
        if (sx_pwm_init(&hmi->pwm, config->backlight) != SX_PWM_OK) {
            return SCREEN_HMI_ERR_HW;
        }
        hmi->backlight_ready = true;
    }

    ret = map_i80(sx_i80_init(&hmi->i80, &hmi->bus_config));
    if (ret != SCREEN_HMI_OK) {
        if (hmi->backlight_ready) {
            (void)sx_pwm_deinit(&hmi->pwm);
            hmi->backlight_ready = false;
        }
        return ret;
    }

    ret = SCREEN_HMI_OK;
    if (config->reset != NULL) {
        ret = hardware_reset(hmi);
    }
    if (ret == SCREEN_HMI_OK) {
        ret = run_init_sequence(hmi);
    }
    if (ret != SCREEN_HMI_OK) {
        release_resources(hmi);
        return ret;
    }

    hmi->ready = true;
    return SCREEN_HMI_OK;
}

int screen_hmi_deinit(screen_hmi_t *hmi)
{
    if ((hmi == NULL) || !hmi->ready) {
        return SCREEN_HMI_ERR_STATE;
    }

    if (hmi->backlight_ready) {
        (void)screen_hmi_set_backlight(hmi, 0U);
    }
    (void)send_cmd(hmi, CMD_DISPOFF, NULL, 0U);
    release_resources(hmi);
    hmi->ready = false;
    return SCREEN_HMI_OK;
}

int screen_hmi_set_display_on(screen_hmi_t *hmi, bool on)
{
    if ((hmi == NULL) || !hmi->ready) {
        return SCREEN_HMI_ERR_STATE;
    }
    return send_cmd(hmi, on ? CMD_DISPON : CMD_DISPOFF, NULL, 0U);
}

int screen_hmi_set_backlight(screen_hmi_t *hmi, uint8_t percent)
{
    uint32_t duty;

    if ((hmi == NULL) || !hmi->ready || !hmi->backlight_ready) {
        return SCREEN_HMI_ERR_STATE;
    }
    if (percent > 100U) {
        return SCREEN_HMI_ERR_PARAM;
    }

    duty = (uint32_t)(((uint64_t)hmi->pwm.max_duty * percent) / 100U);
    if (hmi->config->backlight_active_low) {
        duty = hmi->pwm.max_duty - duty;
    }
    return (sx_pwm_set_duty(&hmi->pwm, duty) == SX_PWM_OK) ? SCREEN_HMI_OK : SCREEN_HMI_ERR_HW;
}

int screen_hmi_draw_bitmap(screen_hmi_t *hmi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           const void *pixels, size_t len)
{
    size_t expected;
    int ret;

    if ((hmi == NULL) || !hmi->ready) {
        return SCREEN_HMI_ERR_STATE;
    }
    if ((pixels == NULL) || !area_is_valid(hmi, x0, y0, x1, y1)) {
        return SCREEN_HMI_ERR_PARAM;
    }
    expected = (size_t)(x1 - x0 + 1U) * (size_t)(y1 - y0 + 1U) * BYTES_PER_PIXEL;
    if ((len != expected) || (len > hmi->bus_config.max_transfer_bytes)) {
        return SCREEN_HMI_ERR_PARAM;
    }

    ret = set_window(hmi, x0, y0, x1, y1);
    if (ret != SCREEN_HMI_OK) {
        return ret;
    }
    return map_i80(sx_i80_write_color(&hmi->i80, CMD_RAMWR, pixels, len));
}

static int wait_fill_done(screen_hmi_t *hmi)
{
    uint32_t waited;

    for (waited = 0U; waited < FILL_TIMEOUT_MS; waited++) {
        if (hmi->fill_done) {
            return SCREEN_HMI_OK;
        }
        hmi->config->delay_ms(1U);
    }
    return hmi->fill_done ? SCREEN_HMI_OK : SCREEN_HMI_ERR_TIMEOUT;
}

int screen_hmi_fill_rect(screen_hmi_t *hmi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    uint32_t width;
    uint32_t total_rows;
    uint32_t rows_per_chunk;
    uint32_t max_rows_by_bus;
    uint32_t row;
    uint32_t i;
    int ret = SCREEN_HMI_OK;

    if ((hmi == NULL) || !hmi->ready) {
        return SCREEN_HMI_ERR_STATE;
    }
    if (!area_is_valid(hmi, x0, y0, x1, y1)) {
        return SCREEN_HMI_ERR_PARAM;
    }

    width = (uint32_t)x1 - x0 + 1U;
    total_rows = (uint32_t)y1 - y0 + 1U;
    if (width > SCREEN_HMI_FILL_BUF_PIXELS) {
        return SCREEN_HMI_ERR_PARAM;
    }

    max_rows_by_bus = hmi->bus_config.max_transfer_bytes / (width * BYTES_PER_PIXEL);
    rows_per_chunk = SCREEN_HMI_FILL_BUF_PIXELS / width;
    if (rows_per_chunk > max_rows_by_bus) {
        rows_per_chunk = max_rows_by_bus;
    }
    if (rows_per_chunk == 0U) {
        return SCREEN_HMI_ERR_PARAM;
    }
    if (rows_per_chunk > total_rows) {
        rows_per_chunk = total_rows;
    }

    for (i = 0U; i < (rows_per_chunk * width); i++) {
        hmi->fill_buf[i] = color;
    }

    hmi->fill_active = true;
    for (row = y0; row <= y1; row += rows_per_chunk) {
        uint32_t rows = (uint32_t)y1 - row + 1U;

        if (rows > rows_per_chunk) {
            rows = rows_per_chunk;
        }
        ret = set_window(hmi, x0, (uint16_t)row, x1, (uint16_t)(row + rows - 1U));
        if (ret != SCREEN_HMI_OK) {
            break;
        }
        hmi->fill_done = false;
        ret = map_i80(sx_i80_write_color(&hmi->i80, CMD_RAMWR, hmi->fill_buf, (size_t)(width * rows * BYTES_PER_PIXEL)));
        if (ret != SCREEN_HMI_OK) {
            break;
        }
        ret = wait_fill_done(hmi);
        if (ret != SCREEN_HMI_OK) {
            break;
        }
    }
    hmi->fill_active = false;
    return ret;
}