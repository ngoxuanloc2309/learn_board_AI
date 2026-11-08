#ifndef APP_H
#define APP_H

/*
 * Entry point called from app_main. For now it only starts Wi-Fi (wifi_app).
 * The old app_init (it called every *_init of the apps, most of them have no body yet)
 * was removed from app.c in commit a6977f4. The old text is in commit 6a59be4.
 */
void app_start(void);

#endif