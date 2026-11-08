#include "app.h"
#include "wifi_app.h"

/*
 * Kept in its own file on purpose: app.c references init functions that have no body yet,
 * and the linker would pull that file in (and fail) as soon as this one is used.
 */
void app_start(void)
{
    /* The result is already printed by wifi_app_start. A failed Wi-Fi start does not stop the device. */
    (void)wifi_app_start();
}