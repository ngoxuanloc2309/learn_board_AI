#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "sx_time.h"
#include "sx_wifi.h"
#include "wifi_app.h"

static const char *sta_state_name(sx_wifi_sta_state_t state)
{
    switch (state) {
    case SX_WIFI_STA_STOPPED:
        return "stopped";
    case SX_WIFI_STA_CONNECTING:
        return "connecting";
    case SX_WIFI_STA_CONNECTED:
        return "linked";
    case SX_WIFI_STA_GOT_IP:
        return "got_ip";
    default:
        return "unknown";
    }
}

/* Builds the config from app_config.h. The password is never printed. */
static void build_config(sx_wifi_config_t *config)
{
    memset(config, 0, sizeof(*config));
    config->mode = SX_WIFI_MODE_APSTA;
    config->sta_ssid = DEFAULT_WIFI_SSID;
    config->sta_password = DEFAULT_WIFI_PASSWORD;
    config->ap_ssid = DEFAULT_AP_SSID;
    config->ap_password = DEFAULT_AP_PASSWORD;
    config->ap_ip = DEFAULT_STATIC_IP;
}

static void wifi_task(void *arg)
{
    sx_wifi_config_t config;
    sx_wifi_status_t status;
    int result;

    (void)arg;
    build_config(&config);

    result = sx_wifi_init(&config);
    if (result != SX_WIFI_OK) {
        printf("[wifi] init failed: %d\n", result);
        vTaskDelete(NULL);
        return;
    }

    result = sx_wifi_start();
    if (result != SX_WIFI_OK) {
        printf("[wifi] start failed: %d\n", result);
        (void)sx_wifi_deinit();
        vTaskDelete(NULL);
        return;
    }
    printf("[wifi] started, AP %s on %s\n", DEFAULT_AP_SSID, DEFAULT_STATIC_IP);
    /* Helps to spot a typo or a hidden space in the settings. The password itself is never printed. */
    printf("[wifi] sta ssid=\"%s\" (%u chars), password %u chars\n", DEFAULT_WIFI_SSID,
           (unsigned)strlen(DEFAULT_WIFI_SSID), (unsigned)strlen(DEFAULT_WIFI_PASSWORD));

    for (;;) {
        if (sx_wifi_get_status(&status) == SX_WIFI_OK) {
            printf("[wifi] sta=%s ip=%s rssi=%d reason=%u | ap=%s clients=%u\n",
                   sta_state_name(status.sta_state), status.sta_ip, (int)status.sta_rssi,
                   (unsigned)status.sta_last_reason, status.ap_ip, (unsigned)status.ap_clients);
        }
        sx_delay_ms(APP_WIFI_STATUS_PERIOD_MS);
    }
}

int wifi_app_start(void)
{
    if (xTaskCreate(wifi_task, APP_WIFI_TASK_NAME, APP_WIFI_STACK_SIZE, NULL,
                    APP_WIFI_TASK_PRIORITY, NULL) != pdPASS) {
        printf("[wifi] cannot create the task\n");
        return -1;
    }
    return 0;
}