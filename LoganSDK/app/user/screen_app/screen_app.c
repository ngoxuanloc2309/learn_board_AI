#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "screenHMI.h"
#include "sx_time.h"
#include "screen_app.h"

/* RGB565 words as the module expects them (native uint16_t, byte order handled by the bus). */
#define COLOR_RED      0xF800U
#define COLOR_GREEN    0x07E0U
#define COLOR_BLUE     0x001FU
#define COLOR_WHITE    0xFFFFU
#define COLOR_BLACK    0x0000U
#define COLOR_YELLOW   0xFFE0U

/* Geometry of the fixed pattern, in pixels. The panel size comes from board.h. */
#define PANEL_W        BOARD_LCD_WIDTH
#define PANEL_H        BOARD_LCD_HEIGHT
#define BORDER         2U
#define BLOCK_W        120U
#define BLOCK_H        80U
#define CENTER_SIZE    40U

typedef struct solid_step {
    const char *name;
    uint16_t    color;
} solid_step_t;

typedef struct pattern_rect {
    const char *name;
    uint16_t    x0;
    uint16_t    y0;
    uint16_t    x1;
    uint16_t    y1;
    uint16_t    color;
} pattern_rect_t;

static const solid_step_t s_solid_steps[] = {
    { "red",   COLOR_RED   },
    { "green", COLOR_GREEN },
    { "blue",  COLOR_BLUE  },
    { "white", COLOR_WHITE },
    { "black", COLOR_BLACK }
};

/* Drawn in this order, so later entries are on top of earlier ones. */
static const pattern_rect_t s_pattern[] = {
    { "background",   0U, 0U, PANEL_W - 1U, PANEL_H - 1U, COLOR_BLACK },
    { "border top",    0U, 0U, PANEL_W - 1U, BORDER - 1U, COLOR_WHITE },
    { "border bottom", 0U, PANEL_H - BORDER, PANEL_W - 1U, PANEL_H - 1U, COLOR_WHITE },
    { "border left",   0U, 0U, BORDER - 1U, PANEL_H - 1U, COLOR_WHITE },
    { "border right",  PANEL_W - BORDER, 0U, PANEL_W - 1U, PANEL_H - 1U, COLOR_WHITE },
    { "top left",      BORDER, BORDER, BORDER + BLOCK_W - 1U, BORDER + BLOCK_H - 1U, COLOR_RED },
    { "top right",     PANEL_W - BORDER - BLOCK_W, BORDER, PANEL_W - BORDER - 1U,
                       BORDER + BLOCK_H - 1U, COLOR_GREEN },
    { "bottom left",   BORDER, PANEL_H - BORDER - BLOCK_H, BORDER + BLOCK_W - 1U,
                       PANEL_H - BORDER - 1U, COLOR_BLUE },
    { "bottom right",  PANEL_W - BORDER - BLOCK_W, PANEL_H - BORDER - BLOCK_H,
                       PANEL_W - BORDER - 1U, PANEL_H - BORDER - 1U, COLOR_YELLOW },
    { "centre",        (PANEL_W - CENTER_SIZE) / 2U, (PANEL_H - CENTER_SIZE) / 2U,
                       ((PANEL_W - CENTER_SIZE) / 2U) + CENTER_SIZE - 1U,
                       ((PANEL_H - CENTER_SIZE) / 2U) + CENTER_SIZE - 1U, COLOR_WHITE }
};

#define SOLID_STEP_COUNT   (sizeof(s_solid_steps) / sizeof(s_solid_steps[0]))
#define PATTERN_RECT_COUNT (sizeof(s_pattern) / sizeof(s_pattern[0]))

/* Prints the lowest free stack, then ends the calling task. */
static void end_task(void)
{
    printf("[screen] task ends, lowest free stack %u of %u bytes\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)APP_SCREEN_STACK_SIZE);
    vTaskDelete(NULL);
}

/* Fills one rectangle, measures the time and prints the result. Returns the module code. */
static int fill_area(screen_hmi_t *hmi, const char *name, uint16_t x0, uint16_t y0,
                     uint16_t x1, uint16_t y1, uint16_t color)
{
    uint32_t start_ms = 0U;
    uint32_t end_ms = 0U;
    int ret;

    sx_get_time_ms(&start_ms);
    ret = screen_hmi_fill_rect(hmi, x0, y0, x1, y1, color);
    sx_get_time_ms(&end_ms);

    if (ret != SCREEN_HMI_OK) {
        printf("[screen] fill %s failed: %d\n", name, ret);
        return ret;
    }
    /* Time is measured by subtraction, so it stays correct when the counter wraps. */
    printf("[screen] fill %s x=%u..%u y=%u..%u rgb565=0x%04X took %u ms\n", name,
           (unsigned)x0, (unsigned)x1, (unsigned)y0, (unsigned)y1, (unsigned)color,
           (unsigned)(end_ms - start_ms));
    return SCREEN_HMI_OK;
}

static int run_solid_colors(screen_hmi_t *hmi)
{
    size_t i;

    for (i = 0U; i < SOLID_STEP_COUNT; i++) {
        int ret = fill_area(hmi, s_solid_steps[i].name, 0U, 0U,
                            (uint16_t)(PANEL_W - 1U), (uint16_t)(PANEL_H - 1U),
                            s_solid_steps[i].color);
        if (ret != SCREEN_HMI_OK) {
            return ret;
        }
        sx_delay_ms(APP_SCREEN_HOLD_MS);
    }
    return SCREEN_HMI_OK;
}

static int run_pattern(screen_hmi_t *hmi)
{
    size_t i;

    for (i = 0U; i < PATTERN_RECT_COUNT; i++) {
        const pattern_rect_t *r = &s_pattern[i];
        int ret = fill_area(hmi, r->name, r->x0, r->y0, r->x1, r->y1, r->color);

        if (ret != SCREEN_HMI_OK) {
            return ret;
        }
    }
    return SCREEN_HMI_OK;
}

static void screen_task(void *arg)
{
    screen_hmi_t *hmi;
    int ret;

    (void)arg;

    ret = board_init();
    if (ret != BOARD_OK) {
        printf("[screen] board_init failed: %d\n", ret);
        end_task();
        return;
    }

    ret = board_init_screen();
    if (ret != BOARD_OK) {
        printf("[screen] board_init_screen failed: %d\n", ret);
        end_task();
        return;
    }
    hmi = board_get_screen();
    if (hmi == NULL) {
        printf("[screen] board_get_screen returned NULL\n");
        end_task();
        return;
    }

    /* These values are unverified guesses, see board.h. They are printed to match them with what is seen. */
    printf("[screen] panel up: size=%ux%u madctl=0x%02X invert=%s pclk=%u Hz\n",
           (unsigned)BOARD_LCD_WIDTH, (unsigned)BOARD_LCD_HEIGHT, (unsigned)BOARD_LCD_MADCTL,
           BOARD_LCD_INVERT ? "on" : "off", (unsigned)BOARD_LCD_PCLK_HZ);

    ret = screen_hmi_set_backlight(hmi, (uint8_t)APP_SCREEN_BACKLIGHT_PERCENT);
    if (ret != SCREEN_HMI_OK) {
        printf("[screen] set_backlight failed: %d\n", ret);
        end_task();
        return;
    }
    printf("[screen] backlight %u percent\n", (unsigned)APP_SCREEN_BACKLIGHT_PERCENT);

    if (run_solid_colors(hmi) == SCREEN_HMI_OK) {
        printf("[screen] expect: pattern next, red TL, green TR, blue BL, yellow BR, white centre and border\n");
        (void)run_pattern(hmi);
    }
    end_task();
}

int screen_app_start(void)
{
    if (xTaskCreate(screen_task, APP_SCREEN_TASK_NAME, APP_SCREEN_STACK_SIZE, NULL,
                    APP_SCREEN_TASK_PRIORITY, NULL) != pdPASS) {
        printf("[screen] cannot create the task\n");
        return -1;
    }
    return 0;
}