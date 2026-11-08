#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define APP_NAME                "LoganSDK"
#define APP_VERSION             "1.0.0"
#define APP_BUILD_DATE          "01/11/2026"
#define APP_USE_OS              1 /*FreeRTOS*/

/* Device will active all of mode which include: STA &AP*/

/*STA Mode*/
#define DEFAULT_WIFI_SSID       "YourSSID"
#define DEFAULT_WIFI_PASSWORD   "YourPassword"

/*AP Mode*/
#define DEFAULT_AP_SSID         "LEARN_BOARD_2309"
#define DEFAULT_AP_PASSWORD     "123456789"
#define DEFAULT_STATIC_IP       "192.168.4.1"

/*Others Configurations*/

#endif // APP_CONFIG_H