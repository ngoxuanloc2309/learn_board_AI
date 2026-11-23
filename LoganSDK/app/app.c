#include "app.h"
#include "AI_app.h"
#include "http_app.h"
#include "mic_app.h"
#include "screen_app.h"
#include "speaker_app.h"
#include "storage_app.h"
#include "wifi_app.h"

void app_start(void)
{
    /* The result is already printed by wifi_app_start. A failed Wi-Fi start does not stop the device. */
    // (void)wifi_app_start();
    /* http_app waits for the Wi-Fi IP by itself, so the order of these two calls does not matter. */
    // (void)http_app_start();
    /* screen_app brings up the display by itself in its own task (board_init is called there). */
    // (void)screen_app_start();
    /*
     * speaker_app and mic_app are the speaker and microphone bring-up tests. Each calls board_init
     * by itself (it is safe to call twice). speaker_app plays three tones once per boot. mic_app
     * waits until that test is over, then shows a microphone level meter and plays back a
     * recording. They are switched off while the microSD test runs, so that the log is not mixed
     * and the speaker does not play the calibration noise of mic_app. Remove the two slashes to
     * run them again.
     */
    // (void)speaker_app_start();
    // (void)mic_app_start();
    /*
     * storage_app is the microSD bring-up test. It runs once, in its own task, and ends. Insert a
     * FAT32 card before the power is on. It never replaces a file of the owner (see storage_app.h).
     */
    // (void)storage_app_start();
    /*
     * ai_app is step 2 of the voice test (talk button, turn state machine, voice activity detector,
     * Vietnamese text). It owns the screen, the touch controller and the microphone, so it must not
     * run together with screen_app, mic_app or speaker_app (the last one is switched off for this
     * reason, and so that its tones do not reach the microphone). It uses no network.
     */
    (void)ai_app_start();
}