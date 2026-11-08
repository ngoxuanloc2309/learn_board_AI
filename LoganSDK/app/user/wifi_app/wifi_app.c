#include <stdbool.h>
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

static const char *auth_name(sx_wifi_auth_t auth)
{
    switch (auth) {
    case SX_WIFI_AUTH_OPEN:
        return "open";
    case SX_WIFI_AUTH_WEP:
        return "wep";
    case SX_WIFI_AUTH_WPA:
        return "wpa";
    case SX_WIFI_AUTH_WPA2:
        return "wpa2";
    case SX_WIFI_AUTH_WPA_WPA2:
        return "wpa/wpa2";
    case SX_WIFI_AUTH_WPA3:
        return "wpa3";
    case SX_WIFI_AUTH_WPA2_WPA3:
        return "wpa2/wpa3";
    default:
        return "other";
    }
}

static uint32_t now_ms(void)
{
    uint32_t ms = 0U;

    sx_get_time_ms(&ms);
    return ms;
}

/* Static, so the task stack stays small. Only the Wi-Fi task uses it. */
static sx_wifi_scan_entry_t s_scan_entries[SX_WIFI_SCAN_MAX_ENTRIES];

/* Scans once and prints every network the chip can see, then says if the router was among them. */
static void run_scan(void)
{
    uint8_t count = 0U;
    uint16_t total = 0U;
    uint8_t index;
    bool target_seen = false;
    int result;

    result = sx_wifi_scan(s_scan_entries, SX_WIFI_SCAN_MAX_ENTRIES, &count, &total);
    if (result != SX_WIFI_OK) {
        printf("[wifi] scan failed: %d\n", result);
        return;
    }

    printf("[wifi] scan: %u networks seen, %u listed\n", (unsigned)total, (unsigned)count);
    for (index = 0U; index < count; index++) {
        const sx_wifi_scan_entry_t *entry = &s_scan_entries[index];

        printf("[wifi] scan #%u ch=%u rssi=%d auth=%s ssid=\"%s\"\n", (unsigned)index,
               (unsigned)entry->channel, (int)entry->rssi, auth_name(entry->auth), entry->ssid);
        if (strcmp(entry->ssid, DEFAULT_WIFI_SSID) == 0) {
            target_seen = true;
            printf("[wifi] scan: target \"%s\" seen on ch=%u rssi=%d auth=%s\n", DEFAULT_WIFI_SSID,
                   (unsigned)entry->channel, (int)entry->rssi, auth_name(entry->auth));
        }
    }
    if (!target_seen) {
        printf("[wifi] scan: target \"%s\" not in the %u listed of %u seen\n", DEFAULT_WIFI_SSID,
               (unsigned)count, (unsigned)total);
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
    uint32_t last_scan_ms;
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

    run_scan();
    last_scan_ms = now_ms();

    for (;;) {
        if (sx_wifi_get_status(&status) == SX_WIFI_OK) {
            printf("[wifi] sta=%s ip=%s rssi=%d reason=%u | ap=%s clients=%u\n",
                   sta_state_name(status.sta_state), status.sta_ip, (int)status.sta_rssi,
                   (unsigned)status.sta_last_reason, status.ap_ip, (unsigned)status.ap_clients);

            /* Time is measured by subtraction, so it stays correct when the counter wraps. */
            if ((APP_WIFI_SCAN_PERIOD_MS != 0U) && (status.sta_state != SX_WIFI_STA_GOT_IP) &&
                ((now_ms() - last_scan_ms) >= APP_WIFI_SCAN_PERIOD_MS)) {
                run_scan();
                last_scan_ms = now_ms();
            }
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