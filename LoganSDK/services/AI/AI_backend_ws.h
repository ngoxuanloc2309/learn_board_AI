#ifndef AI_BACKEND_WS_H
#define AI_BACKEND_WS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "AI_backend.h"

/*
 * Transport of ai_backend on the tcp_transport component of ESP-IDF v5.4.3 (a TCP transport as parent and a
 * WebSocket transport on it). Plain "ws://" only. One link per firmware: the state is static.
 *
 * Facts taken from the ESP-IDF v5.4.3 source (components/tcp_transport/transport_ws.c), see Backend_Protocol
 * section 3 for the full list:
 *  - esp_transport_destroy does not destroy the parent transport. Both are created once here and are never
 *    destroyed (there is no deinit). A reconnect closes and connects the same two objects again.
 *  - The frame that is sent is masked in place, so the caller's buffer must be writable RAM.
 *  - esp_transport_read returns 0 for a timeout and also after it handled a ping, pong or close frame by itself.
 *    A timeout in the middle of a payload loses the frame boundary of the stream, so this module reads with a
 *    long timeout once a frame has started and treats such a stall as a lost connection.
 *  - The transport does not join fragments: this module does, for TEXT messages up to the size of the buffer.
 *  - Nagle: tcp_transport does not set TCP_NODELAY and gives no public way to set it. Measure the arrival
 *    times at the server (backend/server.py logs the gaps) before deciding that anything must change.
 */

/* Splits and keeps the URL, creates the two transports once. AI_BACKEND_OK, or AI_BACKEND_ERR_PARAM (bad URL, or
 * called twice) or AI_BACKEND_ERR_IO (no memory). Call before ai_backend_init and before the net task runs. */
int ai_ws_esp_init(const char *url);

/* Fills ops with the functions of this transport (ctx NULL). now_ms and log are left for the caller. */
void ai_ws_esp_fill_ops(ai_backend_ops_t *ops);

#ifdef __cplusplus
}
#endif

#endif /* AI_BACKEND_WS_H */