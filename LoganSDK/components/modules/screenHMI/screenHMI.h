#ifndef SCREEN_HMI_H
#define SCREEN_HMI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sx_i80.h"
#include "sx_gpio.h"
#include "sx_pwm.h"

/*
 * TFT panel of the ST7796 family on an 8080 parallel bus, with an optional reset line and PWM backlight.
 * Built only on the sx_i80, sx_gpio and sx_pwm contracts. Pixels are RGB565 (16 bits per pixel).
 *
 * The module sends only standard MIPI DCS commands itself (software reset, sleep out, pixel format,
 * memory access control, inversion, display on and off, column and row address, memory write).
 * Everything specific to the panel (power, gamma, extended command unlock, and so on) comes from the
 * init command table that the board layer passes in the configuration.
 *
 * Not thread safe. The caller must serialise all calls. Do not start a new draw before the previous
 * draw reported completion, because the pixel buffer must stay valid until then.
 */

/* Return codes: 0 on success, negative on failure. */
#define SCREEN_HMI_OK              0
#define SCREEN_HMI_ERR_PARAM      (-1)   /* NULL pointer, bad configuration, or area outside the panel */
#define SCREEN_HMI_ERR_STATE      (-2)   /* not initialised, already initialised, or the feature is not configured */
#define SCREEN_HMI_ERR_BUS        (-3)   /* the I80 layer reported an error */
#define SCREEN_HMI_ERR_TIMEOUT    (-4)   /* a blocking transfer did not finish in time */
#define SCREEN_HMI_ERR_HW         (-5)   /* the GPIO or PWM layer reported an error */

/* Bits of the memory access control register (command 0x36), standard MIPI DCS. */
#define SCREEN_HMI_MADCTL_MY      0x80U   /* row address order */
#define SCREEN_HMI_MADCTL_MX      0x40U   /* column address order */
#define SCREEN_HMI_MADCTL_MV      0x20U   /* row and column exchange (landscape) */
#define SCREEN_HMI_MADCTL_ML      0x10U   /* vertical refresh order */
#define SCREEN_HMI_MADCTL_BGR     0x08U   /* 1: blue first (BGR), 0: red first (RGB) */
#define SCREEN_HMI_MADCTL_MH      0x04U   /* horizontal refresh order */

/* Pixels held by the fill buffer inside screen_hmi_t. It must be at least as large as the widest fill. */
#ifndef SCREEN_HMI_FILL_BUF_PIXELS
#define SCREEN_HMI_FILL_BUF_PIXELS 1920U
#endif

/* One panel specific command. It is sent in order, then the module waits delay_ms. */
typedef struct screen_hmi_init_cmd {
    uint8_t         cmd;
    const uint8_t  *param;         /* may be NULL when param_len is 0 */
    uint8_t         param_len;
    uint16_t        delay_ms;      /* wait after the command, 0 for none */
} screen_hmi_init_cmd_t;

/* Waits at least ms milliseconds. Provided by the board layer. Must be callable from the calling task. */
typedef void (*screen_hmi_delay_fn_t)(uint32_t ms);

/*
 * Called when a screen_hmi_draw_bitmap() transfer has finished. It may run in interrupt context,
 * so it must not block. Return true when a higher priority task was woken, otherwise false.
 */
typedef bool (*screen_hmi_flush_cb_t)(void *user_ctx);

typedef struct screen_hmi_config {
    /*
     * I80 bus and panel interface, provided by the board layer. The module keeps its own copy and
     * replaces on_color_done and user_ctx of that copy, so set the completion callback below instead.
     * max_transfer_bytes limits one draw, and swap_color_bytes decides the byte order sent to the panel.
     */
    const sx_i80_config_t *bus;

    /* Hardware reset line, an output pin. NULL when the line is not driven by this module (no line, or shared and driven elsewhere). */
    const sx_gpio_pin_t   *reset;
    uint16_t               reset_low_ms;    /* time the line is held low, 0 = default */
    uint16_t               reset_wait_ms;   /* wait after the line goes high, 0 = default */

    /*
     * Backlight PWM, NULL when there is none. The duty field of this config is the raw duty applied at init, so use the
     * level that keeps the backlight off (0 for an active-high backlight, the maximum for active-low).
     */
    const sx_pwm_config_t *backlight;
    bool                   backlight_active_low;

    uint16_t               width;           /* panel size in pixels as seen after the rotation set by madctl */
    uint16_t               height;
    uint8_t                madctl;          /* value for command 0x36, built from SCREEN_HMI_MADCTL_* bits */
    bool                   invert;          /* true: send display inversion on */

    const screen_hmi_init_cmd_t *init_cmds; /* panel specific setup, NULL when init_cmd_count is 0 */
    size_t                       init_cmd_count;

    screen_hmi_delay_fn_t  delay_ms;        /* required */
    screen_hmi_flush_cb_t  on_flush_done;   /* optional */
    void                  *user_ctx;        /* passed to on_flush_done */
} screen_hmi_config_t;

typedef struct screen_hmi {
    const screen_hmi_config_t *config;
    sx_i80_config_t            bus_config;      /* private copy of config->bus with the module callback */
    sx_i80_t                   i80;
    sx_pwm_t                   pwm;
    bool                       backlight_ready;
    volatile bool              fill_active;
    volatile bool              fill_done;
    bool                       ready;
    uint16_t                   fill_buf[SCREEN_HMI_FILL_BUF_PIXELS];
} screen_hmi_t;

/*
 * Brings up the bus, the backlight and the panel: hardware reset (when configured), software reset,
 * sleep out, the board init table, pixel format RGB565, memory access control, inversion, display on.
 * The screen_hmi_t must be zero-initialised (for example a static variable) before the first call.
 * The config and everything it points to must stay valid and unchanged while the module is in use.
 * The screen_hmi_t must not move while it is initialised. Blocks for several hundred milliseconds.
 */
int screen_hmi_init(screen_hmi_t *hmi, const screen_hmi_config_t *config);

/* Turns the backlight off, switches the display off, and releases the bus and the backlight. */
int screen_hmi_deinit(screen_hmi_t *hmi);

/* Display on or off. The frame memory is kept. */
int screen_hmi_set_display_on(screen_hmi_t *hmi, bool on);

/* Sets the backlight brightness, 0 to 100 percent. Returns SCREEN_HMI_ERR_STATE when no backlight is configured. */
int screen_hmi_set_backlight(screen_hmi_t *hmi, uint8_t percent);

/*
 * Starts drawing a rectangle. The end coordinates are inclusive. pixels holds (x1 - x0 + 1) * (y1 - y0 + 1)
 * RGB565 values and len is that size in bytes, which must not exceed max_transfer_bytes of the bus.
 * The transfer runs in the background: pixels must stay valid and unchanged until on_flush_done is called.
 * Buffers should be DMA capable and aligned as the platform requires.
 */
int screen_hmi_draw_bitmap(screen_hmi_t *hmi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           const void *pixels, size_t len);

/*
 * Fills a rectangle with one color and returns when it is done. The end coordinates are inclusive.
 * color is an RGB565 value held in a uint16_t exactly as the pixels given to screen_hmi_draw_bitmap.
 * Meant for bring-up and simple screens. Do not call it while a draw is still running.
 * The rectangle width must not exceed SCREEN_HMI_FILL_BUF_PIXELS.
 */
int screen_hmi_fill_rect(screen_hmi_t *hmi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* SCREEN_HMI_H */