#include "AI_backend.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"

#define DEFAULT_ACK_TIMEOUT_MS  3000U
#define DEFAULT_RETRY_MIN_MS    1000U
#define DEFAULT_RETRY_MAX_MS    10000U
#define HELLO_SLICE_MS          100U     /* one wait of recv_text while the hello_ack is awaited */
#define JSON_OUT_MAX            256U
#define AUDIO_MIN_LEN           (AI_BACKEND_FRAME_HEADER + 2U)

static const char *const STOP_REASONS[] = { "touch", "vad", "timeout", "max_length" };

#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
static void be_log(const ai_backend_t *be, const char *fmt, ...)
{
    char line[160];
    va_list ap;

    if (be->cfg.ops.log == NULL) {
        return;
    }
    va_start(ap, fmt);
    (void)vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    be->cfg.ops.log(be->cfg.ops.ctx, line);
}

static uint32_t be_now(const ai_backend_t *be)
{
    return be->cfg.ops.now_ms(be->cfg.ops.ctx);
}

/* True when now is at or after target, for counters that wrap. */
static bool time_reached(uint32_t now, uint32_t target)
{
    return ((int32_t)(now - target)) >= 0;
}

static bool valid_token(const char *s)
{
    size_t i;
    size_t n;

    if (s == NULL) {
        return false;
    }
    n = strlen(s);
    if ((n == 0U) || (n >= AI_BACKEND_DEVICE_ID_MAX)) {
        return false;
    }
    for (i = 0U; i < n; i++) {
        char c = s[i];
        bool ok = ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) ||
                  (c == '-') || (c == '_') || (c == '.');

        if (!ok) {
            return false;
        }
    }
    return true;
}

/* Copies src to dst (cap bytes with the zero) and never cuts a UTF-8 character in the middle. */
static void copy_utf8(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);

    if (n >= cap) {
        n = cap - 1U;
        while ((n > 0U) && ((((uint8_t)src[n]) & 0xC0U) == 0x80U)) {
            n--;
        }
    }
    (void)memcpy(dst, src, n);
    dst[n] = '\0';
}

static void emit(const ai_backend_t *be, const ai_backend_event_t *ev)
{
    if (be->cfg.on_event != NULL) {
        be->cfg.on_event(be->cfg.event_ctx, ev);
    }
}

static void emit_simple(const ai_backend_t *be, ai_backend_event_type_t type)
{
    ai_backend_event_t ev;

    (void)memset(&ev, 0, sizeof(ev));
    ev.type = type;
    emit(be, &ev);
}

static void schedule_retry(ai_backend_t *be)
{
    be->next_try_ms = be_now(be) + be->retry_ms;
    if (be->retry_ms > (be->cfg.retry_max_ms / 2U)) {
        be->retry_ms = be->cfg.retry_max_ms;
    } else {
        be->retry_ms *= 2U;
    }
}

/* Closes the connection after a failure and starts the retry timer. Tells the app when it was ready. */
static void lose(ai_backend_t *be, const char *why)
{
    bool was_ready = (be->state == AI_BACKEND_STATE_READY);

    be->cfg.ops.close(be->cfg.ops.ctx);
    be->state = AI_BACKEND_STATE_DOWN;
    be->turn_open = false;
    be->server_ended = false;
    be->stop_valid = false;
    if (was_ready) {
        be->retry_ms = be->cfg.retry_min_ms;
        schedule_retry(be);
        be->stats.disconnects++;
        be_log(be, "[ai_be] connection lost: %s", why);
        emit_simple(be, AI_BACKEND_EVENT_DISCONNECTED);
    }
}

/* Sends a JSON text that was built in text (len bytes). Drops the connection when the send fails. */
static int send_json(ai_backend_t *be, char *text, int len)
{
    if ((len <= 0) || ((size_t)len >= JSON_OUT_MAX)) {
        return AI_BACKEND_ERR_PARAM;
    }
    if (be->cfg.ops.send_text(be->cfg.ops.ctx, text, (size_t)len) != 0) {
        lose(be, "text send failed");
        return AI_BACKEND_ERR_IO;
    }
    return AI_BACKEND_OK;
}

static const char *json_str(const cJSON *obj, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);

    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        return item->valuestring;
    }
    return NULL;
}

static bool json_u32(const cJSON *obj, const char *key, uint32_t *out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);

    if (!cJSON_IsNumber(item) || (item->valuedouble < 0.0) || (item->valuedouble > 4294967295.0)) {
        return false;
    }
    *out = (uint32_t)item->valuedouble;
    return true;
}

/* Handles the messages that come while the app runs (after the hello_ack). */
static void handle_message(ai_backend_t *be, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(be->rx, len);
    const char *type;
    ai_backend_event_t ev;

    if (root == NULL) {
        be_log(be, "[ai_be] ignored a message that is not JSON (%u bytes)", (unsigned)len);
        return;
    }
    type = json_str(root, "type");
    if (type == NULL) {
        cJSON_Delete(root);
        return;
    }
    (void)memset(&ev, 0, sizeof(ev));
    ev.has_turn = json_u32(root, "turn", &ev.turn);

    if (strcmp(type, "stt_result") == 0) {
        const cJSON *fin = cJSON_GetObjectItemCaseSensitive(root, "final");
        const char *text = json_str(root, "text");

        if (!cJSON_IsFalse(fin)) {   /* v0 uses only final results. A missing "final" counts as final */
            ev.type = AI_BACKEND_EVENT_STT_RESULT;
            copy_utf8(ev.text, sizeof(ev.text), (text != NULL) ? text : "");
            if (be->stop_valid && ev.has_turn && (ev.turn == be->stop_turn)) {
                ev.latency_ms = be_now(be) - be->stop_ms;
                be->stats.last_latency_ms = ev.latency_ms;
                be->stop_valid = false;
            }
            emit(be, &ev);
        }
    } else if (strcmp(type, "error") == 0) {
        const char *code = json_str(root, "code");

        ev.type = AI_BACKEND_EVENT_ERROR;
        copy_utf8(ev.code, sizeof(ev.code), (code != NULL) ? code : "unknown");
        if (ev.has_turn && be->turn_open && (ev.turn == be->open_turn)) {
            be->turn_open = false;   /* the server refused this turn: stop sending its audio */
        }
        if (ev.has_turn && be->stop_valid && (ev.turn == be->stop_turn)) {
            be->stop_valid = false;
        }
        emit(be, &ev);
    } else if (strcmp(type, "listen_end") == 0) {
        ev.type = AI_BACKEND_EVENT_LISTEN_END;
        if (be->turn_open && (!ev.has_turn || (ev.turn == be->open_turn))) {
            be->server_ended = true;   /* the audio stops, the app still closes the turn with listen_stop */
        }
        emit(be, &ev);
    } else {
        /* hello_ack after the start-up, or a type of a newer protocol: ignored (Backend_Protocol section 4). */
    }
    cJSON_Delete(root);
}

/* Waits for the hello_ack. Returns 0 on success. */
static int wait_hello_ack(ai_backend_t *be)
{
    uint32_t t0 = be_now(be);

    for (;;) {
        uint32_t elapsed = be_now(be) - t0;
        uint32_t slice;
        int n;
        cJSON *root;
        const char *type;
        int rc = 1;   /* 1: keep waiting */

        if (elapsed >= be->cfg.hello_ack_timeout_ms) {
            be_log(be, "[ai_be] no hello_ack within %u ms", (unsigned)be->cfg.hello_ack_timeout_ms);
            return -1;
        }
        slice = be->cfg.hello_ack_timeout_ms - elapsed;
        if (slice > HELLO_SLICE_MS) {
            slice = HELLO_SLICE_MS;
        }
        n = be->cfg.ops.recv_text(be->cfg.ops.ctx, be->rx, AI_BACKEND_RX_MAX, slice);
        if (n < 0) {
            be_log(be, "[ai_be] connection lost while waiting for hello_ack");
            return -1;
        }
        if (n == 0) {
            continue;
        }
        root = cJSON_ParseWithLength(be->rx, (size_t)n);
        if (root == NULL) {
            continue;
        }
        type = json_str(root, "type");
        if ((type != NULL) && (strcmp(type, "hello_ack") == 0)) {
            uint32_t version = 0xFFFFFFFFU;

            if (json_u32(root, "protocol_version", &version) && (version == (uint32_t)AI_BACKEND_PROTOCOL_VERSION)) {
                rc = 0;
            } else {
                be_log(be, "[ai_be] hello_ack with protocol_version %u, expected %d", (unsigned)version,
                       AI_BACKEND_PROTOCOL_VERSION);
                rc = -1;
            }
        } else if ((type != NULL) && (strcmp(type, "error") == 0)) {
            const char *code = json_str(root, "code");

            be_log(be, "[ai_be] server error during hello: %s", (code != NULL) ? code : "?");
            rc = -1;
        } else {
            /* Other messages before the hello_ack are ignored. */
        }
        cJSON_Delete(root);
        if (rc <= 0) {
            return rc;
        }
    }
}

static void try_connect(ai_backend_t *be)
{
    char hello[JSON_OUT_MAX];
    int n;

    be_log(be, "[ai_be] connecting");
    if (be->cfg.ops.connect(be->cfg.ops.ctx) != 0) {
        be_log(be, "[ai_be] connect failed, next try in %u ms", (unsigned)be->retry_ms);
        be->cfg.ops.close(be->cfg.ops.ctx);
        schedule_retry(be);
        return;
    }
    n = snprintf(hello, sizeof(hello),
                 "{\"type\":\"hello\",\"protocol_version\":%d,\"device_id\":\"%s\",\"fw\":\"%s\","
                 "\"audio\":{\"codec\":\"pcm_s16le\",\"rate\":%u,\"channels\":1,\"frame_ms\":%u}}",
                 AI_BACKEND_PROTOCOL_VERSION, be->cfg.device_id, be->cfg.fw,
                 (unsigned)be->cfg.sample_rate_hz, (unsigned)be->cfg.frame_ms);
    if ((n <= 0) || ((size_t)n >= sizeof(hello)) ||
        (be->cfg.ops.send_text(be->cfg.ops.ctx, hello, (size_t)n) != 0) || (wait_hello_ack(be) != 0)) {
        be_log(be, "[ai_be] hello failed, next try in %u ms", (unsigned)be->retry_ms);
        be->cfg.ops.close(be->cfg.ops.ctx);
        schedule_retry(be);
        return;
    }
    be->state = AI_BACKEND_STATE_READY;
    be->retry_ms = be->cfg.retry_min_ms;
    be->stats.connects++;
    be_log(be, "[ai_be] ready");
    emit_simple(be, AI_BACKEND_EVENT_CONNECTED);
}

int ai_backend_init(ai_backend_t *be, const ai_backend_config_t *cfg)
{
    if ((be == NULL) || (cfg == NULL)) {
        return AI_BACKEND_ERR_PARAM;
    }
    if ((cfg->ops.connect == NULL) || (cfg->ops.send_text == NULL) || (cfg->ops.send_binary == NULL) ||
        (cfg->ops.recv_text == NULL) || (cfg->ops.close == NULL) || (cfg->ops.now_ms == NULL)) {
        return AI_BACKEND_ERR_PARAM;
    }
    if (!valid_token(cfg->device_id) || !valid_token(cfg->fw) || (cfg->sample_rate_hz == 0U) ||
        (cfg->frame_ms == 0U) || (cfg->lang == NULL) ||
        ((strcmp(cfg->lang, "vi") != 0) && (strcmp(cfg->lang, "en") != 0))) {
        return AI_BACKEND_ERR_PARAM;
    }
    (void)memset(be, 0, sizeof(*be));
    be->cfg = *cfg;
    (void)snprintf(be->id_buf, sizeof(be->id_buf), "%s", cfg->device_id);
    (void)snprintf(be->fw_buf, sizeof(be->fw_buf), "%s", cfg->fw);
    (void)snprintf(be->lang_buf, sizeof(be->lang_buf), "%s", cfg->lang);
    be->cfg.device_id = be->id_buf;
    be->cfg.fw = be->fw_buf;
    be->cfg.lang = be->lang_buf;
    if (be->cfg.hello_ack_timeout_ms == 0U) {
        be->cfg.hello_ack_timeout_ms = DEFAULT_ACK_TIMEOUT_MS;
    }
    if (be->cfg.retry_min_ms == 0U) {
        be->cfg.retry_min_ms = DEFAULT_RETRY_MIN_MS;
    }
    if (be->cfg.retry_max_ms == 0U) {
        be->cfg.retry_max_ms = DEFAULT_RETRY_MAX_MS;
    }
    if (be->cfg.retry_max_ms < be->cfg.retry_min_ms) {
        be->cfg.retry_max_ms = be->cfg.retry_min_ms;
    }
    be->state = AI_BACKEND_STATE_DOWN;
    be->retry_ms = be->cfg.retry_min_ms;
    be->next_try_ms = be_now(be);
    return AI_BACKEND_OK;
}

void ai_backend_poll(ai_backend_t *be)
{
    int n;

    if (be == NULL) {
        return;
    }
    if (be->state != AI_BACKEND_STATE_READY) {
        if (time_reached(be_now(be), be->next_try_ms)) {
            try_connect(be);
        }
        return;
    }
    n = be->cfg.ops.recv_text(be->cfg.ops.ctx, be->rx, AI_BACKEND_RX_MAX, 0U);
    if (n < 0) {
        lose(be, "receive failed");
    } else if (n > 0) {
        handle_message(be, (size_t)n);
    } else {
        /* nothing to read */
    }
}

bool ai_backend_is_ready(const ai_backend_t *be)
{
    return (be != NULL) && (be->state == AI_BACKEND_STATE_READY);
}

int ai_backend_listen_start(ai_backend_t *be, uint32_t turn)
{
    char text[JSON_OUT_MAX];
    int n;
    int ret;

    if (be == NULL) {
        return AI_BACKEND_ERR_PARAM;
    }
    if (be->state != AI_BACKEND_STATE_READY) {
        return AI_BACKEND_ERR_STATE;
    }
    if (be->turn_open) {
        /* A turn that was never closed (lost listen_stop): abort it, so the server and this module agree again. */
        ret = ai_backend_abort(be, be->open_turn);
        if (ret != AI_BACKEND_OK) {
            return ret;
        }
    }
    n = snprintf(text, sizeof(text), "{\"type\":\"listen_start\",\"turn\":%u,\"reason\":\"touch\",\"lang\":\"%s\"}",
                 (unsigned)turn, be->cfg.lang);
    ret = send_json(be, text, n);
    if (ret != AI_BACKEND_OK) {
        return ret;
    }
    be->turn_open = true;
    be->server_ended = false;
    be->open_turn = turn;
    be->stats.turn_frames_sent = 0U;
    be->stats.turn_slow_sends = 0U;
    be->stats.turn_send_ms_max = 0U;
    be->stats.turn_send_ms_sum = 0U;
    return AI_BACKEND_OK;
}

int ai_backend_send_audio(ai_backend_t *be, uint8_t *frame, size_t len)
{
    uint32_t t0;
    uint32_t dt;

    if ((be == NULL) || (frame == NULL) || (len < AUDIO_MIN_LEN) || (len > AI_BACKEND_FRAME_MAX) ||
        (frame[0] != (uint8_t)AI_BACKEND_KIND_MIC)) {
        return AI_BACKEND_ERR_PARAM;
    }
    if ((be->state != AI_BACKEND_STATE_READY) || (!be->turn_open) || be->server_ended) {
        be->stats.frames_discarded++;
        return AI_BACKEND_ERR_STATE;
    }
    t0 = be_now(be);
    if (be->cfg.ops.send_binary(be->cfg.ops.ctx, frame, len) != 0) {
        lose(be, "audio send failed");
        return AI_BACKEND_ERR_IO;
    }
    dt = be_now(be) - t0;
    be->stats.frames_sent++;
    be->stats.send_ms_sum += dt;
    be->stats.turn_frames_sent++;
    be->stats.turn_send_ms_sum += dt;
    if (dt > be->stats.send_ms_max) {
        be->stats.send_ms_max = dt;
    }
    if (dt > be->stats.turn_send_ms_max) {
        be->stats.turn_send_ms_max = dt;
    }
    if (dt >= AI_BACKEND_SLOW_SEND_MS) {
        be->stats.slow_sends++;
        be->stats.turn_slow_sends++;
    }
    return AI_BACKEND_OK;
}

int ai_backend_listen_stop(ai_backend_t *be, uint32_t turn, const char *reason)
{
    char text[JSON_OUT_MAX];
    size_t i;
    bool known = false;
    int n;
    int ret;

    if ((be == NULL) || (reason == NULL)) {
        return AI_BACKEND_ERR_PARAM;
    }
    for (i = 0U; i < (sizeof(STOP_REASONS) / sizeof(STOP_REASONS[0])); i++) {
        if (strcmp(reason, STOP_REASONS[i]) == 0) {
            known = true;
        }
    }
    if (!known) {
        return AI_BACKEND_ERR_PARAM;
    }
    if ((be->state != AI_BACKEND_STATE_READY) || (!be->turn_open) || (turn != be->open_turn)) {
        return AI_BACKEND_ERR_STATE;
    }
    n = snprintf(text, sizeof(text), "{\"type\":\"listen_stop\",\"turn\":%u,\"reason\":\"%s\"}", (unsigned)turn, reason);
    ret = send_json(be, text, n);
    if (ret != AI_BACKEND_OK) {
        return ret;
    }
    be->turn_open = false;
    be->server_ended = false;
    be->stop_valid = true;
    be->stop_turn = turn;
    be->stop_ms = be_now(be);
    return AI_BACKEND_OK;
}

int ai_backend_abort(ai_backend_t *be, uint32_t turn)
{
    char text[JSON_OUT_MAX];
    bool open_match;
    bool wait_match;
    int n;
    int ret;

    if (be == NULL) {
        return AI_BACKEND_ERR_PARAM;
    }
    open_match = be->turn_open && (turn == be->open_turn);
    wait_match = be->stop_valid && (turn == be->stop_turn);
    if ((be->state != AI_BACKEND_STATE_READY) || (!open_match && !wait_match)) {
        return AI_BACKEND_ERR_STATE;
    }
    n = snprintf(text, sizeof(text), "{\"type\":\"abort\",\"turn\":%u}", (unsigned)turn);
    ret = send_json(be, text, n);
    if (ret != AI_BACKEND_OK) {
        return ret;
    }
    if (open_match) {
        be->turn_open = false;
        be->server_ended = false;
    } else {
        be->stop_valid = false;
    }
    return AI_BACKEND_OK;
}

void ai_backend_get_stats(const ai_backend_t *be, ai_backend_stats_t *out)
{
    if ((be == NULL) || (out == NULL)) {
        return;
    }
    *out = be->stats;
}

static bool bad_char(char c)
{
    return (((uint8_t)c) <= 0x20U) || (((uint8_t)c) == 0x7FU);
}

/* Host names and IPv4 addresses only: letters, digits, dot, hyphen, underscore. */
static bool bad_host_char(char c)
{
    bool ok = ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) ||
              (c == '.') || (c == '-') || (c == '_');

    return !ok;
}

int ai_backend_parse_url(const char *url, char *host, size_t host_cap, uint16_t *port,
                         char *path, size_t path_cap)
{
    const char *p;
    size_t hl = 0U;
    size_t i;
    uint32_t pt = 80U;

    if ((url == NULL) || (host == NULL) || (port == NULL) || (path == NULL) || (host_cap == 0U) || (path_cap == 0U)) {
        return AI_BACKEND_ERR_PARAM;
    }
    if (strncmp(url, "ws://", 5U) != 0) {
        return AI_BACKEND_ERR_PARAM;
    }
    p = url + 5;
    while ((p[hl] != '\0') && (p[hl] != ':') && (p[hl] != '/')) {
        if (bad_host_char(p[hl])) {
            return AI_BACKEND_ERR_PARAM;
        }
        hl++;
    }
    if ((hl == 0U) || (hl >= host_cap)) {
        return AI_BACKEND_ERR_PARAM;
    }
    (void)memcpy(host, p, hl);
    host[hl] = '\0';
    p += hl;
    if (*p == ':') {
        p++;
        if ((*p < '0') || (*p > '9')) {
            return AI_BACKEND_ERR_PARAM;
        }
        pt = 0U;
        while ((*p >= '0') && (*p <= '9')) {
            pt = (pt * 10U) + (uint32_t)(*p - '0');
            if (pt > 65535U) {
                return AI_BACKEND_ERR_PARAM;
            }
            p++;
        }
        if (pt == 0U) {
            return AI_BACKEND_ERR_PARAM;
        }
    }
    if (*p == '/') {
        size_t pl = strlen(p);

        if (pl >= path_cap) {
            return AI_BACKEND_ERR_PARAM;
        }
        for (i = 0U; i < pl; i++) {
            if (bad_char(p[i])) {
                return AI_BACKEND_ERR_PARAM;
            }
        }
        (void)memcpy(path, p, pl + 1U);
    } else if (*p == '\0') {
        if (path_cap < 2U) {
            return AI_BACKEND_ERR_PARAM;
        }
        path[0] = '/';
        path[1] = '\0';
    } else {
        return AI_BACKEND_ERR_PARAM;
    }
    *port = (uint16_t)pt;
    return AI_BACKEND_OK;
}