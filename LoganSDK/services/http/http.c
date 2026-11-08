#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"

#include "http.h"

static int map_error(esp_err_t error)
{
    switch (error) {
    case ESP_OK:
        return HTTP_SVC_OK;
    case ESP_ERR_NO_MEM:
        return HTTP_SVC_ERR_NOMEM;
    case ESP_ERR_TIMEOUT:
    case ESP_ERR_HTTP_EAGAIN:
    case ESP_ERR_HTTP_READ_TIMEOUT:
    case ESP_ERR_HTTP_CONNECTING:
        return HTTP_SVC_ERR_TIMEOUT;
    default:
        return HTTP_SVC_ERR_NET;
    }
}

/* Read and fetch_headers return a negative esp_err_t value, or -1 for a general failure. */
static int map_negative(int64_t value)
{
    if (value == -(int64_t)ESP_ERR_HTTP_EAGAIN) {
        return HTTP_SVC_ERR_TIMEOUT;
    }
    return HTTP_SVC_ERR_NET;
}

static bool method_to_driver(http_svc_method_t method, esp_http_client_method_t *out)
{
    switch (method) {
    case HTTP_SVC_GET:
        *out = HTTP_METHOD_GET;
        return true;
    case HTTP_SVC_POST:
        *out = HTTP_METHOD_POST;
        return true;
    case HTTP_SVC_PUT:
        *out = HTTP_METHOD_PUT;
        return true;
    case HTTP_SVC_DELETE:
        *out = HTTP_METHOD_DELETE;
        return true;
    default:
        return false;
    }
}

static bool starts_with(const char *text, const char *prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static bool valid_request(const http_svc_request_t *request, const http_svc_response_t *response,
                          esp_http_client_method_t *method)
{
    size_t i;
    bool has_body_method;

    if ((request == NULL) || (response == NULL) || (request->url == NULL) ||
        !method_to_driver(request->method, method)) {
        return false;
    }
    if (!starts_with(request->url, "http://") && !starts_with(request->url, "https://")) {
        return false;
    }

    if ((request->headers == NULL) != (request->header_count == 0U)) {
        return false;
    }
    for (i = 0U; i < request->header_count; i++) {
        if ((request->headers[i].name == NULL) || (request->headers[i].name[0] == '\0') ||
            (request->headers[i].value == NULL)) {
            return false;
        }
    }

    has_body_method = (request->method == HTTP_SVC_POST) || (request->method == HTTP_SVC_PUT);
    if ((request->body == NULL) != (request->body_len == 0U)) {
        return false;
    }
    if (!has_body_method && (request->body_len != 0U)) {
        return false;
    }
    if (request->body_len > (size_t)INT_MAX) {
        return false;
    }

    if ((request->response == NULL) != (request->response_capacity == 0U)) {
        return false;
    }
    return request->timeout_ms <= (uint32_t)INT_MAX;
}

static int send_body(esp_http_client_handle_t client, const char *body, size_t body_len)
{
    size_t sent = 0U;

    while (sent < body_len) {
        int chunk = esp_http_client_write(client, body + sent, (int)(body_len - sent));

        if (chunk < 0) {
            return map_negative(chunk);
        }
        if (chunk == 0) {
            return HTTP_SVC_ERR_NET;
        }
        sent += (size_t)chunk;
    }
    return HTTP_SVC_OK;
}

/*
 * Reads the body into the buffer. When the buffer fills up and the server still has data,
 * the result is HTTP_SVC_ERR_OVERFLOW.
 */
static int read_body(esp_http_client_handle_t client, char *buffer, size_t capacity,
                     size_t *stored)
{
    size_t total = 0U;
    int result = HTTP_SVC_OK;

    while (total < capacity) {
        size_t room = capacity - total;
        int got = esp_http_client_read(client, buffer + total,
                                       (room > (size_t)INT_MAX) ? INT_MAX : (int)room);

        if (got < 0) {
            result = map_negative(got);
            break;
        }
        if (got == 0) {
            /* No more data. It is only a clean end when the whole body arrived. */
            if (!esp_http_client_is_complete_data_received(client)) {
                result = HTTP_SVC_ERR_NET;
            }
            break;
        }
        total += (size_t)got;
    }

    if ((result == HTTP_SVC_OK) && (total == capacity) &&
        !esp_http_client_is_complete_data_received(client)) {
        char probe;
        int got = esp_http_client_read(client, &probe, 1);

        if (got > 0) {
            result = HTTP_SVC_ERR_OVERFLOW;
        } else if (got < 0) {
            result = map_negative(got);
        } else if (!esp_http_client_is_complete_data_received(client)) {
            result = HTTP_SVC_ERR_NET;
        }
    }

    if (total < capacity) {
        buffer[total] = '\0';
    }
    *stored = total;
    return result;
}

int http_svc_perform(const http_svc_request_t *request, http_svc_response_t *response)
{
    esp_http_client_config_t config;
    esp_http_client_handle_t client;
    esp_http_client_method_t method;
    int64_t header_result = 0;
    esp_err_t err;
    int result;
    size_t i;

    if (response != NULL) {
        response->status = 0;
        response->body_len = 0U;
    }
    if (!valid_request(request, response, &method)) {
        return HTTP_SVC_ERR_PARAM;
    }

    memset(&config, 0, sizeof(config));
    config.url = request->url;
    config.method = method;
    config.timeout_ms = (request->timeout_ms == 0U) ? (int)HTTP_SVC_TIMEOUT_MS_DEFAULT
                                                    : (int)request->timeout_ms;
    config.disable_auto_redirect = true;
    if (starts_with(request->url, "https://")) {
        if (request->ca_cert_pem != NULL) {
            config.cert_pem = request->ca_cert_pem;
        } else {
            config.crt_bundle_attach = esp_crt_bundle_attach;
        }
    }

    client = esp_http_client_init(&config);
    if (client == NULL) {
        return HTTP_SVC_ERR_NOMEM;
    }

    result = HTTP_SVC_OK;
    for (i = 0U; (i < request->header_count) && (result == HTTP_SVC_OK); i++) {
        err = esp_http_client_set_header(client, request->headers[i].name,
                                         request->headers[i].value);
        result = map_error(err);
    }

    if (result == HTTP_SVC_OK) {
        err = esp_http_client_open(client, (int)request->body_len);
        result = map_error(err);
    }
    if ((result == HTTP_SVC_OK) && (request->body_len > 0U)) {
        result = send_body(client, (const char *)request->body, request->body_len);
    }
    if (result == HTTP_SVC_OK) {
        header_result = esp_http_client_fetch_headers(client);
        if (header_result < 0) {
            result = map_negative(header_result);
        }
    }

    if (result == HTTP_SVC_OK) {
        response->status = esp_http_client_get_status_code(client);
        if (request->response != NULL) {
            result = read_body(client, (char *)request->response, request->response_capacity,
                               &response->body_len);
        }
    }

    (void)esp_http_client_close(client);
    (void)esp_http_client_cleanup(client);
    return result;
}