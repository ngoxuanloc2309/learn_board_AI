#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "speaker.h"
#include "sx_time.h"
#include "speaker_app.h"

#define RATE_HZ           BOARD_AUDIO_SAMPLE_RATE_HZ
#define TONE_CHUNK        256U
#define FADE_SAMPLES      160U                   /* 10 ms at 16 kHz, avoids a click at the start and the end */
#define WRITE_TIMEOUT_MS  1000U

/* One period of a sine wave, 64 points, peak 32767. */
static const int16_t s_sine[64] = {
    0, 3212, 6393, 9512, 12539, 15446, 18204, 20787,
    23170, 25329, 27245, 28898, 30273, 31356, 32137, 32609,
    32767, 32609, 32137, 31356, 30273, 28898, 27245, 25329,
    23170, 20787, 18204, 15446, 12539, 9512, 6393, 3212,
    0, -3212, -6393, -9512, -12539, -15446, -18204, -20787,
    -23170, -25329, -27245, -28898, -30273, -31356, -32137, -32609,
    -32767, -32609, -32137, -31356, -30273, -28898, -27245, -25329,
    -23170, -20787, -18204, -15446, -12539, -9512, -6393, -3212
};

static const uint32_t s_tone_hz[3] = { 440U, 880U, 1320U };

/* A static buffer keeps the task stack small. Only this task uses it. */
static int16_t s_tone[TONE_CHUNK];

/* Set once, read by other tasks, so it uses atomic accesses with acquire and release order. */
static int s_tone_done;

static void mark_done(void)
{
    __atomic_store_n(&s_tone_done, 1, __ATOMIC_RELEASE);
}

bool speaker_app_tone_test_done(void)
{
    return __atomic_load_n(&s_tone_done, __ATOMIC_ACQUIRE) != 0;
}

/* Sine value for a 32 bit phase (the full range is one period), linear interpolation between points. */
static int32_t sine_at(uint32_t phase)
{
    uint32_t idx = phase >> 26;
    int32_t frac = (int32_t)((phase >> 10) & 0xFFFFU);
    int32_t a = s_sine[idx];
    int32_t b = s_sine[(idx + 1U) & 63U];

    return a + (((b - a) * frac) >> 16);
}

/*
 * Plays one tone of freq_hz for ms milliseconds. Returns SPEAKER_OK or the first error code.
 * written_total gets the number of samples accepted by the driver.
 */
static int play_tone(speaker_t *spk, uint32_t freq_hz, uint32_t ms, uint32_t *written_total)
{
    const uint32_t total = (RATE_HZ / 1000U) * ms;
    const uint32_t step = (uint32_t)(((uint64_t)freq_hz << 32) / RATE_HZ);
    uint32_t phase = 0U;
    uint32_t done = 0U;

    while (done < total) {
        uint32_t n = ((total - done) < TONE_CHUNK) ? (total - done) : TONE_CHUNK;
        uint32_t i;
        size_t written = 0U;
        int ret;

        for (i = 0U; i < n; i++) {
            uint32_t pos = done + i;
            uint32_t left = total - pos;
            uint32_t edge = (pos < left) ? pos : left;
            int32_t fade = (int32_t)((edge < FADE_SAMPLES) ? edge : FADE_SAMPLES);
            int32_t v = (sine_at(phase) * APP_SPEAKER_TONE_AMPLITUDE) / 32767;

            s_tone[i] = (int16_t)((v * fade) / (int32_t)FADE_SAMPLES);
            phase += step;
        }
        ret = speaker_write(spk, s_tone, n, &written, WRITE_TIMEOUT_MS);
        *written_total += (uint32_t)written;
        if (ret != SPEAKER_OK) {
            return ret;
        }
        done += n;
    }
    return SPEAKER_OK;
}

static int play_silence(speaker_t *spk, uint32_t ms, uint32_t *written_total)
{
    const uint32_t total = (RATE_HZ / 1000U) * ms;
    uint32_t done = 0U;

    memset(s_tone, 0, sizeof(s_tone));
    while (done < total) {
        uint32_t n = ((total - done) < TONE_CHUNK) ? (total - done) : TONE_CHUNK;
        size_t written = 0U;
        int ret = speaker_write(spk, s_tone, n, &written, WRITE_TIMEOUT_MS);

        *written_total += (uint32_t)written;
        if (ret != SPEAKER_OK) {
            return ret;
        }
        done += n;
    }
    return SPEAKER_OK;
}

/* Returns true when all three tones were accepted by the driver. */
static bool run_tone_test(speaker_t *spk)
{
    uint32_t written = 0U;
    uint32_t t0 = 0U;
    uint32_t t1 = 0U;
    unsigned k;
    int ret;

    if (spk == NULL) {
        printf("[speaker] the speaker is not available\n");
        return false;
    }
    ret = speaker_set_volume(spk, (uint8_t)APP_SPEAKER_TONE_VOLUME);
    printf("[speaker] three tones 440, 880, 1320 Hz at volume %u percent, listen for three rising beeps\n",
           (unsigned)APP_SPEAKER_TONE_VOLUME);
    if (ret != SPEAKER_OK) {
        printf("[speaker] speaker_set_volume failed: %d\n", ret);
        return false;
    }
    ret = speaker_start(spk);
    if (ret != SPEAKER_OK) {
        printf("[speaker] speaker_start failed: %d\n", ret);
        return false;
    }
    sx_get_time_ms(&t0);
    for (k = 0U; (k < 3U) && (ret == SPEAKER_OK); k++) {
        ret = play_tone(spk, s_tone_hz[k], APP_SPEAKER_TONE_MS, &written);
        if (ret == SPEAKER_OK) {
            ret = play_silence(spk, APP_SPEAKER_GAP_MS, &written);
        }
    }
    sx_get_time_ms(&t1);
    (void)speaker_stop(spk);
    if (ret != SPEAKER_OK) {
        printf("[speaker] speaker_write failed: %d after %u samples\n", ret, (unsigned)written);
        return false;
    }
    /* Times are measured by subtraction, so they stay correct when the counter wraps. */
    printf("[speaker] %u samples queued (%u ms of audio) in %u ms; the I2S clock paces the writes, so these two should be close\n",
           (unsigned)written, (unsigned)(written / (RATE_HZ / 1000U)), (unsigned)(t1 - t0));
    return true;
}

/* Prints the lowest free stack, then ends the calling task. */
static void end_task(void)
{
    printf("[speaker] task ends, lowest free stack %u of %u bytes\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)APP_SPEAKER_STACK_SIZE);
    vTaskDelete(NULL);
}

static void speaker_task(void *arg)
{
    bool ran = false;
    bool ok = false;
    int ret;

    (void)arg;
    ret = board_init();
    if (ret != BOARD_OK) {
        printf("[speaker] board_init failed: %d, the tone test is skipped\n", ret);
    } else {
        ret = board_init_speaker();
        if (ret != SPEAKER_OK) {
            printf("[speaker] board_init_speaker failed: %d, the tone test is skipped\n", ret);
        } else {
            ran = true;
            ok = run_tone_test(board_get_speaker());
        }
    }
    /* The flag is set when the speaker is free again, in every case, so that mic_app never waits for nothing. */
    mark_done();
    printf("[speaker] summary: %s\n",
           ran ? (ok ? "tones ok (heard? judge by ear)" : "tones FAILED") : "tones skipped");
    end_task();
}

int speaker_app_start(void)
{
    if (xTaskCreate(speaker_task, APP_SPEAKER_TASK_NAME, APP_SPEAKER_STACK_SIZE, NULL,
                    APP_SPEAKER_TASK_PRIORITY, NULL) != pdPASS) {
        printf("[speaker] cannot create the task\n");
        return -1;
    }
    return 0;
}