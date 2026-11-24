#ifndef AI_BACKEND_H
#define AI_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Protocol side of the voice link of docs/Backend_Protocol.md (sections 4, 5 and 8): the hello and hello_ack
 * exchange, the turn messages (listen_start, listen_stop, abort), the audio frames and the answers of the server
 * (stt_result, error, listen_end). It knows nothing about sockets, tasks or the chip: the transport is given as
 * a table of functions (ai_backend_ops_t), so the same code runs on the board (AI_backend_ws.c) and on a host
 * test with a fake transport.
 *
 * Threading: one ai_backend_t object belongs to ONE task (the net task). No function here is thread safe.
 * Nothing here blocks except wait_hello_ack inside ai_backend_poll (at most hello_ack_timeout_ms) and the
 * transport calls themselves.
 */

#define AI_BACKEND_OK           0
#define AI_BACKEND_ERR_PARAM    (-1)
#define AI_BACKEND_ERR_STATE    (-2)   /* not connected, no such turn, or the turn is already closed */
#define AI_BACKEND_ERR_IO       (-3)   /* the transport failed; the connection was closed and a retry is scheduled */

/* Version in hello and hello_ack (v0). */
#define AI_BACKEND_PROTOCOL_VERSION   0

/* Audio frame on the wire: 4 byte header then PCM (Backend_Protocol section 5). */
#define AI_BACKEND_FRAME_HEADER   4U
#define AI_BACKEND_KIND_MIC       0x01U
/* Largest frame accepted by ai_backend_send_audio: header plus 20 ms of 16 kHz, 16 bit mono (640 bytes). */
#define AI_BACKEND_FRAME_MAX      (AI_BACKEND_FRAME_HEADER + 640U)

/*
 * Size of the buffer for one message of the server (JSON text). Design value, not measured. It is also the
 * WS_BUFFER_SIZE of this project's sdkconfig (1024), because the ws layer of ESP-IDF reads the frame header
 * into the caller's buffer. A message that does not fit is a protocol error and closes the connection.
 */
#define AI_BACKEND_RX_MAX         1024U

/* Longest recognised text kept in an event (UTF-8 bytes, with the zero). Longer text is cut at a character. */
#define AI_BACKEND_TEXT_MAX       240U
/* Longest error code kept in an event. */
#define AI_BACKEND_CODE_MAX       24U

/* device_id and fw must be 1 to AI_BACKEND_DEVICE_ID_MAX - 1 characters of letters, digits, '-', '_' or '.'. */
#define AI_BACKEND_DEVICE_ID_MAX  33U

/*
 * A send call that takes at least this long counts as slow (one audio frame is 20 ms of audio, so a send
 * that lasts a frame period means the sender cannot keep up). Design value.
 */
#define AI_BACKEND_SLOW_SEND_MS   20U

typedef enum {
    AI_BACKEND_STATE_DOWN = 0,
    AI_BACKEND_STATE_READY
} ai_backend_state_t;

typedef enum {
    AI_BACKEND_EVENT_NONE = 0,
    AI_BACKEND_EVENT_CONNECTED,      /* hello_ack received, turns can start */
    AI_BACKEND_EVENT_DISCONNECTED,   /* the link was lost while READY; the module retries by itself */
    AI_BACKEND_EVENT_STT_RESULT,     /* text (final) of a turn */
    AI_BACKEND_EVENT_ERROR,          /* code (and turn, when has_turn) */
    AI_BACKEND_EVENT_LISTEN_END      /* the server has enough audio: the audio of the open turn stops */
} ai_backend_event_type_t;

typedef struct {
    ai_backend_event_type_t type;
    bool     has_turn;                       /* turn is valid */
    uint32_t turn;
    uint32_t latency_ms;                     /* STT_RESULT: time from listen_stop to this result, 0 if unknown */
    char     text[AI_BACKEND_TEXT_MAX];      /* STT_RESULT: UTF-8, NFC (the server sends NFC) */
    char     code[AI_BACKEND_CODE_MAX];      /* ERROR: code of Backend_Protocol section 8 */
} ai_backend_event_t;

/*
 * Transport functions. ctx is passed back unchanged. All of them run in the net task.
 *   connect     : opens the link and finishes the WebSocket handshake. 0 on success, other on failure.
 *   send_text   : sends len bytes as one TEXT message. text may be modified by the transport (masking) and
 *                 must be writable. 0 on success.
 *   send_binary : same for a BINARY message. 0 on success.
 *   recv_text   : waits up to timeout_ms (0: only look) for one complete TEXT message and copies it to buf
 *                 (cap bytes). Returns its length, 0 when there is none, negative when the link is lost or a
 *                 message does not fit.
 *   close       : closes the link (safe to call when it is already closed).
 *   now_ms      : millisecond counter (wraps; the module only subtracts).
 *   log         : optional, one line without a newline.
 */
typedef struct {
    void *ctx;
    int (*connect)(void *ctx);
    int (*send_text)(void *ctx, char *text, size_t len);
    int (*send_binary)(void *ctx, uint8_t *data, size_t len);
    int (*recv_text)(void *ctx, char *buf, size_t cap, uint32_t timeout_ms);
    void (*close)(void *ctx);
    uint32_t (*now_ms)(void *ctx);
    void (*log)(void *ctx, const char *line);
} ai_backend_ops_t;

typedef void (*ai_backend_event_cb_t)(void *ctx, const ai_backend_event_t *ev);

typedef struct {
    ai_backend_ops_t      ops;
    ai_backend_event_cb_t on_event;          /* called from inside ai_backend_* calls, same task */
    void                 *event_ctx;
    const char           *device_id;         /* copied by ai_backend_init */
    const char           *fw;                /* copied */
    const char           *lang;              /* "vi" or "en", copied */
    uint32_t              sample_rate_hz;    /* goes into hello (16000) */
    uint32_t              frame_ms;          /* goes into hello (20) */
    uint32_t              hello_ack_timeout_ms;   /* 0: 3000 */
    uint32_t              retry_min_ms;           /* 0: 1000. Wait before the first retry */
    uint32_t              retry_max_ms;           /* 0: 10000. The wait doubles up to this */
} ai_backend_config_t;

typedef struct {
    uint32_t connects;
    uint32_t disconnects;
    uint32_t frames_sent;
    uint32_t frames_discarded;     /* audio refused because no turn was open or the link was down */
    uint32_t slow_sends;
    uint32_t send_ms_max;
    uint32_t send_ms_sum;
    uint32_t last_latency_ms;      /* listen_stop to stt_result of the last answered turn */
    uint32_t turn_frames_sent;     /* the same counters for the current turn, cleared by listen_start */
    uint32_t turn_slow_sends;
    uint32_t turn_send_ms_max;
    uint32_t turn_send_ms_sum;
} ai_backend_stats_t;

/* Object. Fields are private to AI_backend.c; the type is public only so that the caller can place it in static memory. */
typedef struct {
    ai_backend_config_t cfg;
    ai_backend_state_t  state;
    uint32_t retry_ms;
    uint32_t next_try_ms;
    bool     turn_open;        /* listen_start sent, listen_stop (or abort) not yet */
    bool     server_ended;     /* listen_end received: audio is refused, the app still sends listen_stop */
    uint32_t open_turn;
    bool     stop_valid;       /* listen_stop sent and no stt_result or error yet */
    uint32_t stop_turn;
    uint32_t stop_ms;
    ai_backend_stats_t stats;
    char     id_buf[AI_BACKEND_DEVICE_ID_MAX];
    char     fw_buf[AI_BACKEND_DEVICE_ID_MAX];
    char     lang_buf[3];
    char     rx[AI_BACKEND_RX_MAX];
} ai_backend_t;

/* Validates cfg (all functions but log, a valid device_id and fw, lang "vi" or "en", rates not 0). Starts DOWN. */
int ai_backend_init(ai_backend_t *be, const ai_backend_config_t *cfg);

/*
 * Call it often (the net task calls it every pass). When DOWN and the retry time has come it connects
 * (blocks for the connect and for at most hello_ack_timeout_ms). When READY it reads at most one message
 * of the server and turns it into an event. Connection loss and failures are handled inside.
 */
void ai_backend_poll(ai_backend_t *be);

bool ai_backend_is_ready(const ai_backend_t *be);

/*
 * Sends listen_start for the turn. AI_BACKEND_ERR_STATE when not ready. A turn that is still open (its
 * listen_stop never went out) is aborted first, so there is never more than one open turn.
 */
int ai_backend_listen_start(ai_backend_t *be, uint32_t turn);

/*
 * Sends one audio frame of the open turn: frame is header (kind 0x01, flags, sequence uint16 little endian) and
 * PCM, len bytes. The buffer must be writable (the transport masks it in place). AI_BACKEND_ERR_STATE when no
 * turn is open, the server sent listen_end, or the link is down (counted in frames_discarded).
 */
int ai_backend_send_audio(ai_backend_t *be, uint8_t *frame, size_t len);

/* Sends listen_stop. reason is "touch", "vad", "timeout" or "max_length". The turn must be the open one. */
int ai_backend_listen_stop(ai_backend_t *be, uint32_t turn, const char *reason);

/* Sends abort for the open turn or for the turn that waits for its stt_result. */
int ai_backend_abort(ai_backend_t *be, uint32_t turn);

void ai_backend_get_stats(const ai_backend_t *be, ai_backend_stats_t *out);

/*
 * Splits a URL "ws://host[:port][/path]" into its parts. Port defaults to 80, path to "/". "wss://" is refused
 * (TLS is not done in this step). Host: letters, digits, '.', '-', '_'. No query string, no spaces or control
 * characters. Returns AI_BACKEND_OK or AI_BACKEND_ERR_PARAM. Pure function, also used by the host test.
 */
int ai_backend_parse_url(const char *url, char *host, size_t host_cap, uint16_t *port, char *path, size_t path_cap);

#ifdef __cplusplus
}
#endif

#endif /* AI_BACKEND_H */