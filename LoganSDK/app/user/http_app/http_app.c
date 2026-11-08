#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portable.h"
#include "freertos/task.h"

#include "http.h"
#include "http_app.h"
#include "sx_time.h"
#include "sx_wifi.h"

/* How many body characters are printed. */
#define BODY_PREVIEW_CHARS   60U

/* The wait message is printed once per this many polls, so the console stays quiet. */
#define WAIT_PRINT_POLLS     10U

/* Static, so the task stack stays small. Only the HTTP task uses it. */
static uint8_t s_body[APP_HTTP_BODY_SIZE];

static const char *result_name(int result)
{
    switch (result) {
    case HTTP_SVC_OK:
        return "ok";
    case HTTP_SVC_ERR_PARAM:
        return "param";
    case HTTP_SVC_ERR_NET:
        return "net";
    case HTTP_SVC_ERR_TIMEOUT:
        return "timeout";
    case HTTP_SVC_ERR_NOMEM:
        return "nomem";
    case HTTP_SVC_ERR_OVERFLOW:
        return "overflow";
    default:
        return "unknown";
    }
}

static bool wifi_has_ip(void)
{
    sx_wifi_status_t status;

    /* Before wifi_app has finished the init this call fails, which counts as no IP yet. */
    if (sx_wifi_get_status(&status) != SX_WIFI_OK) {
        return false;
    }
    return status.sta_state == SX_WIFI_STA_GOT_IP;
}

/* Prints the first characters of the body. Bytes that are not printable ASCII are shown as dots. */
static void print_preview(const uint8_t *body, size_t length)
{
    char text[BODY_PREVIEW_CHARS + 1U];
    size_t count = length;
    size_t index;

    if (count > sizeof(s_body)) {
        count = sizeof(s_body);
    }
    if (count > BODY_PREVIEW_CHARS) {
        count = BODY_PREVIEW_CHARS;
    }
    for (index = 0U; index < count; index++) {
        uint8_t value = body[index];

        text[index] = ((value >= 32U) && (value <= 126U)) ? (char)value : '.';
    }
    text[count] = '\0';
    printf("[http] body starts: \"%s\"\n", text);
}

static void http_task(void *arg)
{
    http_svc_request_t request;
    http_svc_response_t response;
    uint32_t start_ms = 0U;
    uint32_t end_ms = 0U;
    size_t heap_before;
    size_t heap_after;
    size_t low_before;
    size_t low_after;
    unsigned polls = 0U;
    int result;

    (void)arg;

    printf("[http] waiting for a Wi-Fi IP\n");
    while (!wifi_has_ip()) {
        polls++;
        if ((polls % WAIT_PRINT_POLLS) == 0U) {
            printf("[http] still waiting for a Wi-Fi IP\n");
        }
        sx_delay_ms(APP_HTTP_WAIT_POLL_MS);
    }

    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    request.method = HTTP_SVC_GET;
    request.url = APP_HTTP_TEST_URL;
    request.response = s_body;
    request.response_capacity = sizeof(s_body);

    printf("[http] GET %s\n", APP_HTTP_TEST_URL);
    heap_before = xPortGetFreeHeapSize();
    low_before = xPortGetMinimumEverFreeHeapSize();
    sx_get_time_ms(&start_ms);

    result = http_svc_perform(&request, &response);

    sx_get_time_ms(&end_ms);
    heap_after = xPortGetFreeHeapSize();
    low_after = xPortGetMinimumEverFreeHeapSize();

    /* Time is measured by subtraction, so it stays correct when the counter wraps. */
    printf("[http] result=%d (%s) status=%d body=%u bytes time=%lu ms\n", result,
           result_name(result), response.status, (unsigned)response.body_len,
           (unsigned long)(end_ms - start_ms));
    printf("[http] heap free before=%u after=%u, lowest ever before=%u after=%u\n",
           (unsigned)heap_before, (unsigned)heap_after, (unsigned)low_before,
           (unsigned)low_after);
    printf("[http] stack lowest free %u of %u bytes\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)APP_HTTP_STACK_SIZE);

    if ((result == HTTP_SVC_OK) || (result == HTTP_SVC_ERR_OVERFLOW)) {
        print_preview(s_body, response.body_len);
    }

    vTaskDelete(NULL);
}

int http_app_start(void)
{
    if (xTaskCreate(http_task, APP_HTTP_TASK_NAME, APP_HTTP_STACK_SIZE, NULL,
                    APP_HTTP_TASK_PRIORITY, NULL) != pdPASS) {
        printf("[http] cannot create the task\n");
        return -1;
    }
    return 0;
}