#ifndef HTTP_SERVICE_H
#define HTTP_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define HTTP_SVC_OK            0
#define HTTP_SVC_ERR_PARAM     (-1)
#define HTTP_SVC_ERR_NET       (-2)  /* connect, TLS or transfer failed, or the link is down */
#define HTTP_SVC_ERR_TIMEOUT   (-3)
#define HTTP_SVC_ERR_NOMEM     (-4)
#define HTTP_SVC_ERR_OVERFLOW  (-5)  /* body larger than the buffer, see http_svc_perform */

/* Used when timeout_ms is 0. Chosen by design, not measured. */
#define HTTP_SVC_TIMEOUT_MS_DEFAULT  10000U

typedef enum {
    HTTP_SVC_GET,
    HTTP_SVC_POST,
    HTTP_SVC_PUT,
    HTTP_SVC_DELETE
} http_svc_method_t;

typedef struct {
    const char *name;
    const char *value;
} http_svc_header_t;

/*
 * One blocking request. All pointers stay owned by the caller and are only used during the call.
 *
 * url: http:// or https://. Plain http is not encrypted, use it only for development.
 * For https the server certificate is always checked. By default the CA bundle built into
 * the firmware is used. ca_cert_pem (NULL terminated PEM text) replaces it, for example for
 * a private backend. There is no way to turn the check off.
 *
 * headers and header_count: extra request headers. Content-Length is handled by the service.
 * body and body_len: request body, allowed for POST and PUT only (GET and DELETE need body_len 0).
 *
 * response and response_capacity: buffer for the response body. When response is NULL the body
 * is ignored and response_capacity must be 0. When the body is shorter than the buffer a
 * terminating zero byte is written after it (not counted in body_len), so size the buffer
 * one byte more than the largest body you expect if you need a C string.
 *
 * timeout_ms: network timeout of each connect, send and receive step, not of the whole request.
 * 0 selects HTTP_SVC_TIMEOUT_MS_DEFAULT.
 */
typedef struct {
    http_svc_method_t method;
    const char *url;
    const http_svc_header_t *headers;
    size_t header_count;
    const void *body;
    size_t body_len;
    const char *ca_cert_pem;
    void *response;
    size_t response_capacity;
    uint32_t timeout_ms;
} http_svc_request_t;

/*
 * status is the HTTP status code, 0 when no response was received.
 * body_len is the number of body bytes stored in the buffer.
 */
typedef struct {
    int status;
    size_t body_len;
} http_svc_response_t;

/*
 * Runs the request and waits for the answer. Any HTTP status that the server returns is a
 * success of this call (HTTP_SVC_OK), so the caller must check response->status.
 * Redirects are not followed: a 3xx status is returned as it is.
 * A response body needs a Content-Length header or chunked encoding. A body that only ends when
 * the server closes the connection is reported as HTTP_SVC_ERR_NET.
 *
 * Returns HTTP_SVC_ERR_OVERFLOW when the body does not fit. status and the first
 * response_capacity bytes are still valid in that case.
 * A failed connect, a TLS certificate failure and a closed link all return HTTP_SVC_ERR_NET,
 * because the chip driver does not tell them apart.
 *
 * The call blocks the calling task, so never call it from the LVGL task. It keeps no state
 * between calls, so several tasks may use it, but each call uses heap memory for the
 * connection (more with https). It does not check or start the Wi-Fi link.
 */
int http_svc_perform(const http_svc_request_t *request, http_svc_response_t *response);

#ifdef __cplusplus
}
#endif

#endif /* HTTP_SERVICE_H */