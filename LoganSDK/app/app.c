#include "app.h"
#include "wifi_app.h"

void app_start(void)
{
    /* The result is already printed by wifi_app_start. A failed Wi-Fi start does not stop the device. */
    (void)wifi_app_start();
}