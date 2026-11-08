#ifndef SCREEN_APP_H
#define SCREEN_APP_H

#ifndef APP_SCREEN_TASK_NAME
#define APP_SCREEN_TASK_NAME "app_screen_task"
#endif

/*
 * Stack in bytes. The task runs board_init_screen, which creates the I80 bus and runs the panel
 * init sequence inside this task, and later it brings up the touch controller. Chosen by design.
 * Measured once on the board for the display part only: 3884 bytes were still free. The task prints
 * the lowest free stack after the touch init and then every APP_SCREEN_HEARTBEAT_MS.
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
 * Wait between two reads of the touch controller. Chosen by design, not measured. It must stay at
 * 20 ms or more: shorter waits are busy waits in sx_delay_ms and would starve the other tasks.
 */
#ifndef APP_SCREEN_TOUCH_POLL_MS
#define APP_SCREEN_TOUCH_POLL_MS 20U
#endif

/* Period of the alive line of the touch loop (it also shows the lowest free stack). */
#ifndef APP_SCREEN_HEARTBEAT_MS
#define APP_SCREEN_HEARTBEAT_MS 30000U
#endif

/*
 * Creates a task for the display and touch bring-up test. The task calls board_init and
 * board_init_screen, prints the MADCTL, inversion and pixel clock values taken from board.h,
 * switches the backlight on, fills the whole screen with red, green, blue, white and black
 * (printing the time of each fill), then draws a fixed pattern that stays on the screen:
 *   black background, white border 2 pixels wide, red block in the top left corner,
 *   green top right, blue bottom left, yellow bottom right, white square in the centre.
 * Display colors are RGB565 words (red 0xF800, green 0x07E0, blue 0x001F). What the eye sees tells:
 *   red, green, blue shown correctly: byte order and color order are right.
 *   blue, green, red: the BGR bit of MADCTL is wrong.
 *   blue, red, green: the byte order is wrong (swap_color_bytes).
 *   red, blue, green: both are wrong.
 * This table comes from RGB565 arithmetic. On the board (session 15) the panel needed inversion on
 * and showed the right colors and orientation with the values in board.h.
 *
 * Touch test: after the pattern the task brings up the touch controller (board_init_touch),
 * prints the chip, firmware and vendor numbers, and then reads it every APP_SCREEN_TOUCH_POLL_MS.
 * It prints one line when a touch starts and one more when the finger moves into another region:
 *   touch line: Tab in REGION x=X y=Y id=ID points=COUNT
 * with REGION one of: red, green, blue, yellow, white centre, white border, black.
 * x and y are the coordinates after the swap and mirror settings of board.h. Only the first point
 * is checked against the regions, a second finger is only counted. To tune the touch settings,
 * touch the centre of the four corner blocks and the centre square, and compare x and y with the
 * block positions (the blocks are 120 x 80 pixels, 2 pixels from the panel edge).
 * If the touch controller does not answer, the task prints the code and ends, the pattern stays.
 * When the touch works the task never ends.
 * Returns 0 when the task was created, -1 when it could not be created. Call it once.
 * Errors are printed by the task. Wi-Fi and HTTP are not affected.
 */
int screen_app_start(void);

#endif /* SCREEN_APP_H */