#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "speaker.h"
#include "inmp441.h"
#include "sx_time.h"
#include "audio_app.h"

#define RATE_HZ           BOARD_AUDIO_SAMPLE_RATE_HZ
#define TONE_CHUNK        256U
#define FADE_SAMPLES      160U                   /* 10 ms at 16 kHz, avoids a click at the start and the end */
#define BLOCK_SAMPLES     (RATE_HZ / 10U)        /* 100 ms of microphone data */
#define WRITE_TIMEOUT_MS  1000U
#define READ_TIMEOUT_MS   1000U
#define BAR_MAX           16U

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

/* Static buffers keep the task stack small. Only this task uses them. */
static int16_t s_tone[TONE_CHUNK];
static int16_t s_block[BLOCK_SAMPLES];

typedef enum { RESULT_SKIPPED = 0, RESULT_OK, RESULT_FAILED } test_result_t;

static const char *result_text(test_result_t r)
{
    if (r == RESULT_OK) {
        return "ok";
    }
    return (r == RESULT_FAILED) ? "FAILED" : "skipped";
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
            int32_t v = (sine_at(phase) * APP_AUDIO_TONE_AMPLITUDE) / 32767;

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

static test_result_t run_speaker_test(speaker_t *spk)
{
    uint32_t written = 0U;
    uint32_t t0 = 0U;
    uint32_t t1 = 0U;
    unsigned k;
    int ret = speaker_set_volume(spk, (uint8_t)APP_AUDIO_TONE_VOLUME);

    printf("[audio] speaker test: three tones 440, 880, 1320 Hz at volume %u percent, listen for three rising beeps\n",
           (unsigned)APP_AUDIO_TONE_VOLUME);
    if (ret != SPEAKER_OK) {
        printf("[audio] speaker_set_volume failed: %d\n", ret);
        return RESULT_FAILED;
    }
    ret = speaker_start(spk);
    if (ret != SPEAKER_OK) {
        printf("[audio] speaker_start failed: %d\n", ret);
        return RESULT_FAILED;
    }
    sx_get_time_ms(&t0);
    for (k = 0U; (k < 3U) && (ret == SPEAKER_OK); k++) {
        ret = play_tone(spk, s_tone_hz[k], APP_AUDIO_TONE_MS, &written);
        if (ret == SPEAKER_OK) {
            ret = play_silence(spk, APP_AUDIO_GAP_MS, &written);
        }
    }
    sx_get_time_ms(&t1);
    (void)speaker_stop(spk);
    if (ret != SPEAKER_OK) {
        printf("[audio] speaker_write failed: %d after %u samples\n", ret, (unsigned)written);
        return RESULT_FAILED;
    }
    /* Times are measured by subtraction, so they stay correct when the counter wraps. */
    printf("[audio] speaker: %u samples queued (%u ms of audio) in %u ms; the I2S clock paces the writes, so these two should be close\n",
           (unsigned)written, (unsigned)(written / (RATE_HZ / 1000U)), (unsigned)(t1 - t0));
    return RESULT_OK;
}

/* Integer square root. */
static uint32_t isqrt64(uint64_t v)
{
    uint64_t r = 0U;
    uint64_t bit = (uint64_t)1U << 62;

    while (bit > v) {
        bit >>= 2;
    }
    while (bit != 0U) {
        if (v >= (r + bit)) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)r;
}

/* Number of bits needed to write v, 0 for 0. */
static unsigned bit_count(uint32_t v)
{
    unsigned n = 0U;

    while (v != 0U) {
        n++;
        v >>= 1;
    }
    return n;
}

typedef struct block_stats {
    int32_t  peak;      /* largest absolute sample */
    uint32_t rms;
    int32_t  dc;        /* mean of the samples */
    bool     flat;      /* every sample has the same value */
} block_stats_t;

static void compute_stats(const int16_t *s, size_t n, block_stats_t *out)
{
    int64_t sum = 0;
    uint64_t sq = 0U;
    int32_t peak = 0;
    int16_t lo = s[0];
    int16_t hi = s[0];
    size_t i;

    if (n == 0U) {
        out->peak = 0;
        out->rms = 0U;
        out->dc = 0;
        out->flat = false;
        return;
    }
    for (i = 0U; i < n; i++) {
        int32_t v = s[i];
        int32_t a = (v < 0) ? -v : v;

        sum += v;
        sq += (uint64_t)((int64_t)v * v);
        if (a > peak) {
            peak = a;
        }
        if (s[i] < lo) {
            lo = s[i];
        }
        if (s[i] > hi) {
            hi = s[i];
        }
    }
    out->peak = peak;
    out->rms = isqrt64(sq / n);
    out->dc = (int32_t)(sum / (int64_t)n);
    out->flat = (lo == hi);
}

static void print_bar(int32_t peak)
{
    char bar[BAR_MAX + 1U];
    unsigned bits = bit_count((uint32_t)peak);
    unsigned i;

    if (bits > BAR_MAX) {
        bits = BAR_MAX;
    }
    for (i = 0U; i < bits; i++) {
        bar[i] = '#';
    }
    bar[bits] = '\0';
    printf("|%s\n", bar);
}

/* Reads and throws away the samples of the start-up time. Returns INMP441_OK or an error code. */
static int discard_settle(inmp441_t *mic)
{
    uint32_t left = (RATE_HZ / 1000U) * APP_AUDIO_MIC_SETTLE_MS;

    while (left > 0U) {
        size_t want = (left < BLOCK_SAMPLES) ? left : BLOCK_SAMPLES;
        size_t got = 0U;
        int ret = inmp441_read(mic, s_block, want, &got, READ_TIMEOUT_MS);

        if (ret != INMP441_OK) {
            return ret;
        }
        left -= (uint32_t)want;
    }
    return INMP441_OK;
}

static test_result_t run_mic_meter(inmp441_t *mic)
{
    const uint32_t blocks_total = APP_AUDIO_MIC_METER_MS / 100U;
    uint32_t blocks = 0U;
    uint32_t flat_blocks = 0U;
    int32_t quiet = 0;
    int32_t loud = 0;
    int ret;

    printf("[audio] microphone test: level meter for %u ms, TALK OR CLAP now (one line per 100 ms)\n",
           (unsigned)APP_AUDIO_MIC_METER_MS);
    ret = inmp441_start(mic);
    if (ret != INMP441_OK) {
        printf("[audio] inmp441_start failed: %d\n", ret);
        return RESULT_FAILED;
    }
    ret = discard_settle(mic);
    while ((ret == INMP441_OK) && (blocks < blocks_total)) {
        block_stats_t st;
        size_t got = 0U;

        ret = inmp441_read(mic, s_block, BLOCK_SAMPLES, &got, READ_TIMEOUT_MS);
        if (ret != INMP441_OK) {
            break;
        }
        compute_stats(s_block, got, &st);
        if ((blocks == 0U) || (st.peak < quiet)) {
            quiet = st.peak;
        }
        if ((blocks == 0U) || (st.peak > loud)) {
            loud = st.peak;
        }
        if (st.flat) {
            flat_blocks++;
        }
        blocks++;
        printf("[audio] mic peak=%5d rms=%5u dc=%6d ", (int)st.peak, (unsigned)st.rms, (int)st.dc);
        print_bar(st.peak);
    }
    (void)inmp441_stop(mic);
    if (ret != INMP441_OK) {
        printf("[audio] inmp441_read failed: %d after %u blocks\n", ret, (unsigned)blocks);
        return RESULT_FAILED;
    }
    if (blocks == 0U) {
        printf("[audio] mic: no block was measured (APP_AUDIO_MIC_METER_MS is below 100)\n");
        return RESULT_SKIPPED;
    }
    if (loud == 0) {
        printf("[audio] mic verdict: ALL SAMPLES ZERO. The microphone sends nothing on the left slot: check that L/R is tied to GND, the 3.3 V supply, and the wiring of SD to GPIO 12, WS to 11, SCK to 10\n");
        return RESULT_FAILED;
    }
    if (flat_blocks == blocks) {
        printf("[audio] mic verdict: SAMPLES NEVER CHANGE (every block is one constant value). The data line may be stuck or floating: check the SD wire and the supply\n");
        return RESULT_FAILED;
    }
    printf("[audio] mic verdict: samples vary. quietest block peak=%d, loudest block peak=%d. Speech or a clap should be far above the quietest block; judge it from the lines above\n",
           (int)quiet, (int)loud);
    return RESULT_OK;
}

/* Fills dst with count samples. Returns INMP441_OK or an error code; *got is the number of valid samples. */
static int record_samples(inmp441_t *mic, int16_t *dst, size_t count, size_t *got_total)
{
    size_t pos = 0U;
    int ret = INMP441_OK;

    *got_total = 0U;
    while ((pos < count) && (ret == INMP441_OK)) {
        size_t want = ((count - pos) < BLOCK_SAMPLES) ? (count - pos) : BLOCK_SAMPLES;
        size_t got = 0U;

        ret = inmp441_read(mic, dst + pos, want, &got, READ_TIMEOUT_MS);
        pos += got;
        *got_total = pos;
    }
    return ret;
}

/* Multiplies by gain with saturation. */
static void amplify(int16_t *s, size_t n, int32_t gain)
{
    size_t i;

    for (i = 0U; i < n; i++) {
        int32_t v = (int32_t)s[i] * gain;

        if (v > 32767) {
            v = 32767;
        } else if (v < -32768) {
            v = -32768;
        }
        s[i] = (int16_t)v;
    }
}

static test_result_t run_record_play(speaker_t *spk, inmp441_t *mic)
{
    const size_t total = ((size_t)RATE_HZ / 1000U) * APP_AUDIO_RECORD_MS;
    block_stats_t st;
    int16_t *rec;
    size_t got = 0U;
    size_t written = 0U;
    uint32_t silence = 0U;
    int32_t gain;
    bool spk_started = false;
    int ret;

    if (total == 0U) {
        printf("[audio] record and play skipped (APP_AUDIO_RECORD_MS is 0)\n");
        return RESULT_SKIPPED;
    }
    rec = (int16_t *)malloc(total * sizeof(int16_t));
    if (rec == NULL) {
        printf("[audio] record and play skipped: the heap cannot hold %u bytes, lower APP_AUDIO_RECORD_MS\n",
               (unsigned)(total * sizeof(int16_t)));
        return RESULT_SKIPPED;
    }

    printf("[audio] record and play: recording %u ms starts in 1 s, SPEAK after the next line\n",
           (unsigned)APP_AUDIO_RECORD_MS);
    sx_delay_ms(1000U);
    ret = inmp441_start(mic);
    if (ret == INMP441_OK) {
        ret = discard_settle(mic);
    }
    if (ret == INMP441_OK) {
        printf("[audio] recording now\n");
        ret = record_samples(mic, rec, total, &got);
    }
    (void)inmp441_stop(mic);
    if (ret != INMP441_OK) {
        printf("[audio] recording failed: %d after %u of %u samples\n", ret, (unsigned)got, (unsigned)total);
        free(rec);
        return RESULT_FAILED;
    }

    compute_stats(rec, got, &st);
    if (st.peak == 0) {
        printf("[audio] recording is all zero, nothing to play (see the microphone verdict)\n");
        free(rec);
        return RESULT_FAILED;
    }
    gain = APP_AUDIO_PLAY_PEAK / st.peak;
    if (gain < 1) {
        gain = 1;
    }
    if (gain > APP_AUDIO_MAX_GAIN) {
        gain = APP_AUDIO_MAX_GAIN;
    }
    amplify(rec, got, gain);
    printf("[audio] recorded %u samples, peak %d, software gain x%d, playing at volume %u percent\n",
           (unsigned)got, (int)st.peak, (int)gain, (unsigned)APP_AUDIO_PLAY_VOLUME);

    ret = speaker_set_volume(spk, (uint8_t)APP_AUDIO_PLAY_VOLUME);
    if (ret == SPEAKER_OK) {
        ret = speaker_start(spk);
        spk_started = (ret == SPEAKER_OK);
    }
    if (ret == SPEAKER_OK) {
        ret = speaker_write(spk, rec, got, &written, WRITE_TIMEOUT_MS);
    }
    if (ret == SPEAKER_OK) {
        ret = play_silence(spk, 100U, &silence);
    }
    if (spk_started) {
        (void)speaker_stop(spk);
    }
    free(rec);
    if (ret != SPEAKER_OK) {
        printf("[audio] playback failed: %d after %u samples\n", ret, (unsigned)written);
        return RESULT_FAILED;
    }
    printf("[audio] playback done: %u samples queued. You should hear your own voice\n", (unsigned)written);
    return RESULT_OK;
}

/* Prints the lowest free stack, then ends the calling task. */
static void end_task(void)
{
    printf("[audio] task ends, lowest free stack %u of %u bytes\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)APP_AUDIO_STACK_SIZE);
    vTaskDelete(NULL);
}

static void audio_task(void *arg)
{
    test_result_t r_spk = RESULT_SKIPPED;
    test_result_t r_mic = RESULT_SKIPPED;
    test_result_t r_play = RESULT_SKIPPED;
    bool spk_up = false;
    bool mic_up = false;
    int ret;

    (void)arg;
    ret = board_init();
    if (ret != BOARD_OK) {
        printf("[audio] board_init failed: %d\n", ret);
        end_task();
        return;
    }
    ret = board_init_speaker();
    if (ret == SPEAKER_OK) {
        spk_up = true;
    } else {
        printf("[audio] board_init_speaker failed: %d, the speaker tests are skipped\n", ret);
    }
    ret = board_init_mic();
    if (ret == INMP441_OK) {
        mic_up = true;
    } else {
        printf("[audio] board_init_mic failed: %d, the microphone tests are skipped\n", ret);
    }

    if (spk_up) {
        r_spk = run_speaker_test(board_get_speaker());
    }
    if (mic_up) {
        r_mic = run_mic_meter(board_get_mic());
    }
    if (spk_up && mic_up && (r_mic == RESULT_OK)) {
        r_play = run_record_play(board_get_speaker(), board_get_mic());
    }
    printf("[audio] summary: speaker tones %s (heard? judge by ear), mic meter %s, record and play %s\n",
           result_text(r_spk), result_text(r_mic), result_text(r_play));
    end_task();
}

int audio_app_start(void)
{
    if (xTaskCreate(audio_task, APP_AUDIO_TASK_NAME, APP_AUDIO_STACK_SIZE, NULL,
                    APP_AUDIO_TASK_PRIORITY, NULL) != pdPASS) {
        printf("[audio] cannot create the task\n");
        return -1;
    }
    return 0;
}