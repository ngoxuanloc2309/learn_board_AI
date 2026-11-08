#include "app.h"
#include "http_app.h"
#include "screen_app.h"
#include "wifi_app.h"

void app_start(void)
{
    /* The result is already printed by wifi_app_start. A failed Wi-Fi start does not stop the device. */
    (void)wifi_app_start();
    /* http_app waits for the Wi-Fi IP by itself, so the order of these two calls does not matter. */
    (void)http_app_start();
    /* screen_app brings up the display by itself in its own task (board_init is called there). */
    (void)screen_app_start();
}