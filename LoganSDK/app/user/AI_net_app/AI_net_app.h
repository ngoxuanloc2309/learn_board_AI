#ifndef AI_NET_APP_H
#define AI_NET_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "AI_backend.h"

/*
 * Step 3 of docs/Backend_Protocol.md section 13: the net task. It owns the WebSocket link to the server (through
 * services/AI), so that drawing on the screen or a slow send never makes the microphone task lose audio.
 *
 *   AI_app task (producer)                            net task (consumer)
 *   ai_net_turn_begin / _audio / _end / _abort  --->  FreeRTOS MessageBuffer  --->  AI_backend (socket)
 *   ai_net_poll_event                           <---  FreeRTOS queue of events  <---  messages of the server
 *
 * Who owns what (nothing else is shared):
 *  - The MessageBuffer is written only by the AI_app task and read only by the net task (one writer, one reader,
 *    as FreeRTOS requires for a MessageBuffer).
 *  - The event queue is written only by the net task and read only by the AI_app task.
 *  - The flag behind ai_net_is_connected is written only by the net task (a bool, so a read never sees half of it).
 *  - The producer state (turn, sequence number, counters) is written and read only by the AI_app task. The
 *    sequence number is counted for every frame AI_app gives, also for a frame that has to be dropped, so the
 *    server sees the gap.
 *  - ai_net_get_stats reads counters that the AI_app task writes: call it from that task.
 */

#define AI_NET_OK          0
#define AI_NET_ERR_PARAM   (-1)
#define AI_NET_ERR_STATE   (-2)   /* not connected, no such turn, or the net task is not started */
#define AI_NET_ERR_FULL    (-3)   /* the buffer to the net task has no room (an audio frame is dropped) */

/* Samples of one audio frame: 20 ms at 16 kHz. AI_app checks at build time that its frame has this size. */
#define AI_NET_FRAME_SAMPLES   320U

typedef ai_backend_event_t ai_net_event_t;

typedef struct {
    uint32_t frames_offered;     /* frames given by AI_app, all turns */
    uint32_t frames_dropped;     /* of them not put into the buffer (buffer full) */
    uint32_t buffer_min_free;    /* lowest free space of the buffer seen, in bytes (the buffer size at the start) */
    uint32_t events_lost;        /* events dropped because the event queue was full */
} ai_net_stats_t;

#ifndef APP_AI_NET_URL
#define APP_AI_NET_URL "ws://192.168.30.101:8765/v1/voice"   /* GIVEN by the owner (session 26); DHCP, may change */
#endif

#ifndef APP_AI_NET_FW_ID
#define APP_AI_NET_FW_ID "step3"
#endif

#ifndef APP_AI_NET_LANG
#define APP_AI_NET_LANG "vi"
#endif

#ifndef APP_AI_NET_TASK_NAME
#define APP_AI_NET_TASK_NAME "app_ai_net"
#endif

/* Chosen by design, not measured: the task prints its lowest free stack after each turn. */
#ifndef APP_AI_NET_STACK_SIZE
#define APP_AI_NET_STACK_SIZE 6144
#endif

/* Above app_ai_task (2), so that the buffer is emptied at once. Chosen by design. */
#ifndef APP_AI_NET_TASK_PRIORITY
#define APP_AI_NET_TASK_PRIORITY 3
#endif

/*
 * Size of the buffer between the two tasks, in bytes. One audio frame takes 648 bytes (644 and 4 of length), so
 * 16384 holds about 25 frames, 0.5 s. The owner approved 16 KB (session 26). Jitter was not measured: the log
 * line "buf_min_free" at the end of each turn tells how much of it was used.
 */
#ifndef APP_AI_NET_BUFFER_BYTES
#define APP_AI_NET_BUFFER_BYTES 16384U
#endif

/* Slots of the event queue to AI_app. */
#ifndef APP_AI_NET_EVENT_SLOTS
#define APP_AI_NET_EVENT_SLOTS 8U
#endif

/* How long the net task waits for a message of AI_app before it looks at the socket (one tick at 100 Hz). */
#ifndef APP_AI_NET_IDLE_WAIT_MS
#define APP_AI_NET_IDLE_WAIT_MS 10U
#endif

/*
 * Reads the MAC (device_id), checks the URL, allocates the buffer once (internal RAM, 8 bit access), creates the
 * queue and the task. The task waits for the Wi-Fi IP by itself, then connects, sends hello, and keeps the link up.
 * Returns AI_NET_OK, AI_NET_ERR_STATE when already started, -1 on failure. Call once.
 */
int ai_net_start(void);

/* True between the hello_ack and the loss of the connection. */
bool ai_net_is_connected(void);

/* Starts a turn: sends listen_start. AI_NET_ERR_STATE when not connected or a turn is still open. */
int ai_net_turn_begin(uint32_t turn);

/*
 * Gives one audio frame of the open turn: n must be AI_NET_FRAME_SAMPLES samples, already filtered. The frame is
 * copied, so pcm may be reused at once. AI_NET_ERR_FULL when the buffer has no room (dropped and counted).
 */
int ai_net_turn_audio(const int16_t *pcm, size_t n);

/* Closes the turn: sends listen_stop. reason is "touch", "vad", "timeout" or "max_length". Never dropped. */
int ai_net_turn_end(uint32_t turn, const char *reason);

/* Cancels the open turn, or the one that waits for its stt_result: sends abort. */
int ai_net_turn_abort(uint32_t turn);

/* Takes one event of the net task. Returns 1 and fills ev, or 0 when there is none. */
int ai_net_poll_event(ai_net_event_t *ev);

void ai_net_get_stats(ai_net_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* AI_NET_APP_H */