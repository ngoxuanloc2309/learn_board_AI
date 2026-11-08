#include "sx_wifi.h"#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#include "sx_wifi.h"

/* Initial AP channel. With STA connected the radio follows the router channel. */
#define AP_CHANNEL_DEFAULT   1U

#define GOT_IP_BIT           (1U << 0)

typedef struct {
    bool initialized;
    volatile bool running;
    bool wifi_inited;
    sx_wifi_mode_t mode;
    uint32_t retry_ms;
    sx_wifi_event_cb_t callback;
    void *callback_context;
    char ap_ip_text[SX_WIFI_IP_STR_LEN];
    esp_netif_t *sta_netif;
    esp_netif_t *ap_netif;
    esp_event_handler_instance_t wifi_handler;
    esp_event_handler_instance_t ip_handler;
    esp_timer_handle_t retry_timer;
    /* The fields below are shared with the event task and are guarded by s_lock. */
    sx_wifi_sta_state_t sta_state;
    char sta_ip[SX_WIFI_IP_STR_LEN];
    uint8_t last_reason;
    uint8_t ap_clients;
} wifi_state_t;

static wifi_state_t s_state;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

/* The event group is created once and never deleted, so it needs no heap. */
static EventGroupHandle_t s_group;
static StaticEventGroup_t s_group_storage;

static int map_error(esp_err_t error)
{
    switch (error) {
    case ESP_OK:
        return SX_WIFI_OK;
    case ESP_ERR_INVALID_ARG:
        return SX_WIFI_ERR_PARAM;
    case ESP_ERR_NO_MEM:
        return SX_WIFI_ERR_NOMEM;
    case ESP_ERR_TIMEOUT:
        return SX_WIFI_ERR_TIMEOUT;
    case ESP_ERR_INVALID_STATE:
    case ESP_ERR_WIFI_NOT_INIT:
    case ESP_ERR_WIFI_NOT_STARTED:
        return SX_WIFI_ERR_STATE;
    default:
        return SX_WIFI_ERR_BUS;
    }
}

static bool mode_has_sta(sx_wifi_mode_t mode)
{
    return (mode == SX_WIFI_MODE_STA) || (mode == SX_WIFI_MODE_APSTA);
}

static bool mode_has_ap(sx_wifi_mode_t mode)
{
    return (mode == SX_WIFI_MODE_AP) || (mode == SX_WIFI_MODE_APSTA);
}

/* NULL counts as an empty string. */
static size_t text_len(const char *text, size_t max_len)
{
    return (text == NULL) ? 0U : strnlen(text, max_len + 1U);
}

static bool ssid_ok(const char *ssid)
{
    size_t len = text_len(ssid, SX_WIFI_SSID_MAX_LEN);

    return (len >= 1U) && (len <= SX_WIFI_SSID_MAX_LEN);
}

/* Empty means an open network, else the WPA2 length rules apply. */
static bool password_ok(const char *password)
{
    size_t len = text_len(password, SX_WIFI_PASSWORD_MAX_LEN);

    return (len == 0U) ||
           ((len >= SX_WIFI_PASSWORD_MIN_LEN) && (len <= SX_WIFI_PASSWORD_MAX_LEN));
}

static bool parse_ap_ip(const char *text, esp_netif_ip_info_t *info)
{
    esp_ip4_addr_t ip;
    uint8_t first;
    uint8_t last;

    if ((text == NULL) || (esp_netif_str_to_ip4(text, &ip) != ESP_OK)) {
        return false;
    }

    first = (uint8_t)esp_ip4_addr1(&ip);
    last = (uint8_t)esp_ip4_addr4(&ip);
    if ((first == 0U) || (first == 127U) || (first >= 224U) ||
        (last == 0U) || (last == 255U)) {
        return false;
    }

    info->ip = ip;
    info->gw = ip;
    esp_netif_set_ip4_addr(&info->netmask, 255, 255, 255, 0);
    return true;
}

static bool valid_config(const sx_wifi_config_t *config, esp_netif_ip_info_t *ap_info)
{
    if ((config == NULL) || (config->mode < SX_WIFI_MODE_STA) ||
        (config->mode > SX_WIFI_MODE_APSTA)) {
        return false;
    }

    if (mode_has_sta(config->mode) &&
        (!ssid_ok(config->sta_ssid) || !password_ok(config->sta_password))) {
        return false;
    }

    if (mode_has_ap(config->mode)) {
        if (!ssid_ok(config->ap_ssid) || !password_ok(config->ap_password) ||
            (config->ap_max_clients > SX_WIFI_AP_CLIENTS_LIMIT) ||
            !parse_ap_ip(config->ap_ip, ap_info)) {
            return false;
        }
    }

    return true;
}

static void build_sta_config(const sx_wifi_config_t *config, wifi_config_t *out)
{
    size_t ssid_len = text_len(config->sta_ssid, SX_WIFI_SSID_MAX_LEN);
    size_t pass_len = text_len(config->sta_password, SX_WIFI_PASSWORD_MAX_LEN);

    memset(out, 0, sizeof(*out));
    memcpy(out->sta.ssid, config->sta_ssid, ssid_len);
    if (pass_len > 0U) {
        memcpy(out->sta.password, config->sta_password, pass_len);
    }
    /* threshold.authmode stays 0: the driver then accepts WPA2 or better when a password is set. */
    out->sta.pmf_cfg.capable = true;
    out->sta.pmf_cfg.required = false;
}

static void build_ap_config(const sx_wifi_config_t *config, wifi_config_t *out)
{
    size_t ssid_len = text_len(config->ap_ssid, SX_WIFI_SSID_MAX_LEN);
    size_t pass_len = text_len(config->ap_password, SX_WIFI_PASSWORD_MAX_LEN);

    memset(out, 0, sizeof(*out));
    memcpy(out->ap.ssid, config->ap_ssid, ssid_len);
    out->ap.ssid_len = (uint8_t)ssid_len;
    if (pass_len > 0U) {
        memcpy(out->ap.password, config->ap_password, pass_len);
        out->ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        out->ap.authmode = WIFI_AUTH_OPEN;
    }
    out->ap.channel = (uint8_t)AP_CHANNEL_DEFAULT;
    if (config->ap_max_clients == 0U) {
        out->ap.max_connection = (uint8_t)SX_WIFI_AP_CLIENTS_DEFAULT;
    } else {
        out->ap.max_connection = config->ap_max_clients;
    }
    /* Values below are the documented driver defaults, set explicitly. */
    out->ap.beacon_interval = 100U;
    out->ap.csa_count = 3U;
    out->ap.dtim_period = 1U;
    out->ap.pmf_cfg.capable = true;
    out->ap.pmf_cfg.required = false;
}

static wifi_mode_t to_driver_mode(sx_wifi_mode_t mode)
{
    switch (mode) {
    case SX_WIFI_MODE_STA:
        return WIFI_MODE_STA;
    case SX_WIFI_MODE_AP:
        return WIFI_MODE_AP;
    default:
        return WIFI_MODE_APSTA;
    }
}

/*
 * Writes "a.b.c.d" and a zero byte into out (SX_WIFI_IP_STR_LEN bytes is always enough).
 * Hand written instead of snprintf because this runs in the system event task, whose stack is small.
 */
static void format_ip(const esp_ip4_addr_t *ip, char *out)
{
    size_t pos = 0U;
    uint8_t octet;
    uint8_t index;

    for (index = 0U; index < 4U; index++) {
        octet = (uint8_t)esp_ip4_addr_get_byte(ip, index);
        if (octet >= 100U) {
            out[pos++] = (char)('0' + (octet / 100U));
        }
        if (octet >= 10U) {
            out[pos++] = (char)('0' + ((octet / 10U) % 10U));
        }
        out[pos++] = (char)('0' + (octet % 10U));
        if (index < 3U) {
            out[pos++] = '.';
        }
    }
    out[pos] = '\0';
}

static void notify(sx_wifi_event_t event)
{
    sx_wifi_event_cb_t callback = s_state.callback;

    if (callback != NULL) {
        callback(event, s_state.callback_context);
    }
}

static void set_sta_state(sx_wifi_sta_state_t state, bool clear_ip)
{
    portENTER_CRITICAL(&s_lock);
    s_state.sta_state = state;
    if (clear_ip) {
        s_state.sta_ip[0] = '\0';
    }
    portEXIT_CRITICAL(&s_lock);

    if (clear_ip) {
        (void)xEventGroupClearBits(s_group, GOT_IP_BIT);
    }
}

static void schedule_retry(void)
{
    esp_timer_handle_t timer = s_state.retry_timer;

    if (!s_state.running || (timer == NULL)) {
        return;
    }

    /* Stop fails when the timer is not active, which is fine. */
    (void)esp_timer_stop(timer);
    (void)esp_timer_start_once(timer, (uint64_t)s_state.retry_ms * 1000ULL);
}

static void retry_timer_cb(void *arg)
{
    (void)arg;

    if (!s_state.running) {
        return;
    }

    set_sta_state(SX_WIFI_STA_CONNECTING, false);
    if (esp_wifi_connect() != ESP_OK) {
        schedule_retry();
    }
}

static void handle_sta_disconnected(const wifi_event_sta_disconnected_t *info)
{
    bool running = s_state.running;

    portENTER_CRITICAL(&s_lock);
    if (info != NULL) {
        s_state.last_reason = info->reason;
    }
    s_state.sta_state = running ? SX_WIFI_STA_CONNECTING : SX_WIFI_STA_STOPPED;
    s_state.sta_ip[0] = '\0';
    portEXIT_CRITICAL(&s_lock);
    (void)xEventGroupClearBits(s_group, GOT_IP_BIT);

    notify(SX_WIFI_EVENT_STA_DISCONNECTED);
    schedule_retry();
}

static void handle_got_ip(const ip_event_got_ip_t *info)
{
    char text[SX_WIFI_IP_STR_LEN];

    if (info == NULL) {
        return;
    }

    format_ip(&info->ip_info.ip, text);
    portENTER_CRITICAL(&s_lock);
    s_state.sta_state = SX_WIFI_STA_GOT_IP;
    memcpy(s_state.sta_ip, text, sizeof(text));
    portEXIT_CRITICAL(&s_lock);
    (void)xEventGroupSetBits(s_group, GOT_IP_BIT);

    notify(SX_WIFI_EVENT_STA_GOT_IP);
}

static void handle_ap_client(bool joined)
{
    portENTER_CRITICAL(&s_lock);
    if (joined) {
        if (s_state.ap_clients < UINT8_MAX) {
            s_state.ap_clients++;
        }
    } else if (s_state.ap_clients > 0U) {
        s_state.ap_clients--;
    }
    portEXIT_CRITICAL(&s_lock);

    notify(joined ? SX_WIFI_EVENT_AP_CLIENT_JOINED : SX_WIFI_EVENT_AP_CLIENT_LEFT);
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;

    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            if (s_state.running) {
                set_sta_state(SX_WIFI_STA_CONNECTING, true);
                if (esp_wifi_connect() != ESP_OK) {
                    schedule_retry();
                }
            }
            break;
        case WIFI_EVENT_STA_CONNECTED:
            set_sta_state(SX_WIFI_STA_CONNECTED, false);
            notify(SX_WIFI_EVENT_STA_CONNECTED);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            handle_sta_disconnected((const wifi_event_sta_disconnected_t *)data);
            break;
        case WIFI_EVENT_STA_STOP:
            set_sta_state(SX_WIFI_STA_STOPPED, true);
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            handle_ap_client(true);
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            handle_ap_client(false);
            break;
        case WIFI_EVENT_AP_STOP:
            portENTER_CRITICAL(&s_lock);
            s_state.ap_clients = 0U;
            portEXIT_CRITICAL(&s_lock);
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT) {
        if (id == IP_EVENT_STA_GOT_IP) {
            handle_got_ip((const ip_event_got_ip_t *)data);
        } else if (id == IP_EVENT_STA_LOST_IP) {
            set_sta_state(SX_WIFI_STA_CONNECTED, true);
        }
    }
}

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();

    /*
     * The wifi driver needs NVS. When the partition is full or in a newer format it is erased
     * and created again. This removes anything else stored in NVS, so keep that in mind when
     * other code starts to use NVS.
     */
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    return err;
}

/*
 * The AP is normally already on 192.168.4.1, but the address comes from the config.
 * The server is stopped, the address is set, then the server is started again.
 * The AP is not up yet, so the server really starts when the AP starts, using the new address.
 */
static esp_err_t apply_ap_ip(esp_netif_t *netif, const esp_netif_ip_info_t *info)
{
    esp_err_t err = esp_netif_dhcps_stop(netif);

    if ((err != ESP_OK) && (err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)) {
        return err;
    }
    err = esp_netif_set_ip_info(netif, info);
    if (err != ESP_OK) {
        return err;
    }
    return esp_netif_dhcps_start(netif);
}

/* Undoes whatever init did. Every step is skipped when its resource does not exist. */
static void release_all(void)
{
    esp_timer_handle_t timer = s_state.retry_timer;

    s_state.running = false;
    s_state.retry_timer = NULL;
    if (timer != NULL) {
        (void)esp_timer_stop(timer);
        (void)esp_timer_delete(timer);
    }

    if (s_state.wifi_handler != NULL) {
        (void)esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                    s_state.wifi_handler);
        s_state.wifi_handler = NULL;
    }
    if (s_state.ip_handler != NULL) {
        (void)esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID,
                                                    s_state.ip_handler);
        s_state.ip_handler = NULL;
    }

    if (s_state.wifi_inited) {
        (void)esp_wifi_deinit();
        s_state.wifi_inited = false;
    }

    if (s_state.sta_netif != NULL) {
        esp_netif_destroy_default_wifi(s_state.sta_netif);
        s_state.sta_netif = NULL;
    }
    if (s_state.ap_netif != NULL) {
        esp_netif_destroy_default_wifi(s_state.ap_netif);
        s_state.ap_netif = NULL;
    }

    s_state.initialized = false;
}

int sx_wifi_init(const sx_wifi_config_t *config)
{
    esp_netif_ip_info_t ap_info;
    wifi_config_t sta_config;
    wifi_config_t ap_config;
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err;

    memset(&ap_info, 0, sizeof(ap_info));
    memset(&sta_config, 0, sizeof(sta_config));
    memset(&ap_config, 0, sizeof(ap_config));
    if (!valid_config(config, &ap_info)) {
        return SX_WIFI_ERR_PARAM;
    }
    if (s_state.initialized) {
        return SX_WIFI_ERR_STATE;
    }

    if (s_group == NULL) {
        s_group = xEventGroupCreateStatic(&s_group_storage);
    }
    (void)xEventGroupClearBits(s_group, GOT_IP_BIT);

    memset(&s_state, 0, sizeof(s_state));
    s_state.mode = config->mode;
    s_state.retry_ms = (config->sta_retry_ms == 0U) ? SX_WIFI_RETRY_MS_DEFAULT
                                                    : config->sta_retry_ms;
    s_state.callback = config->callback;
    s_state.callback_context = config->callback_context;
    s_state.sta_state = SX_WIFI_STA_STOPPED;
    if (mode_has_ap(config->mode)) {
        format_ip(&ap_info.ip, s_state.ap_ip_text);
        build_ap_config(config, &ap_config);
    }
    if (mode_has_sta(config->mode)) {
        build_sta_config(config, &sta_config);
    }

    err = init_nvs();
    if (err == ESP_OK) {
        err = esp_netif_init();
    }
    if (err == ESP_OK) {
        err = esp_event_loop_create_default();
        /* The default loop may already exist, that is fine. */
        if (err == ESP_ERR_INVALID_STATE) {
            err = ESP_OK;
        }
    }
    if (err != ESP_OK) {
        release_all();
        return map_error(err);
    }

    /* These two calls abort on failure instead of returning an error (see esp_wifi_default.h). */
    if (mode_has_sta(config->mode)) {
        s_state.sta_netif = esp_netif_create_default_wifi_sta();
    }
    if (mode_has_ap(config->mode)) {
        s_state.ap_netif = esp_netif_create_default_wifi_ap();
        err = apply_ap_ip(s_state.ap_netif, &ap_info);
        if (err != ESP_OK) {
            release_all();
            return map_error(err);
        }
    }

    err = esp_wifi_init(&init_config);
    if (err != ESP_OK) {
        release_all();
        return map_error(err);
    }
    s_state.wifi_inited = true;

    /* Settings come from the config on every start, so keep them out of flash. */
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err == ESP_OK) {
        err = esp_wifi_set_mode(to_driver_mode(config->mode));
    }
    if ((err == ESP_OK) && mode_has_sta(config->mode)) {
        err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    }
    if ((err == ESP_OK) && mode_has_ap(config->mode)) {
        err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    }
    if (err == ESP_OK) {
        err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler,
                                                  NULL, &s_state.wifi_handler);
    }
    if (err == ESP_OK) {
        err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, event_handler,
                                                  NULL, &s_state.ip_handler);
    }
    if ((err == ESP_OK) && mode_has_sta(config->mode)) {
        const esp_timer_create_args_t timer_args = {
            .callback = retry_timer_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "sx_wifi_retry",
            .skip_unhandled_events = false,
        };

        err = esp_timer_create(&timer_args, &s_state.retry_timer);
    }
    if (err != ESP_OK) {
        release_all();
        return map_error(err);
    }

    s_state.initialized = true;
    return SX_WIFI_OK;
}

int sx_wifi_deinit(void)
{
    if (!s_state.initialized) {
        return SX_WIFI_OK;
    }

    if (s_state.running) {
        (void)sx_wifi_stop();
    }
    release_all();
    return SX_WIFI_OK;
}

int sx_wifi_start(void)
{
    esp_err_t err;

    if (!s_state.initialized || s_state.running) {
        return SX_WIFI_ERR_STATE;
    }

    portENTER_CRITICAL(&s_lock);
    s_state.sta_state = SX_WIFI_STA_STOPPED;
    s_state.sta_ip[0] = '\0';
    s_state.ap_clients = 0U;
    portEXIT_CRITICAL(&s_lock);
    (void)xEventGroupClearBits(s_group, GOT_IP_BIT);

    /* running is set first: the start event can arrive before esp_wifi_start returns. */
    s_state.running = true;
    err = esp_wifi_start();
    if (err != ESP_OK) {
        s_state.running = false;
        return map_error(err);
    }
    return SX_WIFI_OK;
}

int sx_wifi_stop(void)
{
    esp_timer_handle_t timer;
    esp_err_t err;

    if (!s_state.initialized) {
        return SX_WIFI_ERR_STATE;
    }
    if (!s_state.running) {
        return SX_WIFI_OK;
    }

    /* running goes false first so a disconnect during the stop does not schedule a retry. */
    s_state.running = false;
    timer = s_state.retry_timer;
    if (timer != NULL) {
        (void)esp_timer_stop(timer);
    }

    err = esp_wifi_stop();

    portENTER_CRITICAL(&s_lock);
    s_state.sta_state = SX_WIFI_STA_STOPPED;
    s_state.sta_ip[0] = '\0';
    s_state.ap_clients = 0U;
    portEXIT_CRITICAL(&s_lock);
    (void)xEventGroupClearBits(s_group, GOT_IP_BIT);

    return map_error(err);
}

/* Rounds up to whole ticks so a short wait is never shortened to zero. */
static TickType_t to_ticks(uint32_t timeout_ms)
{
    uint64_t ticks;

    if (timeout_ms == UINT32_MAX) {
        return portMAX_DELAY;
    }
    ticks = (((uint64_t)timeout_ms * (uint64_t)configTICK_RATE_HZ) + 999ULL) / 1000ULL;
    if (ticks >= (uint64_t)portMAX_DELAY) {
        return portMAX_DELAY;
    }
    return (TickType_t)ticks;
}

int sx_wifi_wait_ip(uint32_t timeout_ms)
{
    EventBits_t bits;

    if (!s_state.initialized || !s_state.running || !mode_has_sta(s_state.mode)) {
        return SX_WIFI_ERR_STATE;
    }

    bits = xEventGroupWaitBits(s_group, GOT_IP_BIT, pdFALSE, pdTRUE, to_ticks(timeout_ms));
    return ((bits & GOT_IP_BIT) != 0U) ? SX_WIFI_OK : SX_WIFI_ERR_TIMEOUT;
}

int sx_wifi_get_status(sx_wifi_status_t *status)
{
    wifi_ap_record_t record;
    bool linked;

    if (status == NULL) {
        return SX_WIFI_ERR_PARAM;
    }
    if (!s_state.initialized) {
        return SX_WIFI_ERR_STATE;
    }

    memset(status, 0, sizeof(*status));
    portENTER_CRITICAL(&s_lock);
    status->sta_state = s_state.sta_state;
    memcpy(status->sta_ip, s_state.sta_ip, sizeof(status->sta_ip));
    status->sta_last_reason = s_state.last_reason;
    status->ap_clients = s_state.ap_clients;
    portEXIT_CRITICAL(&s_lock);
    memcpy(status->ap_ip, s_state.ap_ip_text, sizeof(status->ap_ip));

    linked = (status->sta_state == SX_WIFI_STA_CONNECTED) ||
             (status->sta_state == SX_WIFI_STA_GOT_IP);
    if (linked && (esp_wifi_sta_get_ap_info(&record) == ESP_OK)) {
        status->sta_rssi = record.rssi;
    }
    return SX_WIFI_OK;
}