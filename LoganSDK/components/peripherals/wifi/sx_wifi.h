#ifndef SX_WIFI_H
#define SX_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define SX_WIFI_OK           0
#define SX_WIFI_ERR_PARAM    (-1)
#define SX_WIFI_ERR_STATE    (-2)
#define SX_WIFI_ERR_BUS      (-3)
#define SX_WIFI_ERR_TIMEOUT  (-4)
#define SX_WIFI_ERR_NOMEM    (-5)

#define SX_WIFI_SSID_MAX_LEN      32
#define SX_WIFI_PASSWORD_MAX_LEN  63
#define SX_WIFI_PASSWORD_MIN_LEN  8
#define SX_WIFI_IP_STR_LEN        16

/* Hard limit for ap_max_clients. When ap_max_clients is 0 the default below is used. */
#define SX_WIFI_AP_CLIENTS_LIMIT    8
#define SX_WIFI_AP_CLIENTS_DEFAULT  4

/* Used when sta_retry_ms is 0. Chosen by design, not measured. */
#define SX_WIFI_RETRY_MS_DEFAULT    5000U

typedef enum {
    SX_WIFI_MODE_STA = 1,
    SX_WIFI_MODE_AP = 2,
    SX_WIFI_MODE_APSTA = 3
} sx_wifi_mode_t;

typedef enum {
    SX_WIFI_EVENT_STA_CONNECTED,     /* linked to the router, no IP yet */
    SX_WIFI_EVENT_STA_GOT_IP,        /* router gave an IP, network is usable */
    SX_WIFI_EVENT_STA_DISCONNECTED,  /* link lost or connect failed, a retry is scheduled */
    SX_WIFI_EVENT_AP_CLIENT_JOINED,
    SX_WIFI_EVENT_AP_CLIENT_LEFT
} sx_wifi_event_t;

/*
 * Runs in the system event task. Keep it short and never block in it.
 * Do not call sx_wifi_wait_ip, sx_wifi_start, sx_wifi_stop or sx_wifi_deinit from it.
 * sx_wifi_get_status is allowed.
 */
typedef void (*sx_wifi_event_cb_t)(sx_wifi_event_t event, void *context);

typedef enum {
    SX_WIFI_STA_STOPPED = 0,
    SX_WIFI_STA_CONNECTING,  /* trying to link, or waiting for the next retry */
    SX_WIFI_STA_CONNECTED,   /* linked, no IP yet */
    SX_WIFI_STA_GOT_IP
} sx_wifi_sta_state_t;

/*
 * One Wi-Fi radio, so one instance. All strings are copied during init.
 *
 * STA (modes STA and APSTA): sta_ssid is required (1 to 32 chars).
 * sta_password may be NULL or empty for an open network, else 8 to 63 chars (WPA2).
 * The IP comes from DHCP. After a disconnect the driver retries every sta_retry_ms
 * (0 selects SX_WIFI_RETRY_MS_DEFAULT) until sx_wifi_stop.
 *
 * AP (modes AP and APSTA): ap_ssid is required (1 to 32 chars).
 * ap_password may be NULL or empty for an open AP, else 8 to 63 chars (WPA2).
 * ap_ip is a dotted IPv4 string, for example "192.168.4.1". The AP uses netmask
 * 255.255.255.0, the gateway is ap_ip, and a DHCP server hands out the rest of the subnet.
 * The last octet must be 1 to 254. In APSTA mode the AP subnet must differ from the router subnet.
 * The AP channel is not configurable: it starts on channel 1 and, once STA connects, the radio
 * follows the router channel, so the AP can change channel and clients may drop for a moment.
 * ap_max_clients: 0 selects SX_WIFI_AP_CLIENTS_DEFAULT, else 1 to SX_WIFI_AP_CLIENTS_LIMIT.
 *
 * callback may be NULL. Fields of the other mode are ignored.
 */
typedef struct {
    sx_wifi_mode_t mode;
    const char *sta_ssid;
    const char *sta_password;
    uint32_t sta_retry_ms;
    const char *ap_ssid;
    const char *ap_password;
    const char *ap_ip;
    uint8_t ap_max_clients;
    sx_wifi_event_cb_t callback;
    void *callback_context;
} sx_wifi_config_t;

typedef struct {
    sx_wifi_sta_state_t sta_state;
    char sta_ip[SX_WIFI_IP_STR_LEN];  /* empty string when there is no IP */
    int8_t sta_rssi;                  /* dBm, 0 when not linked */
    uint8_t sta_last_reason;          /* last disconnect reason, chip specific code, 0 if none */
    uint8_t ap_clients;               /* stations now joined to the AP */
    char ap_ip[SX_WIFI_IP_STR_LEN];   /* empty string when the mode has no AP */
} sx_wifi_status_t;

/*
 * Lifecycle calls (init, deinit, start, stop) must not run at the same time as any other
 * call of this contract. sx_wifi_get_status and sx_wifi_wait_ip may run from any task.
 *
 * sx_wifi_init prepares the driver and network interfaces but does not start the radio.
 * Failures inside the chip driver while creating the interfaces can abort the system
 * (this is the behaviour of the underlying API, normally only when memory runs out).
 * Returns SX_WIFI_ERR_STATE if already initialised.
 */
int sx_wifi_init(const sx_wifi_config_t *config);

/* Stops if running, then releases the driver. Safe to call again after an error. */
int sx_wifi_deinit(void);

/* Starts the radio. STA connects by itself and keeps retrying. AP starts and serves DHCP. */
int sx_wifi_start(void);

/* Stops the radio and the retry timer. The driver stays initialised. */
int sx_wifi_stop(void);

/*
 * Waits until STA has an IP. timeout_ms 0 only checks, UINT32_MAX waits practically forever.
 * Waits are rounded up to whole system ticks.
 * Returns SX_WIFI_ERR_STATE when the driver is not running or the mode has no STA,
 * and SX_WIFI_ERR_TIMEOUT when there is still no IP.
 */
int sx_wifi_wait_ip(uint32_t timeout_ms);

/* Copies the current state. Allowed whenever the driver is initialised. */
int sx_wifi_get_status(sx_wifi_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* SX_WIFI_H */