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
#define BAR_WIDTH         12U
#define BAND_COUNT        8U
#define LIVE_CHUNK        128U                    /* samples per read in the live monitor, 8 ms */
#define LIVE_WINDOW       (RATE_HZ / 10U)         /* 100 ms window for the howl guard */
#define LIVE_REPORT_WINDOWS 10U                   /* one report line per 10 windows, 1 s */
#define HOWL_WINDOWS      3U                      /* loud windows in a row that trip the howl guard */

/* The filter coefficients below are made for a sample rate of 16000 Hz. */
#if (BOARD_AUDIO_SAMPLE_RATE_HZ != 16000U)
#error "the band filters of mic_app are written for a sample rate of 16000 Hz"
#endif

/* Static buffers keep the task stack small. Only this task uses them. */
static int16_t s_block[BLOCK_SAMPLES];
static int16_t s_silence[SILENCE_CHUNK];
static int16_t s_chunk[LIVE_CHUNK];

/*
 * Band filters. Each band is a 2nd order Butterworth high-pass followed by a 2nd order Butterworth
 * low-pass (12 dB per octave), made offline with the usual bilinear biquad formulas for 16000 Hz.
 * The -3 dB edges, in band order: 1 to 15 Hz, 15 to 100 Hz, 100 to 400 Hz (LOW), 400 to 600 Hz,
 * 600 to 1700 Hz (MID), 1700 to 2500 Hz, 2500 to 7000 Hz (HIGH), and 7000 Hz and up. The three
 * bands named LOW, MID and HIGH are the original ones. The others were added after a measurement
 * showed that LOW, MID and HIGH together hold only about 2 to 9 percent of the signal power, so the
 * rest had to be located; a later log then showed that most of it is below 15 Hz. The coefficients
 * are in Q28. The filter states are in Q14 (the sample times 16384): with fewer bits the rounding
 * noise of the 1 to 15 Hz band was 20 times larger than its true level (a host simulation with
 * white noise gave 96 instead of 4), while the other bands did not change. The squares that are
 * added up are first reduced to Q8. The states are kept from one block to the next.
 */
#define BAND_STATE_SHIFT 14
typedef struct biquad_coeff {
    int32_t b0;
    int32_t b1;
    int32_t b2;
    int32_t a1;
    int32_t a2;
} biquad_coeff_t;

typedef struct biquad_state {
    int64_t x1;
    int64_t x2;
    int64_t y1;
    int64_t y2;
} biquad_state_t;

/* Positions of the three original bands inside the band tables. */
#define BAND_LOW  2U
#define BAND_MID  4U
#define BAND_HIGH 6U

static const char *const s_band_name[BAND_COUNT] = {
    "1-15", "15-100", "100-400", "400-600", "600-1700", "1700-2500", "2500-7000", "7000+"
};

static const biquad_coeff_t s_hp[BAND_COUNT] = {
    { 268360927, -536721854, 268360927, -536721834, 268286419 },   /* band 0, high-pass 1 Hz */
    { 267319693, -534639386, 267319693, -534634749, 266208568 },   /* band 1, high-pass 15 Hz */
    { 261084055, -522168111, 261084055, -521966747, 253934019 },   /* band 2, high-pass 100 Hz */
    { 240211778, -480423556, 240211778, -477447832, 214963824 },   /* band 3, high-pass 400 Hz */
    { 227219676, -454439352, 227219676, -448073307, 192369941 },   /* band 4, high-pass 600 Hz */
    { 166662211, -333324422, 166662211, -293242402, 104970987 },   /* band 5, high-pass 1700 Hz */
    { 131481915, -262963829, 131481915, -187834497, 69657706 },   /* band 6, high-pass 2500 Hz */
    { 8040872, -16081744, 8040872, 390370540, 154098572 }   /* band 7, high-pass 7000 Hz */
};

static const biquad_coeff_t s_lp[BAND_COUNT] = {
    { 2319, 4638, 2319, -534634749, 266208568 },   /* band 0, low-pass 15 Hz */
    { 100682, 201364, 100682, -521966747, 253934019 },   /* band 1, low-pass 100 Hz */
    { 1487862, 2975724, 1487862, -477447832, 214963824 },   /* band 2, low-pass 400 Hz */
    { 3183022, 6366045, 3183022, -448073307, 192369941 },   /* band 3, low-pass 600 Hz */
    { 20041010, 40082021, 20041010, -293242402, 104970987 },   /* band 4, low-pass 1700 Hz */
    { 37564666, 75129333, 37564666, -187834497, 69657706 },   /* band 5, low-pass 2500 Hz */
    { 203226142, 406452284, 203226142, 390370540, 154098572 },   /* band 6, low-pass 7000 Hz */
    { 268435456, 0, 0, 0, 0 }   /* band 7, no low-pass: passes the signal unchanged (1.0 in Q28) */
};

static biquad_state_t s_hp_state[BAND_COUNT];
static biquad_state_t s_lp_state[BAND_COUNT];

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
    printf("[mic] level meter for %u ms, TALK OR CLAP now (one line per second, it shows one 100 ms block)\n", (unsigned)APP_MIC_METER_MS);
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
        if (((blocks - 1U) % 10U) == 0U) {
            printf("[mic] peak=%5d rms=%5u dc=%6d ", (int)st.peak, (unsigned)st.rms, (int)st.dc);
            print_bar(st.peak);
        }
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

/* Writes a bar of hash characters, one per bit of value, at most BAR_WIDTH, into out (BAR_WIDTH + 1 bytes). */
static void make_bar(char *out, uint32_t value)
{
    unsigned bits = bit_count(value);
    unsigned i;

    if (bits > BAR_WIDTH) {
        bits = BAR_WIDTH;
    }
    for (i = 0U; i < bits; i++) {
        out[i] = '#';
    }
    out[bits] = '\0';
}

/* One step of a biquad (direct form I, Q28 coefficients, input and output in the format of the state, rounded). */
static int64_t biquad_run(const biquad_coeff_t *c, biquad_state_t *st, int64_t x)
{
    int64_t acc = (int64_t)c->b0 * x + (int64_t)c->b1 * st->x1 + (int64_t)c->b2 * st->x2
                  - (int64_t)c->a1 * st->y1 - (int64_t)c->a2 * st->y2;
    int64_t y = (acc + ((int64_t)1 << 27)) >> 28;

    st->x2 = st->x1;
    st->x1 = x;
    st->y2 = st->y1;
    st->y1 = y;
    return y;
}

/*
 * Prepares the band filters for a new stream whose first sample is first. The high-pass state is
 * set as if the signal had always been first, so a DC offset does not ring at the start.
 */
static void bands_reset(int16_t first)
{
    unsigned b;

    memset(s_hp_state, 0, sizeof(s_hp_state));
    memset(s_lp_state, 0, sizeof(s_lp_state));
    for (b = 0U; b < BAND_COUNT; b++) {
        s_hp_state[b].x1 = (int64_t)first * ((int64_t)1 << BAND_STATE_SHIFT);
        s_hp_state[b].x2 = (int64_t)first * ((int64_t)1 << BAND_STATE_SHIFT);
    }
}

/* Runs one sample through all bands and adds the square of each band output (Q8) to sq. */
static void bands_step(int16_t x, uint64_t sq[BAND_COUNT])
{
    unsigned b;

    for (b = 0U; b < BAND_COUNT; b++) {
        int64_t h = biquad_run(&s_hp[b], &s_hp_state[b], (int64_t)x * ((int64_t)1 << BAND_STATE_SHIFT));
        int64_t y = biquad_run(&s_lp[b], &s_lp_state[b], h);
        int64_t y8 = (y + ((int64_t)1 << (BAND_STATE_SHIFT - 9))) >> (BAND_STATE_SHIFT - 8);

        sq[b] += (uint64_t)(y8 * y8);
    }
}

/* RMS in sample units from a sum of squares of Q8 values over n samples. */
static uint32_t band_level(uint64_t sq, uint32_t n)
{
    if (n == 0U) {
        return 0U;
    }
    return (isqrt64(sq / n) + 128U) / 256U;
}

/*
 * Playback high-pass. A log taken with the speaker connected showed that most of the signal power
 * is below 15 Hz, and that the 15 to 100 Hz band is about three times stronger than the LOW band even
 * in quiet seconds. The old output path only removed DC with a slow tracker (corner near 10 Hz), so
 * all of that reached the small speaker and was heard as a rumble and as scratching. The playback now
 * goes through the 2nd order Butterworth high-pass at 100 Hz (the coefficients of band LOW). The
 * measurements above still use the raw samples. Set APP_MIC_PLAY_HPF to 0 to get the old DC tracker.
 */
#ifndef APP_MIC_PLAY_HPF
#define APP_MIC_PLAY_HPF 1
#endif

/*
 * Pre-roll for the live playback, in ms of silence written to the speaker before the first sample
 * of the microphone. In the I2S driver of ESP-IDF v5.4.3 a buffer that the DMA has finished is
 * cleared and handed back to the writer, and the DMA sends a buffer when it reaches it, filled or
 * not. If the writer only adds samples as fast as the microphone delivers them (8 ms per chunk), it
 * has almost no lead on the DMA, and a late chunk is heard as a gap of zeros: a rough, scratchy
 * sound. The recorded playback does not have this problem because it writes a whole buffer at once.
 * The pre-roll builds a lead of this many ms; it also delays the sound by the same time. The value
 * is chosen by design, not measured. The DMA ring holds 6 buffers of 240 frames (90 ms), so keep it
 * below about 75. Set 0 to disable.
 */
#ifndef APP_MIC_LIVE_PREROLL_MS
#define APP_MIC_LIVE_PREROLL_MS 60U
#endif

#if APP_MIC_PLAY_HPF
static biquad_state_t s_play_hpf;

/* Prepares the playback filter for a new stream whose first sample is first (no ringing on a DC offset). */
static void play_hpf_reset(int16_t first)
{
    memset(&s_play_hpf, 0, sizeof(s_play_hpf));
    s_play_hpf.x1 = (int64_t)first * 256;
    s_play_hpf.x2 = (int64_t)first * 256;
}

/* One sample through the playback high-pass, rounded and limited to 16 bit. */
static int16_t play_hpf_step(int16_t x)
{
    int64_t y = biquad_run(&s_hp[BAND_LOW], &s_play_hpf, (int64_t)x * 256);
    int64_t r = (y + 128) >> 8;

    if (r > 32767) {
        r = 32767;
    } else if (r < -32768) {
        r = -32768;
    }
    return (int16_t)r;
}
#endif

typedef struct live_acc {
    int32_t  peak;
    int64_t  sum;
    uint64_t sq;
    uint64_t band_sq[BAND_COUNT];
    uint32_t n;
} live_acc_t;

static void live_print(const live_acc_t *a, bool playing, bool muted)
{
    char bar[BAND_COUNT][BAR_WIDTH + 1U];
    uint32_t level[BAND_COUNT];
    uint64_t mean_sq;
    uint64_t band_ms = 0U;
    uint64_t ac_ms;
    uint32_t ac;
    uint32_t covered = 0U;
    int32_t dc;
    unsigned b;

    if (a->n == 0U) {
        return;
    }
    mean_sq = (uint64_t)(a->sum * a->sum) / a->n;
    ac_ms = ((a->sq > mean_sq) ? (a->sq - mean_sq) : 0U) / a->n;
    ac = isqrt64(ac_ms);
    dc = (int32_t)(a->sum / (int64_t)a->n);
    for (b = 0U; b < BAND_COUNT; b++) {
        level[b] = band_level(a->band_sq[b], a->n);
        make_bar(bar[b], level[b]);
        band_ms += a->band_sq[b] / a->n;     /* mean square in Q16 */
    }
    if (ac_ms > 0U) {
        /* The bands overlap a little at their edges, so this is only a rough share. */
        covered = (uint32_t)((band_ms * 100U) / (ac_ms * 65536U));
    }
    printf("[mic] peak=%5d ac=%5u dc=%6d | LOW=%5u %-12s MID=%5u %-12s HIGH=%5u %-12s | %s\n",
           (int)a->peak, (unsigned)ac, (int)dc,
           (unsigned)level[BAND_LOW], bar[BAND_LOW],
           (unsigned)level[BAND_MID], bar[BAND_MID],
           (unsigned)level[BAND_HIGH], bar[BAND_HIGH],
           muted ? "MUTED by howl guard" : (playing ? "playing" : "print only"));
    printf("[mic] bands rms (Hz): %s=%u %s=%u %s=%u %s=%u %s=%u %s=%u %s=%u %s=%u | about %u%% of the ac power\n",
           s_band_name[0], (unsigned)level[0], s_band_name[1], (unsigned)level[1],
           s_band_name[2], (unsigned)level[2], s_band_name[3], (unsigned)level[3],
           s_band_name[4], (unsigned)level[4], s_band_name[5], (unsigned)level[5],
           s_band_name[6], (unsigned)level[6], s_band_name[7], (unsigned)level[7], (unsigned)covered);
}

/*
 * Continuous monitor with live playback (see mic_app.h). Reads the microphone in 8 ms chunks,
 * measures every sample, removes the DC offset, multiplies by APP_MIC_LIVE_GAIN and writes the
 * result to the speaker. Prints one line per second. Returns RESULT_OK when it ended by itself.
 */
static test_result_t run_live_monitor(speaker_t *spk, inmp441_t *mic, bool speaker_free)
{
    const bool forever = (APP_MIC_LIVE_MS == APP_MIC_LIVE_FOREVER);
    const uint32_t total = forever ? 0U : ((RATE_HZ / 1000U) * APP_MIC_LIVE_MS);
    const uint32_t mute_samples = (RATE_HZ / 1000U) * APP_MIC_LIVE_MUTE_MS;
    const bool want_play = (APP_MIC_LIVE_PLAY != 0) && speaker_free && (spk != NULL);
    live_acc_t acc;
    uint32_t done = 0U;
    uint32_t win_n = 0U;
    uint32_t windows = 0U;
    uint32_t mute_left = 0U;
    unsigned howl = 0U;
    int32_t win_peak = 0;
#if !APP_MIC_PLAY_HPF
    int32_t dc_q8 = 0;
#endif
    bool first = true;
    bool spk_started = false;
    int ret;
    int sret = SPEAKER_OK;

    if (mic == NULL) {
        printf("[mic] the microphone is not available\n");
        return RESULT_FAILED;
    }
    memset(&acc, 0, sizeof(acc));
    printf("[mic] live monitor%s, one line per second. Make a sound: silence, hum mmm (LOW), say aaa (MID), "
           "hiss sss (HIGH), clap (all three)\n", forever ? " (never ends)" : "");
    if (want_play) {
        printf("[mic] live playback ON: volume %u percent, gain x%d. If you hear a howl, move the speaker "
               "away from the microphone or lower APP_MIC_LIVE_VOLUME\n",
               (unsigned)APP_MIC_LIVE_VOLUME, (int)APP_MIC_LIVE_GAIN);
#if APP_MIC_PLAY_HPF
        printf("[mic] playback high-pass 100 Hz is ON (APP_MIC_PLAY_HPF)\n");
#else
        printf("[mic] playback high-pass is OFF, only a slow DC tracker is used\n");
#endif
        sret = speaker_set_volume(spk, (uint8_t)APP_MIC_LIVE_VOLUME);
        if (sret == SPEAKER_OK) {
            sret = speaker_start(spk);
            spk_started = (sret == SPEAKER_OK);
        }
        if (sret != SPEAKER_OK) {
            printf("[mic] speaker start failed: %d, the monitor prints only\n", sret);
        }
    } else {
        printf("[mic] live playback OFF (APP_MIC_LIVE_PLAY is 0 or the speaker is not available)\n");
    }
    ret = inmp441_start(mic);
    if (ret != INMP441_OK) {
        printf("[mic] inmp441_start failed: %d\n", ret);
        if (spk_started) {
            (void)speaker_stop(spk);
        }
        return RESULT_FAILED;
    }
    ret = discard_settle(mic);
#if APP_MIC_LIVE_PREROLL_MS > 0U
    if (spk_started && (sret == SPEAKER_OK)) {
        sret = write_silence(spk, APP_MIC_LIVE_PREROLL_MS);
        if (sret != SPEAKER_OK) {
            printf("[mic] speaker pre-roll failed: %d, the monitor prints only\n", sret);
        } else {
            printf("[mic] live playback pre-roll %u ms (APP_MIC_LIVE_PREROLL_MS)\n",
                   (unsigned)APP_MIC_LIVE_PREROLL_MS);
        }
    }
#endif

    while ((ret == INMP441_OK) && (forever || (done < total))) {
        size_t got = 0U;
        size_t i;
        const bool playing = spk_started && (sret == SPEAKER_OK);

        ret = inmp441_read(mic, s_chunk, LIVE_CHUNK, &got, READ_TIMEOUT_MS);
        if (ret != INMP441_OK) {
            break;
        }
        if (first && (got > 0U)) {
            bands_reset(s_chunk[0]);
#if APP_MIC_PLAY_HPF
            play_hpf_reset(s_chunk[0]);
#else
            dc_q8 = (int32_t)s_chunk[0] * 256;
#endif
            first = false;
        }
        for (i = 0U; i < got; i++) {
            int32_t v = s_chunk[i];
            int32_t a = (v < 0) ? -v : v;
            int32_t out;

            if (a > acc.peak) {
                acc.peak = a;
            }
            if (a > win_peak) {
                win_peak = a;
            }
            acc.sum += v;
            acc.sq += (uint64_t)((int64_t)v * v);
            bands_step(s_chunk[i], acc.band_sq);

#if APP_MIC_PLAY_HPF
            /* Output: 100 Hz high-pass (it also removes DC), then the gain, with saturation. */
            out = (int32_t)play_hpf_step(s_chunk[i]) * (int32_t)APP_MIC_LIVE_GAIN;
#else
            /* Output: remove the slow DC estimate, then the gain, with saturation. */
            dc_q8 += ((v * 256) - dc_q8) / 256;
            out = (v - (dc_q8 / 256)) * (int32_t)APP_MIC_LIVE_GAIN;
#endif
            if (out > 32767) {
                out = 32767;
            } else if (out < -32768) {
                out = -32768;
            }
            s_chunk[i] = (int16_t)out;
        }
        acc.n += (uint32_t)got;

        if (playing && (mute_left == 0U) && (got > 0U)) {
            size_t written = 0U;

            sret = speaker_write(spk, s_chunk, got, &written, WRITE_TIMEOUT_MS);
            if (sret != SPEAKER_OK) {
                printf("[mic] speaker_write failed: %d, the monitor prints only from now on\n", sret);
            }
        } else if (mute_left > 0U) {
            mute_left = (mute_left > (uint32_t)got) ? (mute_left - (uint32_t)got) : 0U;
        }
        done += (uint32_t)got;
        win_n += (uint32_t)got;

        if (win_n >= LIVE_WINDOW) {
            /* Howl guard: the speaker is close to the microphone, so a loud level for 300 ms in a row
               is taken as feedback. The speaker is then silent for APP_MIC_LIVE_MUTE_MS. */
            if (playing && (mute_left == 0U) && (win_peak >= (int32_t)APP_MIC_LIVE_HOWL_PEAK)) {
                howl++;
                if (howl >= HOWL_WINDOWS) {
                    mute_left = mute_samples;
                    howl = 0U;
                    printf("[mic] howl guard: loud input for %u ms in a row, speaker muted for %u ms\n",
                           (unsigned)(HOWL_WINDOWS * 100U), (unsigned)APP_MIC_LIVE_MUTE_MS);
                }
            } else {
                howl = 0U;
            }
            win_n = 0U;
            win_peak = 0;
            windows++;
            if (windows >= LIVE_REPORT_WINDOWS) {
                live_print(&acc, playing, mute_left > 0U);
                memset(&acc, 0, sizeof(acc));
                windows = 0U;
            }
        }
    }
    (void)inmp441_stop(mic);
    if (spk_started) {
        (void)speaker_stop(spk);
    }
    if (ret != INMP441_OK) {
        printf("[mic] live monitor: inmp441_read failed: %d\n", ret);
        return RESULT_FAILED;
    }
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

/*
 * Raw dump of the recording over the console, for offline analysis. It runs once, right after the
 * recording and before the gain is applied, so the samples are exactly what the microphone sent.
 * Only used when the record and play step is on (APP_MIC_RECORD_MS above 0). Set to 0 to disable.
 * Each sample is four hex digits (16 bit two's complement), DUMP_PER_LINE samples per line.
 */
#ifndef APP_MIC_DUMP_RAW
#define APP_MIC_DUMP_RAW 0
#endif
#define DUMP_PER_LINE 64U

#if APP_MIC_DUMP_RAW
static char s_dump_line[(DUMP_PER_LINE * 4U) + 1U];

static void dump_raw(const int16_t *s, size_t n)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t pos = 0U;

    printf("[mic] raw begin samples=%u rate=%u hex16 per_line=%u\n", (unsigned)n, (unsigned)RATE_HZ,
           (unsigned)DUMP_PER_LINE);
    while (pos < n) {
        size_t cnt = n - pos;
        size_t i;

        if (cnt > DUMP_PER_LINE) {
            cnt = DUMP_PER_LINE;
        }
        for (i = 0U; i < cnt; i++) {
            uint16_t u = (uint16_t)s[pos + i];

            s_dump_line[(i * 4U) + 0U] = hex[(u >> 12) & 0xFU];
            s_dump_line[(i * 4U) + 1U] = hex[(u >> 8) & 0xFU];
            s_dump_line[(i * 4U) + 2U] = hex[(u >> 4) & 0xFU];
            s_dump_line[(i * 4U) + 3U] = hex[u & 0xFU];
        }
        s_dump_line[cnt * 4U] = '\0';
        printf("%s\n", s_dump_line);
        pos += cnt;
        /* Yield every line (one tick). A long console write without yielding starves the idle task
           and trips the task watchdog. sx_delay_ms is not used because a short wait there is a busy wait. */
        vTaskDelay(1);
    }
    printf("[mic] raw end\n");
}
#endif

/*
 * Prints where the loudest sample is, the rms of the even and of the odd samples, and eight
 * samples around the peak. If two I2S slots arrive interleaved, the even and odd rms differ a lot.
 * A peak in the first few hundred samples points to a start-up transient.
 */
static void print_slot_diag(const int16_t *s, size_t n)
{
    uint64_t sq_even = 0U;
    uint64_t sq_odd = 0U;
    size_t peak_at = 0U;
    int32_t peak = 0;
    size_t from;
    size_t i;

    if (n < 16U) {
        return;
    }
    for (i = 0U; i < n; i++) {
        int32_t v = s[i];
        int32_t a = (v < 0) ? -v : v;

        if (a > peak) {
            peak = a;
            peak_at = i;
        }
        if ((i % 2U) == 0U) {
            sq_even += (uint64_t)((int64_t)v * v);
        } else {
            sq_odd += (uint64_t)((int64_t)v * v);
        }
    }
    from = (peak_at >= 4U) ? (peak_at - 4U) : 0U;
    if ((from + 8U) > n) {
        from = n - 8U;
    }
    printf("[mic] diag: peak at sample %u of %u, even rms=%u odd rms=%u\n",
           (unsigned)peak_at, (unsigned)n,
           (unsigned)isqrt64(sq_even / (n / 2U)), (unsigned)isqrt64(sq_odd / (n / 2U)));
    printf("[mic] diag: samples %u..%u: %d %d %d %d %d %d %d %d\n", (unsigned)from, (unsigned)(from + 7U),
           (int)s[from], (int)s[from + 1U], (int)s[from + 2U], (int)s[from + 3U],
           (int)s[from + 4U], (int)s[from + 5U], (int)s[from + 6U], (int)s[from + 7U]);
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
    print_slot_diag(rec, got);
#if APP_MIC_DUMP_RAW
    dump_raw(rec, got);
#endif
#if APP_MIC_PLAY_HPF
    {
        const int32_t raw_peak = st.peak;
        size_t k;

        play_hpf_reset(rec[0]);
        for (k = 0U; k < got; k++) {
            rec[k] = play_hpf_step(rec[k]);
        }
        compute_stats(rec, got, &st);
        printf("[mic] playback high-pass 100 Hz: peak %d -> %d\n", (int)raw_peak, (int)st.peak);
        if (st.peak < 1) {
            st.peak = 1;
        }
    }
#endif
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
    test_result_t r_live = RESULT_SKIPPED;
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
    if ((r_meter == RESULT_OK) && (APP_MIC_LIVE_MS != 0U)) {
        r_live = run_live_monitor(board_get_speaker(), board_get_mic(), speaker_free);
        printf("[mic] live monitor %s\n", result_text(r_live));
    }
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