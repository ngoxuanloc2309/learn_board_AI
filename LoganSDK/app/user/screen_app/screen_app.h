#ifndef SCREEN_APP_H
#define SCREEN_APP_H

#ifndef APP_SCREEN_TASK_NAME
#define APP_SCREEN_TASK_NAME "app_screen_task"
#endif

/*
 * Stack in bytes. The task runs board_init_screen, which creates the I80 bus and runs the panel
 * init sequence inside this task. Chosen by design, not measured. The task prints the lowest free
 * stack when it ends, use that number to tune this value.
 */
#ifndef APP_SCREEN_STACK_SIZE
#define APP_SCREEN_STACK_SIZE 6144
#endif

/* Below the Wi-Fi task (3), same as the HTTP task. Chosen by design, not measured. */
#ifndef APP_SCREEN_TASK_PRIORITY
#define APP_SCREEN_TASK_PRIORITY 2
#endif

/* Backlight level after the panel is up, 0 to 100 percent. Chosen by design. */
#ifndef APP_SCREEN_BACKLIGHT_PERCENT
#define APP_SCREEN_BACKLIGHT_PERCENT 100U
#endif

/* How long each solid color stays on the screen. Chosen by design. */
#ifndef APP_SCREEN_HOLD_MS
#define APP_SCREEN_HOLD_MS 1500U
#endif

/*
 * Creates a task for the display bring-up test. The task calls board_init and board_init_screen,
 * prints the MADCTL, inversion and pixel clock values taken from board.h, switches the backlight on,
 * fills the whole screen with red, green, blue, white and black (printing the time of each fill),
 * then draws a fixed pattern that stays on the screen:
 *   black background, white border 2 pixels wide, red block in the top left corner,
 *   green top right, blue bottom left, yellow bottom right, white square in the centre.
 * Then it prints the lowest free stack and ends.
 * Colors are RGB565 words (red 0xF800, green 0x07E0, blue 0x001F). What the eye sees tells:
 *   red, green, blue shown correctly: byte order and color order are right.
 *   blue, green, red: the BGR bit of MADCTL is wrong.
 *   blue, red, green: the byte order is wrong (swap_color_bytes).
 *   red, blue, green: both are wrong.
 * This table comes from RGB565 arithmetic, it is not verified on the board.
 * Orientation: compare the corner colors with the list above. Inversion: black must look black.
 * Pixel clock: change BOARD_LCD_PCLK_HZ in board.h, rebuild, and look for noise or errors.
 * Returns 0 when the task was created, -1 when it could not be created. Call it once.
 * Errors are printed by the task, and the task then ends. Wi-Fi and HTTP are not affected.
 */
int screen_app_start(void);

#endif /* SCREEN_APP_H */