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
#include "speaker_app.h"
#include "mic_app.h"

#define RATE_HZ           BOARD_AUDIO_SAMPLE_RATE_HZ
#define BLOCK_SAMPLES     (RATE_HZ / 10U)        /* 100 ms of microphone data */
#define WRITE_TIMEOUT_MS  1000U
#define READ_TIMEOUT_MS   1000U
#define POLL_MS           100U
#define BAR_MAX           16U
#define SILENCE_CHUNK     256U

/* Static buffers keep the task stack small. Only this task uses them. */
static int16_t s_block[BLOCK_SAMPLES];
static int16_t s_silence[SILENCE_CHUNK];

typedef enum { RESULT_SKIPPED = 0, RESULT_OK, RESULT_FAILED } test_result_t;

static const char *result_text(test_result_t r)
{
    if (r == RESULT_OK) {
        return "ok";
    }
    return (r == RESULT_FAILED) ? "FAILED" : "skipped";
}

/*
 * Waits until speaker_app has finished with the speaker. Returns true when it has, false when
 * timeout_ms ran out. The time is measured by subtraction, so it stays correct when the counter wraps.
 */
static bool wait_speaker_done(uint32_t timeout_ms)
{
    uint32_t t0 = 0U;
    uint32_t now = 0U;

    printf("[mic] waiting for the speaker tone test to end (at most %u ms)\n", (unsigned)timeout_ms);
    sx_get_time_ms(&t0);
    while (!speaker_app_tone_test_done()) {
        sx_get_time_ms(&now);
        if ((now - t0) >= timeout_ms) {
            return false;
        }
        sx_delay_ms(POLL_MS);
    }
    return true;
}

/* Writes ms milliseconds of silence. Returns SPEAKER_OK or the first error code. */
static int write_silence(speaker_t *spk, uint32_t ms)
{
    const uint32_t total = (RATE_HZ / 1000U) * ms;
    uint32_t done = 0U;

    memset(s_silence, 0, sizeof(s_silence));
    while (done < total) {
        uint32_t n = ((total - done) < SILENCE_CHUNK) ? (total - done) : SILENCE_CHUNK;
        size_t written = 0U;
        int ret = speaker_write(spk, s_silence, n, &written, WRITE_TIMEOUT_MS);

        if (ret != SPEAKER_OK) {
            return ret;
        }
        done += n;
    }
    return SPEAKER_OK;
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
    int16_t lo;
    int16_t hi;
    size_t i;

    if (n == 0U) {
        out->peak = 0;
        out->rms = 0U;
        out->dc = 0;
        out->flat = false;
        return;
    }
    lo = s[0];
    hi = s[0];
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
    uint32_t left = (RATE_HZ / 1000U) * APP_MIC_SETTLE_MS;

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
    const uint32_t blocks_total = APP_MIC_METER_MS / 100U;
    uint32_t blocks = 0U;
    uint32_t flat_blocks = 0U;
    int32_t quiet = 0;
    int32_t loud = 0;
    int ret;

    if (mic == NULL) {
        printf("[mic] the microphone is not available\n");
        return RESULT_FAILED;
    }
    printf("[mic] level meter for %u ms, TALK OR CLAP now (one line per 100 ms)\n", (unsigned)APP_MIC_METER_MS);
    ret = inmp441_start(mic);
    if (ret != INMP441_OK) {
        printf("[mic] inmp441_start failed: %d\n", ret);
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
        printf("[mic] peak=%5d rms=%5u dc=%6d ", (int)st.peak, (unsigned)st.rms, (int)st.dc);
        print_bar(st.peak);
    }
    (void)inmp441_stop(mic);
    if (ret != INMP441_OK) {
        printf("[mic] inmp441_read failed: %d after %u blocks\n", ret, (unsigned)blocks);
        return RESULT_FAILED;
    }
    if (blocks == 0U) {
        printf("[mic] no block was measured (APP_MIC_METER_MS is below 100)\n");
        return RESULT_SKIPPED;
    }
    if (loud == 0) {
        printf("[mic] verdict: ALL SAMPLES ZERO. The microphone sends nothing on the left slot: check that L/R is tied to GND, the 3.3 V supply, and the wiring of SD to GPIO 12, WS to 11, SCK to 10\n");
        return RESULT_FAILED;
    }
    if (flat_blocks == blocks) {
        printf("[mic] verdict: SAMPLES NEVER CHANGE (every block is one constant value). The data line may be stuck or floating: check the SD wire and the supply\n");
        return RESULT_FAILED;
    }
    printf("[mic] verdict: samples vary. quietest block peak=%d, loudest block peak=%d. Speech or a clap should be far above the quietest block; judge it from the lines above\n",
           (int)quiet, (int)loud);
    return RESULT_OK;
}

/* Fills dst with count samples. Returns INMP441_OK or an error code; got_total is the number of valid samples. */
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
    const size_t total = ((size_t)RATE_HZ / 1000U) * APP_MIC_RECORD_MS;
    block_stats_t st;
    int16_t *rec;
    size_t got = 0U;
    size_t written = 0U;
    int32_t gain;
    bool spk_started = false;
    int ret;

    if (total == 0U) {
        printf("[mic] record and play skipped (APP_MIC_RECORD_MS is 0)\n");
        return RESULT_SKIPPED;
    }
    if ((spk == NULL) || (mic == NULL)) {
        printf("[mic] record and play skipped: the speaker is not available\n");
        return RESULT_SKIPPED;
    }
    rec = (int16_t *)malloc(total * sizeof(int16_t));
    if (rec == NULL) {
        printf("[mic] record and play skipped: the heap cannot hold %u bytes, lower APP_MIC_RECORD_MS\n",
               (unsigned)(total * sizeof(int16_t)));
        return RESULT_SKIPPED;
    }

    printf("[mic] record and play: recording %u ms starts in 1 s, SPEAK after the next line\n",
           (unsigned)APP_MIC_RECORD_MS);
    sx_delay_ms(1000U);
    ret = inmp441_start(mic);
    if (ret == INMP441_OK) {
        ret = discard_settle(mic);
    }
    if (ret == INMP441_OK) {
        printf("[mic] recording now\n");
        ret = record_samples(mic, rec, total, &got);
    }
    (void)inmp441_stop(mic);
    if (ret != INMP441_OK) {
        printf("[mic] recording failed: %d after %u of %u samples\n", ret, (unsigned)got, (unsigned)total);
        free(rec);
        return RESULT_FAILED;
    }

    compute_stats(rec, got, &st);
    if (st.peak == 0) {
        printf("[mic] recording is all zero, nothing to play (see the verdict of the level meter)\n");
        free(rec);
        return RESULT_FAILED;
    }
    gain = APP_MIC_PLAY_PEAK / st.peak;
    if (gain < 1) {
        gain = 1;
    }
    if (gain > APP_MIC_MAX_GAIN) {
        gain = APP_MIC_MAX_GAIN;
    }
    amplify(rec, got, gain);
    printf("[mic] recorded %u samples, peak %d, software gain x%d, playing at volume %u percent\n",
           (unsigned)got, (int)st.peak, (int)gain, (unsigned)APP_MIC_PLAY_VOLUME);

    ret = speaker_set_volume(spk, (uint8_t)APP_MIC_PLAY_VOLUME);
    if (ret == SPEAKER_OK) {
        ret = speaker_start(spk);
        spk_started = (ret == SPEAKER_OK);
    }
    if (ret == SPEAKER_OK) {
        ret = speaker_write(spk, rec, got, &written, WRITE_TIMEOUT_MS);
    }
    if (ret == SPEAKER_OK) {
        ret = write_silence(spk, 100U);
    }
    if (spk_started) {
        (void)speaker_stop(spk);
    }
    free(rec);
    if (ret != SPEAKER_OK) {
        printf("[mic] playback failed: %d after %u samples\n", ret, (unsigned)written);
        return RESULT_FAILED;
    }
    printf("[mic] playback done: %u samples queued. You should hear your own voice\n", (unsigned)written);
    return RESULT_OK;
}

/* Prints the lowest free stack, then ends the calling task. */
static void end_task(void)
{
    printf("[mic] task ends, lowest free stack %u of %u bytes\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)APP_MIC_STACK_SIZE);
    vTaskDelete(NULL);
}

static void mic_task(void *arg)
{
    test_result_t r_meter = RESULT_SKIPPED;
    test_result_t r_play = RESULT_SKIPPED;
    bool speaker_free;
    int ret;

    (void)arg;
    /* Nothing touches the hardware before the speaker test is over. */
    speaker_free = wait_speaker_done(APP_MIC_WAIT_SPEAKER_MS);
    if (!speaker_free) {
        printf("[mic] the speaker test did not end in time (is speaker_app_start called?), record and play will be skipped\n");
    }
    ret = board_init();
    if (ret != BOARD_OK) {
        printf("[mic] board_init failed: %d, the microphone tests are skipped\n", ret);
        end_task();
        return;
    }
    ret = board_init_mic();
    if (ret != INMP441_OK) {
        printf("[mic] board_init_mic failed: %d, the microphone tests are skipped\n", ret);
        end_task();
        return;
    }

    r_meter = run_mic_meter(board_get_mic());
    if ((r_meter == RESULT_OK) && speaker_free) {
        r_play = run_record_play(board_get_speaker(), board_get_mic());
    }
    printf("[mic] summary: level meter %s, record and play %s\n", result_text(r_meter), result_text(r_play));
    end_task();
}

int mic_app_start(void)
{
    if (xTaskCreate(mic_task, APP_MIC_TASK_NAME, APP_MIC_STACK_SIZE, NULL,
                    APP_MIC_TASK_PRIORITY, NULL) != pdPASS) {
        printf("[mic] cannot create the task\n");
        return -1;
    }
    return 0;
}