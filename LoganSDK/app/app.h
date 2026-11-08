#ifndef APP_H
#define APP_H

void app_init(void);

/*
 * Entry point called from app_main (defined in app_start.c). For now it only starts Wi-Fi (wifi_app).
 * app_init is not called yet because several of its *_init functions have no body.
 */
void app_start(void);

#endif