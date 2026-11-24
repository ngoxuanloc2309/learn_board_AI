#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/message_buffer.h"
#include "esp_heap_caps.h"

#include "AI_backend.h"
#include "AI_backend_ws.h"
#include "sx_system.h"
#include "sx_time.h"
#include "sx_wifi.h"
#include "AI_net_app.h"

/*
 * Messages in the MessageBuffer. Every message starts with the same 4 bytes as an audio frame on the wire.
 *   audio   : [0x01][flags 0][seq lo][seq hi] then AI_NET_FRAME_SAMPLES samples, 16 bit little endian.
 *             The net task sends this very buffer as the BINARY message (no copy).
 *   control : [kind][reason index][0][0] then turn, frames offered, frames dropped (u32 little endian each).
 */
#define MSG_KIND_AUDIO      0x01U
#define MSG_KIND_BEGIN      0x81U
#define MSG_KIND_END        0x82U
#define MSG_KIND_ABORT      0x83U
#define AUDIO_MSG_LEN       (AI_BACKEND_FRAME_HEADER + (AI_NET_FRAME_SAMPLES * 2U))
#define CTRL_MSG_LEN        16U
#define MB_LENGTH_FIELD     4U       /* the MessageBuffer stores 4 bytes of length before each message */
/*
 * Free space that audio must leave in the buffer, so that listen_stop and abort always fit: four control messages
 * of CTRL_MSG_LEN + MB_LENGTH_FIELD bytes each, rounded up. Design value.
 */
#define CONTROL_RESERVE     128U

static const char *const REASON_NAMES[] = { "touch", "vad", "timeout", "max_length" };
#define REASON_COUNT        (sizeof(REASON_NAMES) / sizeof(REASON_NAMES[0]))

/* Producer side: written by the AI_app task only. */
typedef struct {
    bool     open;          /* a turn is open (begin sent, end not yet) */
    bool     waiting;       /* the turn was ended and its stt_result is awaited */
    uint32_t turn;
    uint16_t seq;           /* next sequence number, counted for every frame given, dropped or not */
    uint32_t turn_offered;
    uint32_t turn_dropped;
    uint8_t  frame[AUDIO_MSG_LEN];
} producer_t;

static producer_t        s_prod;
static ai_net_stats_t    s_stats;
static uint8_t          *s_store;                      /* storage of the MessageBuffer, allocated once at start */
static StaticMessageBuffer_t s_mb_struct;
static MessageBufferHandle_t s_mb;
static QueueHandle_t     s_events;
static volatile bool     s_connected;                  /* written by the net task only */
static bool              s_started;

/* Net task side. */
static ai_backend_t      s_be;
static uint8_t           s_rx[AUDIO_MSG_LEN];          /* writable RAM: a frame is masked in place when it is sent */
static char              s_device_id[AI_BACKEND_DEVICE_ID_MAX];
static bool              s_waited_ip_logged;

/* ---------- helpers ---------- */

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t net_now_ms(void *ctx)
{
    uint32_t t = 0U;

    (void)ctx;
    sx_get_time_ms(&t);
    return t;
}

static void net_log(void *ctx, const char *line)
{
    (void)ctx;
    printf("%s\n", line);
}

/* Sends a control message. The reserve of audio guarantees room, but the result is still checked. */
static int send_ctrl(uint8_t kind, uint8_t reason_idx, uint32_t turn, uint32_t offered, uint32_t dropped)
{
    uint8_t msg[CTRL_MSG_LEN];

    (void)memset(msg, 0, sizeof(msg));
    msg[0] = kind;
    msg[1] = reason_idx;
    put_u32(&msg[4], turn);
    put_u32(&msg[8], offered);
    put_u32(&msg[12], dropped);
    if (xMessageBufferSend(s_mb, msg, sizeof(msg), 0) != sizeof(msg)) {
        return AI_NET_ERR_FULL;
    }
    return AI_NET_OK;
}

/* ---------- net task ---------- */

/* Event of the backend: remember the link state, then queue the event for AI_app. */
static void on_backend_event(void *ctx, const ai_backend_event_t *ev)
{
    (void)ctx;
    if (ev->type == AI_BACKEND_EVENT_CONNECTED) {
        s_connected = true;
    } else if (ev->type == AI_BACKEND_EVENT_DISCONNECTED) {
        s_connected = false;
    } else {
        /* other events do not change the link */
    }
    if (xQueueSend(s_events, ev, 0) != pdTRUE) {
        s_stats.events_lost++;
        printf("[net] event queue full, event type %d lost\n", (int)ev->type);
    }
}

static void log_turn_summary(uint32_t turn, uint32_t offered, uint32_t dropped)
{
    ai_backend_stats_t bs;
    uint32_t avg_x100 = 0U;

    ai_backend_get_stats(&s_be, &bs);
    if (bs.turn_frames_sent > 0U) {
        avg_x100 = (bs.turn_send_ms_sum * 100U) / bs.turn_frames_sent;
    }
    /*
     * offered: frames AI_app gave. dropped: not put into the buffer (full). sent: frames on the socket.
     * send_ms is the time of one send call: a send that returns at once does NOT prove that the packet left
     * (Nagle), see the server log for the arrival times. buf_min_free: lowest free space seen by AI_app.
     */
    printf("[net] turn %u: offered=%u dropped=%u sent=%u send_ms avg=%u.%02u max=%u slow(>=%u ms)=%u buf_min_free=%u of %u stack_free=%u\n",
           (unsigned)turn, (unsigned)offered, (unsigned)dropped, (unsigned)bs.turn_frames_sent,
           (unsigned)(avg_x100 / 100U), (unsigned)(avg_x100 % 100U), (unsigned)bs.turn_send_ms_max,
           (unsigned)AI_BACKEND_SLOW_SEND_MS, (unsigned)bs.turn_slow_sends,
           (unsigned)s_stats.buffer_min_free, (unsigned)APP_AI_NET_BUFFER_BYTES,
           (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

static void handle_message(uint8_t *msg, size_t len)
{
    uint32_t turn;
    int ret;

    if ((len < AI_BACKEND_FRAME_HEADER) || ((msg[0] != MSG_KIND_AUDIO) && (len < CTRL_MSG_LEN))) {
        return;
    }
    switch (msg[0]) {
    case MSG_KIND_AUDIO:
        (void)ai_backend_send_audio(&s_be, msg, len);   /* a refused frame is counted by the backend */
        break;

    case MSG_KIND_BEGIN:
        turn = get_u32(&msg[4]);
        ret = ai_backend_listen_start(&s_be, turn);
        if (ret != AI_BACKEND_OK) {
            printf("[net] turn %u: listen_start failed: %d\n", (unsigned)turn, ret);
        }
        break;

    case MSG_KIND_END:
        turn = get_u32(&msg[4]);
        if (msg[1] < REASON_COUNT) {
            ret = ai_backend_listen_stop(&s_be, turn, REASON_NAMES[msg[1]]);
            if (ret == AI_BACKEND_OK) {
                log_turn_summary(turn, get_u32(&msg[8]), get_u32(&msg[12]));
            } else {
                printf("[net] turn %u: listen_stop failed: %d\n", (unsigned)turn, ret);
            }
        }
        break;

    case MSG_KIND_ABORT:
        turn = get_u32(&msg[4]);
        ret = ai_backend_abort(&s_be, turn);
        printf("[net] turn %u: abort sent: %d\n", (unsigned)turn, ret);
        break;

    default:
        break;
    }
}

static void net_task(void *arg)
{
    (void)arg;
    for (;;) {
        size_t n = xMessageBufferReceive(s_mb, s_rx, sizeof(s_rx), pdMS_TO_TICKS(APP_AI_NET_IDLE_WAIT_MS));

        if (n > 0U) {
            handle_message(s_rx, n);
        }
        if ((!ai_backend_is_ready(&s_be)) && (sx_wifi_wait_ip(0U) != SX_WIFI_OK)) {
            /* No Wi-Fi IP yet: do not try to connect (that would only lengthen the retry wait). */
            if (!s_waited_ip_logged) {
                printf("[net] waiting for the Wi-Fi IP\n");
                s_waited_ip_logged = true;
            }
            continue;
        }
        s_waited_ip_logged = false;
        ai_backend_poll(&s_be);
    }
}

/* ---------- start ---------- */

int ai_net_start(void)
{
    ai_backend_config_t cfg;
    uint8_t mac[SX_SYSTEM_MAC_LEN];
    int ret;

    if (s_started) {
        return AI_NET_ERR_STATE;
    }
    if (sx_system_get_mac(mac) != SX_SYSTEM_OK) {
        printf("[net] cannot read the MAC address\n");
        return -1;
    }
    (void)snprintf(s_device_id, sizeof(s_device_id), "%02X%02X%02X%02X%02X%02X",
                   (unsigned)mac[0], (unsigned)mac[1], (unsigned)mac[2], (unsigned)mac[3], (unsigned)mac[4],
                   (unsigned)mac[5]);
    if (ai_ws_esp_init(APP_AI_NET_URL) != AI_BACKEND_OK) {
        printf("[net] bad URL: %s\n", APP_AI_NET_URL);
        return -1;
    }

    /* One allocation for the whole run. The storage of a MessageBuffer is its size plus one byte. */
    s_store = (uint8_t *)heap_caps_malloc(APP_AI_NET_BUFFER_BYTES + 1U, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (s_store == NULL) {
        printf("[net] no internal RAM for the %u byte buffer\n", (unsigned)APP_AI_NET_BUFFER_BYTES);
        return -1;
    }
    s_mb = xMessageBufferCreateStatic(APP_AI_NET_BUFFER_BYTES, s_store, &s_mb_struct);
    s_events = xQueueCreate(APP_AI_NET_EVENT_SLOTS, sizeof(ai_net_event_t));
    if ((s_mb == NULL) || (s_events == NULL)) {
        printf("[net] cannot create the buffer or the event queue\n");
        return -1;
    }

    (void)memset(&cfg, 0, sizeof(cfg));
    ai_ws_esp_fill_ops(&cfg.ops);
    cfg.ops.now_ms = net_now_ms;
    cfg.ops.log = net_log;
    cfg.on_event = on_backend_event;
    cfg.device_id = s_device_id;
    cfg.fw = APP_AI_NET_FW_ID;
    cfg.lang = APP_AI_NET_LANG;
    cfg.sample_rate_hz = 16000U;
    cfg.frame_ms = 20U;
    ret = ai_backend_init(&s_be, &cfg);
    if (ret != AI_BACKEND_OK) {
        printf("[net] ai_backend_init failed: %d\n", ret);
        return -1;
    }

    s_stats.buffer_min_free = APP_AI_NET_BUFFER_BYTES;
    if (xTaskCreate(net_task, APP_AI_NET_TASK_NAME, APP_AI_NET_STACK_SIZE, NULL, APP_AI_NET_TASK_PRIORITY,
                    NULL) != pdPASS) {
        printf("[net] cannot create the task\n");
        return -1;
    }
    s_started = true;
    printf("[net] started: device_id=%s url=%s buffer=%u bytes (internal RAM)\n", s_device_id, APP_AI_NET_URL,
           (unsigned)APP_AI_NET_BUFFER_BYTES);
    return AI_NET_OK;
}

/* ---------- producer API (AI_app task) ---------- */

bool ai_net_is_connected(void)
{
    return s_connected;
}

int ai_net_turn_begin(uint32_t turn)
{
    int ret;

    if ((!s_started) || (!s_connected) || s_prod.open) {
        return AI_NET_ERR_STATE;
    }
    ret = send_ctrl((uint8_t)MSG_KIND_BEGIN, 0U, turn, 0U, 0U);
    if (ret != AI_NET_OK) {
        return ret;
    }
    s_prod.open = true;
    s_prod.waiting = false;
    s_prod.turn = turn;
    s_prod.seq = 0U;
    s_prod.turn_offered = 0U;
    s_prod.turn_dropped = 0U;
    return AI_NET_OK;
}

int ai_net_turn_audio(const int16_t *pcm, size_t n)
{
    size_t free_bytes;
    size_t i;
    uint16_t seq;

    if ((pcm == NULL) || (n != AI_NET_FRAME_SAMPLES)) {
        return AI_NET_ERR_PARAM;
    }
    if (!s_prod.open) {
        return AI_NET_ERR_STATE;
    }
    seq = s_prod.seq;
    s_prod.seq++;   /* counted also when the frame is dropped below */
    s_prod.turn_offered++;
    s_stats.frames_offered++;

    free_bytes = xMessageBufferSpacesAvailable(s_mb);
    if (free_bytes < s_stats.buffer_min_free) {
        s_stats.buffer_min_free = (uint32_t)free_bytes;
    }
    if (free_bytes < (AUDIO_MSG_LEN + MB_LENGTH_FIELD + CONTROL_RESERVE)) {
        s_prod.turn_dropped++;
        s_stats.frames_dropped++;
        return AI_NET_ERR_FULL;
    }
    s_prod.frame[0] = (uint8_t)MSG_KIND_AUDIO;
    s_prod.frame[1] = 0U;   /* flags: not used in v0 */
    s_prod.frame[2] = (uint8_t)(seq & 0xFFU);
    s_prod.frame[3] = (uint8_t)(seq >> 8);
    for (i = 0U; i < n; i++) {
        uint16_t v = (uint16_t)pcm[i];

        s_prod.frame[AI_BACKEND_FRAME_HEADER + (2U * i)] = (uint8_t)(v & 0xFFU);
        s_prod.frame[AI_BACKEND_FRAME_HEADER + (2U * i) + 1U] = (uint8_t)(v >> 8);
    }
    if (xMessageBufferSend(s_mb, s_prod.frame, AUDIO_MSG_LEN, 0) != AUDIO_MSG_LEN) {
        s_prod.turn_dropped++;
        s_stats.frames_dropped++;
        return AI_NET_ERR_FULL;
    }
    return AI_NET_OK;
}

int ai_net_turn_end(uint32_t turn, const char *reason)
{
    size_t i;
    int idx = -1;
    int ret;

    if (reason == NULL) {
        return AI_NET_ERR_PARAM;
    }
    for (i = 0U; i < REASON_COUNT; i++) {
        if (strcmp(reason, REASON_NAMES[i]) == 0) {
            idx = (int)i;
        }
    }
    if (idx < 0) {
        return AI_NET_ERR_PARAM;
    }
    if ((!s_prod.open) || (turn != s_prod.turn)) {
        return AI_NET_ERR_STATE;
    }
    ret = send_ctrl((uint8_t)MSG_KIND_END, (uint8_t)idx, turn, s_prod.turn_offered, s_prod.turn_dropped);
    s_prod.open = false;                 /* closed even when the message did not fit, so the next turn can start */
    s_prod.waiting = (ret == AI_NET_OK);
    return ret;
}

int ai_net_turn_abort(uint32_t turn)
{
    int ret;

    if (!(((s_prod.open) || (s_prod.waiting)) && (turn == s_prod.turn))) {
        return AI_NET_ERR_STATE;
    }
    ret = send_ctrl((uint8_t)MSG_KIND_ABORT, 0U, turn, 0U, 0U);
    s_prod.open = false;
    s_prod.waiting = false;
    return ret;
}

int ai_net_poll_event(ai_net_event_t *ev)
{
    if ((ev == NULL) || (s_events == NULL)) {
        return 0;
    }
    return (xQueueReceive(s_events, ev, 0) == pdTRUE) ? 1 : 0;
}

void ai_net_get_stats(ai_net_stats_t *stats)
{
    if (stats != NULL) {
        *stats = s_stats;
    }
}