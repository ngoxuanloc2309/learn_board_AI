#include "board.h"

#include <stddef.h>

#include "sx_gpio.h"
#include "sx_i2c.h"
#include "sx_i80.h"
#include "sx_pwm.h"
#include "sx_spi.h"
#include "sx_time.h"
#include "sdcard.h"
#include "storage.h"

/*
 * Instances of the devices. They are static, so they start zero-initialised as every module requires,
 * and they never move. Only this file touches them directly.
 */
static screen_hmi_t s_screen;
static touch_t      s_touch;
static speaker_t    s_speaker;
static inmp441_t    s_mic;
static sdcard_t     s_sdcard;

/* State flags. A device flag is true only after its init call succeeded. */
static bool s_board_ready;
static bool s_screen_ready;
static bool s_touch_ready;
static bool s_speaker_ready;
static bool s_mic_ready;
static bool s_storage_ready;

/* LCD bus. Entries of data_pins after bus_width are not read by the platform. */
static const sx_i80_config_t s_lcd_bus = {
    .dc_pin             = BOARD_PIN_LCD_RS,
    .wr_pin             = BOARD_PIN_LCD_WR,
    .cs_pin             = -1,
    .data_pins          = {
        BOARD_PIN_LCD_DB0, BOARD_PIN_LCD_DB1, BOARD_PIN_LCD_DB2, BOARD_PIN_LCD_DB3,
        BOARD_PIN_LCD_DB4, BOARD_PIN_LCD_DB5, BOARD_PIN_LCD_DB6, BOARD_PIN_LCD_DB7
    },
    .bus_width          = 8U,
    .pclk_hz            = BOARD_LCD_PCLK_HZ,
    .max_transfer_bytes = BOARD_LCD_MAX_TRANSFER,
    .queue_depth        = 1U,
    /* RGB565 values are held in native uint16_t, the panel wants the high byte first. DESIGN. */
    .swap_color_bytes   = true,
    .on_color_done      = NULL,
    .user_ctx           = NULL
};

static const sx_gpio_pin_t s_lcd_reset = {
    .pin  = BOARD_PIN_LCD_RESET,
    .mode = SX_GPIO_MODE_OUTPUT_PP
};

/* The duty field is the level applied at init: 0 keeps the active high backlight off. */
static const sx_pwm_config_t s_backlight = {
    .channel         = BOARD_BACKLIGHT_CHANNEL,
    .timer           = BOARD_BACKLIGHT_TIMER,
    .output_pin      = BOARD_PIN_BACKLIGHT,
    .frequency_hz    = BOARD_BACKLIGHT_FREQ_HZ,
    .resolution_bits = BOARD_BACKLIGHT_RES_BITS,
    .duty            = 0U
};

static const screen_hmi_config_t s_screen_config = {
    .bus                  = &s_lcd_bus,
    .reset                = &s_lcd_reset,
    .reset_low_ms         = 0U,
    .reset_wait_ms        = 0U,
    .backlight            = &s_backlight,
    .backlight_active_low = false,
    .width                = (uint16_t)BOARD_LCD_WIDTH,
    .height               = (uint16_t)BOARD_LCD_HEIGHT,
    .madctl               = (uint8_t)BOARD_LCD_MADCTL,
    .invert               = BOARD_LCD_INVERT,
    .init_cmds            = NULL,
    .init_cmd_count       = 0U,
    .delay_ms             = sx_delay_ms,
    .on_flush_done        = NULL,
    .user_ctx             = NULL
};

static const sx_i2c_config_t s_touch_i2c = {
    .port            = BOARD_TOUCH_I2C_PORT,
    .sda_pin         = BOARD_PIN_TOUCH_SDA,
    .scl_pin         = BOARD_PIN_TOUCH_SCL,
    .clock_hz        = BOARD_TOUCH_I2C_CLOCK_HZ,
    .internal_pullup = BOARD_TOUCH_I2C_PULLUP
};

/*
 * The reset line is shared with the display and driven by it, so reset is NULL here. A zero in
 * i2c_addr, timeout_ms, reset_low_ms and boot_wait_ms selects the defaults of the touch module.
 */
static const touch_config_t s_touch_config = {
    .i2c           = &s_touch_i2c,
    .i2c_addr      = 0U,
    .timeout_ms    = 0U,
    .reset         = NULL,
    .reset_low_ms  = 0U,
    .boot_wait_ms  = 0U,
    .width         = (uint16_t)BOARD_LCD_WIDTH,
    .height        = (uint16_t)BOARD_LCD_HEIGHT,
    .swap_xy       = BOARD_TOUCH_SWAP_XY,
    .mirror_x      = BOARD_TOUCH_MIRROR_X,
    .mirror_y      = BOARD_TOUCH_MIRROR_Y,
    .delay_ms      = sx_delay_ms
};

/* Not const: the SPI contract takes a non-const pointer for the software chip select. */
static sx_gpio_pin_t s_sd_cs = {
    .pin  = BOARD_PIN_SD_CS,
    .mode = SX_GPIO_MODE_OUTPUT_PP
};

/* The sdcard module requires mode 0, MSB first, a dummy byte of 0xFF and a software chip select. */
static const sx_spi_config_t s_sd_spi = {
    .host               = BOARD_SD_SPI_HOST,
    .mosi_pin           = BOARD_PIN_SD_MOSI,
    .miso_pin           = BOARD_PIN_SD_MISO,
    .sclk_pin           = BOARD_PIN_SD_SCLK,
    .clock_hz           = BOARD_SD_CLOCK_HZ,
    .mode               = SX_SPI_MODE_0,
    .lsb_first          = false,
    .max_transfer_bytes = 0U,
    .cs                 = &s_sd_cs,
    .cs_active_high     = false,
    .dummy              = 0xFFU
};

static const sdcard_config_t s_sdcard_config = {
    .spi           = &s_sd_spi,
    .init_clock_hz = 0U
};

/* Never format the card from here: formatting erases it and must be an explicit decision. */
static const storage_config_t s_storage_config = {
    .card                   = &s_sdcard,
    .card_config            = &s_sdcard_config,
    .format_if_mount_failed = false
};

static const speaker_config_t s_speaker_config = {
    .port           = BOARD_SPK_I2S_PORT,
    .bclk_pin       = BOARD_PIN_SPK_BCLK,
    .ws_pin         = BOARD_PIN_SPK_WS,
    .dout_pin       = BOARD_PIN_SPK_DOUT,
    .sample_rate_hz = BOARD_AUDIO_SAMPLE_RATE_HZ
};

static const inmp441_config_t s_mic_config = {
    .port           = BOARD_MIC_I2S_PORT,
    .bclk_pin       = BOARD_PIN_MIC_BCLK,
    .ws_pin         = BOARD_PIN_MIC_WS,
    .din_pin        = BOARD_PIN_MIC_DIN,
    .sample_rate_hz = BOARD_AUDIO_SAMPLE_RATE_HZ
};

int board_init(void)
{
    /* All values above are build time constants, so there is nothing to compute and no hardware to touch. */
    s_board_ready = true;
    return BOARD_OK;
}

int board_init_screen(void)
{
    if (!s_board_ready || s_screen_ready) {
        return BOARD_ERR_STATE;
    }
    int ret = screen_hmi_init(&s_screen, &s_screen_config);
    if (ret == SCREEN_HMI_OK) {
        s_screen_ready = true;
    }
    return ret;
}

int board_init_touch(void)
{
    /* The reset line is driven by the display, so the display must have run its reset first. */
    if (!s_board_ready || !s_screen_ready || s_touch_ready) {
        return BOARD_ERR_STATE;
    }
    int ret = touch_init(&s_touch, &s_touch_config);
    if (ret == TOUCH_OK) {
        s_touch_ready = true;
    }
    return ret;
}

int board_init_speaker(void)
{
    if (!s_board_ready || s_speaker_ready) {
        return BOARD_ERR_STATE;
    }
    int ret = speaker_init(&s_speaker, &s_speaker_config);
    if (ret == SPEAKER_OK) {
        s_speaker_ready = true;
    }
    return ret;
}

int board_init_mic(void)
{
    if (!s_board_ready || s_mic_ready) {
        return BOARD_ERR_STATE;
    }
    int ret = inmp441_init(&s_mic, &s_mic_config);
    if (ret == INMP441_OK) {
        s_mic_ready = true;
    }
    return ret;
}

int board_init_storage(void)
{
    if (!s_board_ready || s_storage_ready) {
        return BOARD_ERR_STATE;
    }
    int ret = storage_init(&s_storage_config);
    if (ret == STORAGE_OK) {
        s_storage_ready = true;
    }
    return ret;
}

screen_hmi_t *board_get_screen(void)
{
    return s_screen_ready ? &s_screen : NULL;
}

touch_t *board_get_touch(void)
{
    return s_touch_ready ? &s_touch : NULL;
}

speaker_t *board_get_speaker(void)
{
    return s_speaker_ready ? &s_speaker : NULL;
}

inmp441_t *board_get_mic(void)
{
    return s_mic_ready ? &s_mic : NULL;
}