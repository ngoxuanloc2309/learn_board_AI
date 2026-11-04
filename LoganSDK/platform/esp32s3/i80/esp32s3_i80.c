#include <stdbool.h>
#include <stdint.h>
#include "sx_i80.h"
#include "esp_err.h"
#include "esp_lcd_io_i80.h"
#include "esp_lcd_panel_io.h"

#define I80_CMD_BITS     8
#define I80_PARAM_BITS   8

static int map_err(esp_err_t err)
{
    switch (err) {
    case ESP_OK:              return SX_I80_OK;
    case ESP_ERR_INVALID_ARG: return SX_I80_ERR_PARAM;
    case ESP_ERR_NO_MEM:      return SX_I80_ERR_NOMEM;
    default:                  return SX_I80_ERR_BUS;
    }
}

/* Runs in interrupt context when a color transfer has finished. */
static bool on_color_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)io;
    (void)edata;
    const sx_i80_t *i80 = (const sx_i80_t *)user_ctx;
    if ((i80 != NULL) && (i80->config != NULL) && (i80->config->on_color_done != NULL)) {
        return i80->config->on_color_done(i80->config->user_ctx);
    }
    return false;
}

int sx_i80_init(sx_i80_t *i80, const sx_i80_config_t *config)
{
    if ((i80 == NULL) || (config == NULL)) {
        return SX_I80_ERR_PARAM;
    }
    if (i80->ready) {
        return SX_I80_ERR_BUS;
    }
    if (((config->bus_width != 8U) && (config->bus_width != 16U)) ||
        (config->dc_pin < 0) || (config->wr_pin < 0) ||
        (config->pclk_hz == 0U) || (config->max_transfer_bytes == 0U) || (config->queue_depth < 1U)) {
        return SX_I80_ERR_PARAM;
    }
    for (uint8_t i = 0; i < config->bus_width; i++) {
        if (config->data_pins[i] < 0) {
            return SX_I80_ERR_PARAM;
        }
    }

    esp_lcd_i80_bus_config_t bus_cfg = {
        .dc_gpio_num        = config->dc_pin,
        .wr_gpio_num        = config->wr_pin,
        .clk_src            = LCD_CLK_SRC_DEFAULT,
        .bus_width          = config->bus_width,
        .max_transfer_bytes = config->max_transfer_bytes,
    };
    for (int i = 0; i < ESP_LCD_I80_BUS_WIDTH_MAX; i++) {
        bus_cfg.data_gpio_nums[i] = (i < (int)config->bus_width) ? config->data_pins[i] : -1;
    }

    esp_lcd_i80_bus_handle_t bus = NULL;
    int ret = map_err(esp_lcd_new_i80_bus(&bus_cfg, &bus));
    if (ret != SX_I80_OK) {
        return ret;
    }

    esp_lcd_panel_io_i80_config_t io_cfg = {
        .cs_gpio_num         = config->cs_pin,
        .pclk_hz             = config->pclk_hz,
        .trans_queue_depth   = config->queue_depth,
        .on_color_trans_done = on_color_trans_done,
        .user_ctx            = i80,
        .lcd_cmd_bits        = I80_CMD_BITS,
        .lcd_param_bits      = I80_PARAM_BITS,
        .dc_levels = {
            .dc_idle_level  = 0,
            .dc_cmd_level   = 0,
            .dc_dummy_level = 0,
            .dc_data_level  = 1,
        },
        .flags = {
            .swap_color_bytes = config->swap_color_bytes ? 1U : 0U,
        },
    };

    esp_lcd_panel_io_handle_t io = NULL;
    ret = map_err(esp_lcd_new_panel_io_i80(bus, &io_cfg, &io));
    if (ret != SX_I80_OK) {
        esp_lcd_del_i80_bus(bus);
        return ret;
    }

    i80->config = config;
    i80->bus    = bus;
    i80->io     = io;
    i80->ready  = true;
    return SX_I80_OK;
}

int sx_i80_deinit(sx_i80_t *i80)
{
    if (i80 == NULL) {
        return SX_I80_ERR_PARAM;
    }
    if (!i80->ready) {
        return SX_I80_OK;
    }

    int ret = map_err(esp_lcd_panel_io_del((esp_lcd_panel_io_handle_t)i80->io));
    if (ret != SX_I80_OK) {
        return ret;
    }
    ret = map_err(esp_lcd_del_i80_bus((esp_lcd_i80_bus_handle_t)i80->bus));
    if (ret != SX_I80_OK) {
        return ret;
    }

    i80->io    = NULL;
    i80->bus   = NULL;
    i80->ready = false;
    return SX_I80_OK;
}

int sx_i80_write_cmd(sx_i80_t *i80, uint8_t cmd, const void *param, size_t len)
{
    if ((i80 == NULL) || !i80->ready || ((param == NULL) && (len != 0U))) {
        return SX_I80_ERR_PARAM;
    }
    return map_err(esp_lcd_panel_io_tx_param((esp_lcd_panel_io_handle_t)i80->io, cmd, param, len));
}

int sx_i80_write_color(sx_i80_t *i80, uint8_t cmd, const void *data, size_t len)
{
    if ((i80 == NULL) || !i80->ready || (data == NULL) || (len == 0U) ||
        (len > i80->config->max_transfer_bytes)) {
        return SX_I80_ERR_PARAM;
    }
    return map_err(esp_lcd_panel_io_tx_color((esp_lcd_panel_io_handle_t)i80->io, cmd, data, len));
}