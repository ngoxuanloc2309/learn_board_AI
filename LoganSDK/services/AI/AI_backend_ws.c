#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_transport.h"
#include "esp_transport_tcp.h"
#include "esp_transport_ws.h"

#include "AI_backend_ws.h"

#define HOST_MAX              48U
#define PATH_MAX_LEN          64U
#define CONNECT_TIMEOUT_MS    3000   /* TCP connect and the upgrade answer, each. Design value */
#define SEND_TIMEOUT_MS       1000   /* wait for the socket to take a frame. Design value */
#define READ_TIMEOUT_MS       1000   /* once a frame has started. Long on purpose: a timeout inside a payload breaks the stream */
#define KEEPALIVE_IDLE_S      5      /* TCP keep-alive: dead peer found after about idle + interval * count = 11 s. Design */
#define KEEPALIVE_INTERVAL_S  2
#define KEEPALIVE_COUNT       3
#define MAX_PASSES            16U    /* frames joined into one message at most */
#define MIN_READ_ROOM         4U     /* ws_read_header may keep a 4 byte mask in the caller's buffer */

static const char *TAG = "ai_ws";

static esp_transport_handle_t s_tcp;
static esp_transport_handle_t s_ws;
static char     s_host[HOST_MAX];
static char     s_path[PATH_MAX_LEN];
static uint16_t s_port;
static bool     s_open;   /* connect succeeded and no close or failure since */

int ai_ws_esp_init(const char *url)
{
    esp_transport_ws_config_t wcfg;
    esp_transport_keep_alive_t ka;

    if ((url == NULL) || (s_tcp != NULL)) {
        return AI_BACKEND_ERR_PARAM;
    }
    if (ai_backend_parse_url(url, s_host, sizeof(s_host), &s_port, s_path, sizeof(s_path)) != AI_BACKEND_OK) {
        return AI_BACKEND_ERR_PARAM;
    }
    s_tcp = esp_transport_tcp_init();
    if (s_tcp == NULL) {
        return AI_BACKEND_ERR_IO;
    }
    s_ws = esp_transport_ws_init(s_tcp);
    if (s_ws == NULL) {
        (void)esp_transport_destroy(s_tcp);   /* the ws transport does not own it, so it is freed here */
        s_tcp = NULL;
        return AI_BACKEND_ERR_IO;
    }
    (void)memset(&wcfg, 0, sizeof(wcfg));
    wcfg.ws_path = s_path;                    /* copied by the transport (strdup) */
    wcfg.propagate_control_frames = false;    /* ping and pong are answered inside esp_transport_read */
    if (esp_transport_ws_set_config(s_ws, &wcfg) != ESP_OK) {
        (void)esp_transport_destroy(s_ws);
        (void)esp_transport_destroy(s_tcp);
        s_ws = NULL;
        s_tcp = NULL;
        return AI_BACKEND_ERR_IO;
    }
    (void)memset(&ka, 0, sizeof(ka));
    ka.keep_alive_enable = true;
    ka.keep_alive_idle = KEEPALIVE_IDLE_S;
    ka.keep_alive_interval = KEEPALIVE_INTERVAL_S;
    ka.keep_alive_count = KEEPALIVE_COUNT;
    esp_transport_tcp_set_keep_alive(s_tcp, &ka);
    return AI_BACKEND_OK;
}

static void ws_close(void *ctx)
{
    (void)ctx;
    if (s_ws != NULL) {
        (void)esp_transport_close(s_ws);   /* closes the parent socket; safe when already closed */
    }
    s_open = false;
}

static int ws_connect(void *ctx)
{
    (void)ctx;
    if (s_ws == NULL) {
        return -1;
    }
    ws_close(NULL);
    if (esp_transport_connect(s_ws, s_host, (int)s_port, CONNECT_TIMEOUT_MS) < 0) {
        ESP_LOGW(TAG, "connect to %s:%u failed", s_host, (unsigned)s_port);
        ws_close(NULL);
        return -1;
    }
    s_open = true;
    return 0;
}

static int ws_send(ws_transport_opcodes_t opcode, char *data, size_t len)
{
    int ret;

    if ((!s_open) || (len == 0U) || (len > 0x7FFFU)) {
        return -1;
    }
    ret = esp_transport_ws_send_raw(s_ws, (ws_transport_opcodes_t)((int)opcode | (int)WS_TRANSPORT_OPCODES_FIN),
                                    data, (int)len, SEND_TIMEOUT_MS);
    if (ret != (int)len) {
        /* Header and payload are two separate send() calls: a short or failed write breaks the stream. */
        ESP_LOGW(TAG, "send of %u bytes returned %d", (unsigned)len, ret);
        ws_close(NULL);
        return -1;
    }
    return 0;
}

static int ws_send_text(void *ctx, char *text, size_t len)
{
    (void)ctx;
    return ws_send(WS_TRANSPORT_OPCODES_TEXT, text, len);
}

static int ws_send_binary(void *ctx, uint8_t *data, size_t len)
{
    (void)ctx;
    return ws_send(WS_TRANSPORT_OPCODES_BINARY, (char *)data, len);
}

/*
 * Returns the length of one complete TEXT message in buf, 0 when there is none yet (or only a message of another
 * kind or a control frame), and -1 when the link is lost, a message does not fit, or the stream is broken.
 */
static int ws_recv_text(void *ctx, char *buf, size_t cap, uint32_t timeout_ms)
{
    size_t total = 0U;
    size_t remaining = 0U;      /* payload bytes of the current frame that are still to come */
    bool in_frame = false;
    bool is_text = false;
    unsigned int pass;
    int poll;

    (void)ctx;
    if ((!s_open) || (buf == NULL) || (cap < MIN_READ_ROOM)) {
        return -1;
    }
    poll = esp_transport_poll_read(s_ws, (int)timeout_ms);
    if (poll < 0) {
        ws_close(NULL);
        return -1;
    }
    if (poll == 0) {
        return 0;
    }
    for (pass = 0U; pass < (MAX_PASSES * 8U); pass++) {
        size_t room = cap - total;
        int n;

        if (room < MIN_READ_ROOM) {
            ESP_LOGW(TAG, "message of the server does not fit in %u bytes", (unsigned)cap);
            ws_close(NULL);
            return -1;
        }
        n = esp_transport_read(s_ws, &buf[total], (int)room, READ_TIMEOUT_MS);
        if (n < 0) {
            ws_close(NULL);   /* FIN from the server or a socket error */
            return -1;
        }
        if (n == 0) {
            ws_transport_opcodes_t op = esp_transport_ws_get_read_opcode(s_ws);
            int plen = esp_transport_ws_get_read_payload_len(s_ws);

            if ((int)op == (int)WS_TRANSPORT_OPCODES_CLOSE) {
                ws_close(NULL);   /* the transport already answered the close frame */
                return -1;
            }
            if (in_frame || (((int)op < 8) && (plen > 0))) {
                /* Data frame, payload not delivered within the timeout: the frame boundary is lost. */
                ESP_LOGW(TAG, "payload stalled (opcode %d, %d bytes)", (int)op, plen);
                ws_close(NULL);
                return -1;
            }
            return 0;   /* ping or pong handled inside the transport, or an empty frame */
        }
        if (!in_frame) {
            ws_transport_opcodes_t op = esp_transport_ws_get_read_opcode(s_ws);
            int plen = esp_transport_ws_get_read_payload_len(s_ws);

            if (plen < n) {
                ws_close(NULL);
                return -1;
            }
            if ((int)op == (int)WS_TRANSPORT_OPCODES_TEXT) {
                is_text = true;
            } else if ((int)op == (int)WS_TRANSPORT_OPCODES_BINARY) {
                is_text = false;
            } else if ((int)op != (int)WS_TRANSPORT_OPCODES_CONT) {
                ws_close(NULL);   /* control frames never reach here (propagate_control_frames is false) */
                return -1;
            } else {
                /* A continuation keeps the kind of the first fragment. */
            }
            remaining = (size_t)plen;
            in_frame = true;
        }
        remaining -= (size_t)n;
        if (is_text) {
            total += (size_t)n;
        }
        if (remaining > 0U) {
            continue;   /* more of this frame to come (the payload came in pieces) */
        }
        in_frame = false;
        if (esp_transport_ws_get_fin_flag(s_ws)) {
            return is_text ? (int)total : 0;   /* a BINARY message from the server is not used in v0 */
        }
    }
    ESP_LOGW(TAG, "too many fragments");
    ws_close(NULL);
    return -1;
}

void ai_ws_esp_fill_ops(ai_backend_ops_t *ops)
{
    if (ops == NULL) {
        return;
    }
    ops->ctx = NULL;
    ops->connect = ws_connect;
    ops->send_text = ws_send_text;
    ops->send_binary = ws_send_binary;
    ops->recv_text = ws_recv_text;
    ops->close = ws_close;
}