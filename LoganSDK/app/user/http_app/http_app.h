#ifndef HTTP_APP_H
#define HTTP_APP_H

#ifndef APP_HTTP_TASK_NAME
#define APP_HTTP_TASK_NAME "app_http_task"
#endif

/*
 * Stack in bytes. The TLS handshake runs inside this task, so it needs much more than a plain
 * task. Chosen by design, not measured. The task prints the lowest free stack after the request,
 * use that number to tune this value.
 */
#ifndef APP_HTTP_STACK_SIZE
#define APP_HTTP_STACK_SIZE 8192
#endif

/* Below the Wi-Fi task (3). Chosen by design, not measured. */
#ifndef APP_HTTP_TASK_PRIORITY
#define APP_HTTP_TASK_PRIORITY 2
#endif

/*
 * Address of the one test request. example.com is kept by IANA for documentation and its page is
 * small. It was picked by the assistant for this test, it is not part of any product backend.
 * Use an https address to test TLS, or an http address to test the plain path.
 */
#ifndef APP_HTTP_TEST_URL
#define APP_HTTP_TEST_URL "https://example.com/"
#endif

/* Static buffer for the response body, in bytes. A larger body is reported as an overflow. */
#ifndef APP_HTTP_BODY_SIZE
#define APP_HTTP_BODY_SIZE 2048U
#endif

/* How often the task checks for a Wi-Fi IP while it waits. */
#ifndef APP_HTTP_WAIT_POLL_MS
#define APP_HTTP_WAIT_POLL_MS 1000U
#endif

/*
 * Creates a task that waits until STA has an IP (it polls sx_wifi_get_status, so it does not
 * matter which of wifi_app_start and http_app_start is called first), sends one GET request to
 * APP_HTTP_TEST_URL, prints the result, the free heap and the free stack, then ends.
 * Returns 0 when the task was created, -1 when it could not be created.
 * Call it once. This replaces the old declaration http_app_init, which had no body.
 */
int http_app_start(void);

#endif /* HTTP_APP_H */