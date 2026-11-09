#include "app.h"
#include "http_app.h"
#include "mic_app.h"
#include "screen_app.h"
#include "speaker_app.h"
#include "wifi_app.h"

void app_start(void)
{
    /* The result is already printed by wifi_app_start. A failed Wi-Fi start does not stop the device. */
    (void)wifi_app_start();
    /* http_app waits for the Wi-Fi IP by itself, so the order of these two calls does not matter. */
    (void)http_app_start();
    /* screen_app brings up the display by itself in its own task (board_init is called there). */
    (void)screen_app_start();
    /*
     * speaker_app and mic_app are the speaker and microphone bring-up tests. Each calls board_init
     * by itself (it is safe to call twice). speaker_app plays three tones once per boot. mic_app
     * waits until that test is over, then shows a microphone level meter and plays back a
     * recording. Remove both calls when the tests are no longer needed.
     */
    (void)speaker_app_start();
    (void)mic_app_start();
}