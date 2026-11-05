#include "AI_app.h"
#include "http_app.h"
#include "ntp_app.h"
#include "speaker_app.h"
#include "storage_app.h"

void app_init(void)
{
    // Initialize the AI application
    ai_app_init();

    // Initialize the HTTP application
    http_app_init();

    // Initialize the NTP application
    ntp_app_init();

    // Initialize the speaker application
    speaker_app_init();

    // Initialize the storage application
    app_storage_init();
}