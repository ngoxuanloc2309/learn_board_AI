#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "speaker.h"
#include "inmp441.h"
#include "sx_time.h"
#include "speaker_app.h"
#include "mic_app.h"

/* Helpers of the measuring bands are not used while the live monitor is quiet. */
#define MIC_MAYBE_UNUSED  __attribute__((unused))

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

static const char *const s_band_name[BAND_COUNT] MIC_MAYBE_UNUSED = {
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

static const biquad_coeff_t s_lp[BAND_COUNT] MIC_MAYBE_UNUSED = {
    { 2319, 4638, 2319, -534634749, 266208568 },   /* band 0, low-pass 15 Hz */
    { 100682, 201364, 100682, -521966747, 253934019 },   /* band 1, low-pass 100 Hz */
    { 1487862, 2975724, 1487862, -477447832, 214963824 },   /* band 2, low-pass 400 Hz */
    { 3183022, 6366045, 3183022, -448073307, 192369941 },   /* band 3, low-pass 600 Hz */
    { 20041010, 40082021, 20041010, -293242402, 104970987 },   /* band 4, low-pass 1700 Hz */
    { 37564666, 75129333, 37564666, -187834497, 69657706 },   /* band 5, low-pass 2500 Hz */
    { 203226142, 406452284, 203226142, 390370540, 154098572 },   /* band 6, low-pass 7000 Hz */
    { 268435456, 0, 0, 0, 0 }   /* band 7, no low-pass: passes the signal unchanged (1.0 in Q28) */
};

static biquad_state_t s_hp_state[BAND_COUNT] MIC_MAYBE_UNUSED;
static biquad_state_t s_lp_state[BAND_COUNT] MIC_MAYBE_UNUSED;

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
MIC_MAYBE_UNUSED static void make_bar(char *out, uint32_t value)
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
MIC_MAYBE_UNUSED static void bands_reset(int16_t first)
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
MIC_MAYBE_UNUSED static void bands_step(int16_t x, uint64_t sq[BAND_COUNT])
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
MIC_MAYBE_UNUSED static uint32_t band_level(uint64_t sq, uint32_t n)
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

/*
 * Two switches for finding out why the live playback sounds worse than the recorded playback.
 * APP_MIC_LIVE_TEST_TONE 1 sends a steady 1 kHz tone (amplitude 3000 before the gain) through the whole
 * live path instead of the microphone: if it is clean for a minute, the streaming to the speaker has
 * no gaps and the difference lies in the level or the microphone side; clicks or crackle mean that
 * the speaker runs out of data. APP_MIC_LIVE_BANDS 0 switches the eight measuring bands off, which are
 * the heaviest part of the loop; the line then shows only peak, ac and dc. The line "[mic] load" tells
 * how much of each second the loop waits for the microphone and for the speaker.
 */
#ifndef APP_MIC_LIVE_TEST_TONE
#define APP_MIC_LIVE_TEST_TONE 0
#endif
#ifndef APP_MIC_LIVE_BANDS
#define APP_MIC_LIVE_BANDS 1
#endif

#if APP_MIC_LIVE_TEST_TONE
/* One period of 1 kHz at 16000 Hz is 16 samples: 3000 times sin(2 pi k / 16). */
static const int16_t s_tone[16] = { 0, 1148, 2121, 2772, 3000, 2772, 2121, 1148, 0, -1148, -2121, -2772, -3000, -2772, -2121, -1148 };
#endif

typedef struct live_acc {
    int32_t  peak;
    int64_t  sum;
    uint64_t sq;
    uint64_t band_sq[BAND_COUNT];
    uint32_t n;
} live_acc_t;

MIC_MAYBE_UNUSED static void live_print(const live_acc_t *a, bool playing, bool muted)
{
#if !APP_MIC_LIVE_BANDS
    {
        uint64_t ms;
        uint64_t mean;

        if (a->n == 0U) {
            return;
        }
        mean = (uint64_t)(a->sum * a->sum) / a->n;
        ms = ((a->sq > mean) ? (a->sq - mean) : 0U) / a->n;
        printf("[mic] peak=%5d ac=%5u dc=%6d | bands off | %s\n", (int)a->peak, (unsigned)isqrt64(ms),
               (int)(a->sum / (int64_t)a->n), muted ? "MUTED by howl guard" : (playing ? "playing" : "print only"));
        return;
    }
#else
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
#endif
}

/*
 * Howl guard. The speaker is close to the microphone, so when the output stays loud the guard cannot
 * tell by the level alone whether it is feedback or a person talking loudly into the microphone (a
 * log with speech at 5 cm tripped the old guard and silenced the speaker although there was no howl).
 * The trigger is the rms of the output (what the speaker really gets, after the limit to 16 bit) over
 * one window, not its peak: speech has peaks about four times its rms, so a loud voice reaches the
 * full scale with a peak but only about a third of it as rms (a log with the microphone at the mouth
 * gave about 8400 at the speaker), while a howl is a tone that grows until it clips and has an rms of
 * 20000 or more. It then tests it: when that rms is above APP_MIC_LIVE_HOWL_RMS for HOWL_WINDOWS windows
 * in a row, the speaker is muted for GUARD_SETTLE_WINDOWS (the audio already queued in the I2S buffers
 * still plays during that time), then the input peak is watched for GUARD_PROBE_WINDOWS. If the input
 * is still above APP_MIC_LIVE_GUARD_VOICE_PEAK, somebody is still talking: the speaker comes back at
 * once. If it fell to the room level, the loop was feedback and the speaker stays muted until
 * APP_MIC_LIVE_MUTE_MS is over. After a voice verdict the guard waits GUARD_COOLDOWN_WINDOWS before it
 * can trip again. A window is LIVE_WINDOW samples (100 ms). The times are chosen by design, not
 * measured. The voice level comes from one 3 s recording at about 20 cm, filtered at 100 Hz: the
 * quiet 100 ms blocks had a peak of at most 97 and the blocks with speech at least 1202.
 */
#ifndef APP_MIC_LIVE_GUARD_VOICE_PEAK
#define APP_MIC_LIVE_GUARD_VOICE_PEAK 500
#endif
#define GUARD_SETTLE_WINDOWS   2U
#define GUARD_PROBE_WINDOWS    2U
#define GUARD_COOLDOWN_WINDOWS 10U

typedef enum guard_phase {
    GUARD_ARMED = 0,     /* speaker on, watching the output level */
    GUARD_SETTLE,        /* speaker muted, waiting for the queued audio to play out */
    GUARD_PROBE,         /* speaker muted, measuring the input */
    GUARD_HOLD           /* feedback confirmed, speaker muted until the time is over */
} guard_phase_t;

typedef enum guard_event {
    GUARD_EVENT_NONE = 0,
    GUARD_EVENT_TRIPPED,     /* the speaker was just muted for the test */
    GUARD_EVENT_VOICE,       /* test says voice, the speaker must be switched on again */
    GUARD_EVENT_FEEDBACK,    /* test says feedback, the speaker stays muted */
    GUARD_EVENT_RELEASED     /* hold time is over, the speaker must be switched on again */
} guard_event_t;

typedef struct howl_guard {
    guard_phase_t phase;
    uint32_t loud;           /* loud windows in a row */
    uint32_t left;           /* windows left in the current phase */
    uint32_t cool;           /* windows left in which the guard cannot trip */
    int32_t trigger_in;      /* largest input peak while the output was loud */
    int32_t probe_in;        /* largest input peak during the probe */
} howl_guard_t;

static void guard_reset(howl_guard_t *g)
{
    memset(g, 0, sizeof(*g));
}

/* True while the speaker must not be written. */
static bool guard_muted(const howl_guard_t *g)
{
    return g->phase != GUARD_ARMED;
}

/*
 * Called once per window with the largest input sample (after the high-pass, before the gain) and the
 * rms of the output samples (after the gain and the limit to 16 bit) of that window.
 */
static guard_event_t guard_window(howl_guard_t *g, int32_t in_peak, int32_t out_rms)
{
    switch (g->phase) {
    case GUARD_ARMED:
        if (g->cool > 0U) {
            g->cool--;
            g->loud = 0U;
            return GUARD_EVENT_NONE;
        }
        if (out_rms >= (int32_t)APP_MIC_LIVE_HOWL_RMS) {
            if (in_peak > g->trigger_in) {
                g->trigger_in = in_peak;
            }
            g->loud++;
            if (g->loud >= HOWL_WINDOWS) {
                g->phase = GUARD_SETTLE;
                g->left = GUARD_SETTLE_WINDOWS;
                g->probe_in = 0;
                g->loud = 0U;
                return GUARD_EVENT_TRIPPED;
            }
        } else {
            g->loud = 0U;
            g->trigger_in = 0;
        }
        return GUARD_EVENT_NONE;
    case GUARD_SETTLE:
        if (--g->left == 0U) {
            g->phase = GUARD_PROBE;
            g->left = GUARD_PROBE_WINDOWS;
        }
        return GUARD_EVENT_NONE;
    case GUARD_PROBE:
        if (in_peak > g->probe_in) {
            g->probe_in = in_peak;
        }
        if (--g->left == 0U) {
            if (g->probe_in >= (int32_t)APP_MIC_LIVE_GUARD_VOICE_PEAK) {
                g->phase = GUARD_ARMED;
                g->cool = GUARD_COOLDOWN_WINDOWS;
                g->trigger_in = 0;
                return GUARD_EVENT_VOICE;
            }
            {
                const uint32_t used = GUARD_SETTLE_WINDOWS + GUARD_PROBE_WINDOWS;
                const uint32_t total = APP_MIC_LIVE_MUTE_MS / 100U;

                g->phase = GUARD_HOLD;
                g->left = (total > used) ? (total - used) : 1U;
            }
            return GUARD_EVENT_FEEDBACK;
        }
        return GUARD_EVENT_NONE;
    case GUARD_HOLD:
    default:
        if (--g->left == 0U) {
            g->phase = GUARD_ARMED;
            g->trigger_in = 0;
            return GUARD_EVENT_RELEASED;
        }
        return GUARD_EVENT_NONE;
    }
}

/*
 * Automatic gain for the live playback. The recorded playback scales the whole recording so that its
 * peak reaches APP_MIC_PLAY_PEAK (at most APP_MIC_MAX_GAIN times). The live path cannot see the whole
 * sound, so it does the same per chunk: the gain that brings the peak of this chunk to
 * APP_MIC_LIVE_AGC_TARGET is used at once when it is lower than the current gain (no clipping), and is
 * approached slowly when it is higher (no pumping between words). A chunk whose peak is below
 * APP_MIC_LIVE_AGC_GATE is room noise: the gain is held, so the noise is not raised. The gain is in
 * Q8 (256 is x1). Returns the gain for the next chunk.
 */
static int32_t agc_next_gain(int32_t cur_q8, int32_t peak)
{
    int32_t want_q8;
    int32_t step;

    if (peak < (int32_t)APP_MIC_LIVE_AGC_GATE) {
        return cur_q8;
    }
    want_q8 = (int32_t)(((int64_t)APP_MIC_LIVE_AGC_TARGET * 256) / peak);
    if (want_q8 < 256) {
        want_q8 = 256;
    } else if (want_q8 > ((int32_t)APP_MIC_LIVE_AGC_MAX * 256)) {
        want_q8 = (int32_t)APP_MIC_LIVE_AGC_MAX * 256;
    }
    if (want_q8 <= cur_q8) {
        return want_q8;
    }
    step = (want_q8 - cur_q8) / (int32_t)APP_MIC_LIVE_AGC_RELEASE;
    if (step < 1) {
        step = 1;
    }
    return cur_q8 + step;
}

/*
 * Continuous monitor with live playback (see mic_app.h). Reads the microphone in 8 ms chunks,
 * measures every sample, removes the DC offset, multiplies by APP_MIC_LIVE_GAIN and writes the
 * result to the speaker. Prints one line per second. Returns RESULT_OK when it ended by itself.
 */
MIC_MAYBE_UNUSED static test_result_t run_live_monitor(speaker_t *spk, inmp441_t *mic, bool speaker_free)
{
    const bool forever = (APP_MIC_LIVE_MS == APP_MIC_LIVE_FOREVER);
    const uint32_t total = forever ? 0U : ((RATE_HZ / 1000U) * APP_MIC_LIVE_MS);
    const bool want_play = (APP_MIC_LIVE_PLAY != 0) && speaker_free && (spk != NULL);
    live_acc_t acc;
    uint32_t done = 0U;
    uint32_t win_n = 0U;
    uint32_t windows = 0U;
    howl_guard_t guard;
    int32_t win_in_peak = 0;
    uint64_t win_out_sq = 0U;
#if !APP_MIC_LIVE_QUIET
    uint32_t read_ms = 0U;
    uint32_t write_ms = 0U;
    uint32_t t0 = 0U;
    uint32_t t1 = 0U;
#endif
    int32_t gain_q8 = APP_MIC_LIVE_AGC ? ((int32_t)APP_MIC_LIVE_AGC_START * 256) : ((int32_t)APP_MIC_LIVE_GAIN * 256);
#if APP_MIC_LIVE_TEST_TONE
    uint32_t tone_phase = 0U;
#endif
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
    guard_reset(&guard);
    printf("[mic] live monitor%s, one line per second. Make a sound: silence, hum mmm (LOW), say aaa (MID), "
           "hiss sss (HIGH), clap (all three)\n", forever ? " (never ends)" : "");
    if (want_play) {
        printf("[mic] live playback ON: volume %u percent, gain x%d (start value when AGC is on). If you hear a howl, move the speaker "
               "away from the microphone or lower APP_MIC_LIVE_VOLUME\n",
               (unsigned)APP_MIC_LIVE_VOLUME, (int)APP_MIC_LIVE_GAIN);
#if APP_MIC_LIVE_TEST_TONE
        printf("[mic] TEST TONE: a 1 kHz tone replaces the microphone in the live path, listen for clicks\n");
#endif
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
        int32_t chunk_peak = 0;
        int32_t gain_new;
        const bool playing = spk_started && (sret == SPEAKER_OK);

#if !APP_MIC_LIVE_QUIET
        sx_get_time_ms(&t0);
#endif
        ret = inmp441_read(mic, s_chunk, LIVE_CHUNK, &got, READ_TIMEOUT_MS);
#if !APP_MIC_LIVE_QUIET
        sx_get_time_ms(&t1);
        read_ms += t1 - t0;
#endif
        if (ret != INMP441_OK) {
            break;
        }
        if (first && (got > 0U)) {
#if APP_MIC_LIVE_BANDS && !APP_MIC_LIVE_QUIET
            bands_reset(s_chunk[0]);
#endif
#if APP_MIC_PLAY_HPF
            play_hpf_reset(s_chunk[0]);
#else
            dc_q8 = (int32_t)s_chunk[0] * 256;
#endif
            first = false;
        }

        /* Pass 1: measure, remove DC (high-pass) and find the peak of this chunk. */
        for (i = 0U; i < got; i++) {
            int32_t v = s_chunk[i];
            int32_t f;
            int32_t fa;

            (void)v;     /* not used when the loop is quiet and the high-pass is on */
#if !APP_MIC_LIVE_QUIET
            {
                int32_t a = (v < 0) ? -v : v;

                if (a > acc.peak) {
                    acc.peak = a;
                }
                acc.sum += v;
                acc.sq += (uint64_t)((int64_t)v * v);
            }
#if APP_MIC_LIVE_BANDS
            bands_step(s_chunk[i], acc.band_sq);
#endif
#endif

#if APP_MIC_PLAY_HPF
            /* Output: 100 Hz high-pass (it also removes DC). */
            f = (int32_t)play_hpf_step(s_chunk[i]);
#else
            /* Output: remove the slow DC estimate. */
            dc_q8 += ((v * 256) - dc_q8) / 256;
            f = v - (dc_q8 / 256);
            if (f > 32767) {
                f = 32767;
            } else if (f < -32768) {
                f = -32768;
            }
#endif
#if APP_MIC_LIVE_TEST_TONE
            f = (int32_t)s_tone[tone_phase];
            tone_phase = (tone_phase + 1U) & 15U;
#endif
            fa = (f < 0) ? -f : f;
            if (fa > win_in_peak) {
                win_in_peak = fa;
            }
            if (fa > chunk_peak) {
                chunk_peak = fa;
            }
            s_chunk[i] = (int16_t)f;
        }

        /*
         * Pass 2: gain, with saturation. A rise of the gain is ramped across the chunk so it makes no
         * click. A fall is applied to the whole chunk at once, because the loud part that caused it may
         * be at the start of the chunk and a ramp would let that part clip.
         */
#if APP_MIC_LIVE_AGC
        gain_new = agc_next_gain(gain_q8, chunk_peak);
#else
        gain_new = gain_q8;
        (void)chunk_peak;
#endif
        for (i = 0U; i < got; i++) {
            int32_t g = (gain_new <= gain_q8) ? gain_new
                        : (gain_q8 + (((gain_new - gain_q8) * (int32_t)(i + 1U)) / (int32_t)got));
            int32_t out = ((int32_t)s_chunk[i] * g) / 256;

            if (out > 32767) {
                out = 32767;
            } else if (out < -32768) {
                out = -32768;
            }
            win_out_sq += (uint64_t)((int64_t)out * out);
            s_chunk[i] = (int16_t)out;
        }
        if (got > 0U) {
            gain_q8 = gain_new;
        }
#if !APP_MIC_LIVE_QUIET
        acc.n += (uint32_t)got;
#endif

        if (playing && !guard_muted(&guard) && (got > 0U)) {
            size_t written = 0U;

#if !APP_MIC_LIVE_QUIET
            sx_get_time_ms(&t0);
#endif
            sret = speaker_write(spk, s_chunk, got, &written, WRITE_TIMEOUT_MS);
#if !APP_MIC_LIVE_QUIET
            sx_get_time_ms(&t1);
            write_ms += t1 - t0;
#endif
            if (sret != SPEAKER_OK) {
                printf("[mic] speaker_write failed: %d, the monitor prints only from now on\n", sret);
            }
        }
        done += (uint32_t)got;
        win_n += (uint32_t)got;

        if (win_n >= LIVE_WINDOW) {
            if (playing) {
                const guard_event_t ev = guard_window(&guard, win_in_peak,
                                                      (int32_t)isqrt64(win_out_sq / win_n));

                /* Prints of the guard are rare, but one print stalls the loop, so they are off when quiet. */
#if !APP_MIC_LIVE_QUIET
                if (ev == GUARD_EVENT_TRIPPED) {
                    printf("[mic] howl guard: output rms above %d for %u ms in a row, speaker muted to test it\n",
                           (int)APP_MIC_LIVE_HOWL_RMS, (unsigned)(HOWL_WINDOWS * 100U));
                } else if (ev == GUARD_EVENT_VOICE) {
                    printf("[mic] howl guard: input stays loud with the speaker muted (peak %d of %d), "
                           "so it is a voice, speaker back on\n", (int)guard.probe_in, (int)guard.trigger_in);
                } else if (ev == GUARD_EVENT_FEEDBACK) {
                    printf("[mic] howl guard: input fell to peak %d of %d with the speaker muted, so it was "
                           "feedback, speaker stays muted for %u ms in total\n",
                           (int)guard.probe_in, (int)guard.trigger_in, (unsigned)APP_MIC_LIVE_MUTE_MS);
                } else if (ev == GUARD_EVENT_RELEASED) {
                    printf("[mic] howl guard: mute time over, speaker back on\n");
                }
#endif
                if ((ev == GUARD_EVENT_VOICE) || (ev == GUARD_EVENT_RELEASED)) {
#if APP_MIC_LIVE_PREROLL_MS > 0U
                    /* The speaker buffers ran empty while it was muted: build the lead again. */
                    sret = write_silence(spk, APP_MIC_LIVE_PREROLL_MS);
                    if (sret != SPEAKER_OK) {
                        printf("[mic] speaker pre-roll failed: %d, the monitor prints only\n", sret);
                    }
#endif
                }
            }
            win_n = 0U;
            win_in_peak = 0;
            win_out_sq = 0U;
            windows++;
            if (windows >= LIVE_REPORT_WINDOWS) {
#if !APP_MIC_LIVE_QUIET
                live_print(&acc, playing, guard_muted(&guard));
                {
                    const uint32_t span = LIVE_REPORT_WINDOWS * 100U;
                    const uint32_t idle = read_ms + write_ms;

                    printf("[mic] load: per %u ms the loop waited %u ms for the microphone and %u ms for the speaker, own work about %u ms\n",
                           (unsigned)span, (unsigned)read_ms, (unsigned)write_ms,
                           (unsigned)((idle < span) ? (span - idle) : 0U));
                }
                read_ms = 0U;
                write_ms = 0U;
                memset(&acc, 0, sizeof(acc));
#endif
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
        printf("[mic] playback high-pass 100 Hz: peak %d to %d\n", (int)raw_peak, (int)st.peak);
        {
            int32_t head_peak = 0;
            int32_t tail_peak = 0;

            for (k = 0U; k < got; k++) {
                const int32_t a = (rec[k] < 0) ? -(int32_t)rec[k] : (int32_t)rec[k];

                if (k < (RATE_HZ / 5U)) {
                    head_peak = (a > head_peak) ? a : head_peak;
                } else {
                    tail_peak = (a > tail_peak) ? a : tail_peak;
                }
            }
            printf("[mic] record peak after high-pass: first 200 ms %d, rest %d (the gain follows the larger)\n",
                   (int)head_peak, (int)tail_peak);
        }
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

/*
 * Live monitor in segments (see APP_MIC_LIVE_SEGMENT in mic_app.h). Each round is the record and play
 * step on a short piece: the same read, the same high-pass, the same scaling and the same speaker
 * start and stop as run_record_play. The microphone and the speaker never run together. Runs until an
 * error, so it returns only RESULT_FAILED or RESULT_SKIPPED.
 */
MIC_MAYBE_UNUSED static test_result_t run_live_segments(speaker_t *spk, inmp441_t *mic)
{
    const size_t total = ((size_t)RATE_HZ / 1000U) * APP_MIC_SEG_MS;
    block_stats_t st;
    int16_t *rec;
    int ret = SPEAKER_OK;
    int mret = INMP441_OK;

    if ((spk == NULL) || (mic == NULL) || (total == 0U)) {
        printf("[mic] live segments skipped: the speaker is not available\n");
        return RESULT_SKIPPED;
    }
    rec = (int16_t *)malloc(total * sizeof(int16_t));
    if (rec == NULL) {
        printf("[mic] live segments skipped: the heap cannot hold %u bytes, lower APP_MIC_SEG_MS\n",
               (unsigned)(total * sizeof(int16_t)));
        return RESULT_SKIPPED;
    }
    printf("[mic] live in segments of %u ms (record, then play, never together), no more logs\n",
           (unsigned)APP_MIC_SEG_MS);
    ret = speaker_set_volume(spk, (uint8_t)APP_MIC_PLAY_VOLUME);

    while ((ret == SPEAKER_OK) && (mret == INMP441_OK)) {
        size_t got = 0U;
        size_t written = 0U;
        int32_t gain;

        mret = inmp441_start(mic);
        if (mret == INMP441_OK) {
            mret = discard_settle(mic);
        }
        if (mret == INMP441_OK) {
            mret = record_samples(mic, rec, total, &got);
        }
        (void)inmp441_stop(mic);
        if ((mret != INMP441_OK) || (got == 0U)) {
            break;
        }

        compute_stats(rec, got, &st);
#if APP_MIC_PLAY_HPF
        {
            size_t k;

            play_hpf_reset(rec[0]);
            for (k = 0U; k < got; k++) {
                rec[k] = play_hpf_step(rec[k]);
            }
            compute_stats(rec, got, &st);
        }
#endif
        if (st.peak < (int32_t)APP_MIC_SEG_GATE) {
            continue;       /* room noise only: play nothing */
        }
        gain = APP_MIC_PLAY_PEAK / st.peak;
        if (gain < 1) {
            gain = 1;
        }
        if (gain > APP_MIC_MAX_GAIN) {
            gain = APP_MIC_MAX_GAIN;
        }
        amplify(rec, got, gain);

        ret = speaker_start(spk);
        if (ret == SPEAKER_OK) {
            ret = speaker_write(spk, rec, got, &written, WRITE_TIMEOUT_MS);
            if (ret == SPEAKER_OK) {
                ret = write_silence(spk, 100U);
            }
            (void)speaker_stop(spk);
        }
    }
    free(rec);
    printf("[mic] live segments stopped: microphone %d, speaker %d\n", mret, ret);
    return RESULT_FAILED;
}

#if APP_MIC_DIAG && APP_MIC_PLAY_HPF
#define DIAG_WINDOW      (RATE_HZ / 10U)    /* 100 ms window for the spread of the noise */
#define DIAG_TRACE_WIN   (RATE_HZ / 4U)     /* 250 ms window of the start-up trace */
#define DIAG_TRACE_MAX   24U
#define DIAG_PHASES      5U
#define DIAG_GUARD_MS    400U               /* dropped at the start of each phase, after a speaker start or stop */

static int16_t s_diag_out[LIVE_CHUNK];

typedef struct diag_acc {
    int64_t  sum;       /* sum of the raw samples */
    uint64_t sq;        /* sum of squares of the raw samples */
    uint64_t fsq;       /* sum of squares after the 100 Hz high-pass */
    int32_t  peak;      /* largest absolute sample after the high-pass */
    int32_t  raw_peak;  /* largest absolute raw sample */
    uint32_t n;
} diag_acc_t;

typedef struct diag_phase_res {
    diag_acc_t acc;
    uint32_t   win_min;  /* smallest and largest rms_hpf of a 100 ms window in this phase */
    uint32_t   win_max;
    uint64_t   win_fsq;
    uint32_t   win_n;
} diag_phase_res_t;

static void acc_add(diag_acc_t *a, int32_t v, int32_t f)
{
    const int32_t fa = (f < 0) ? -f : f;
    const int32_t va = (v < 0) ? -v : v;

    a->sum += v;
    a->sq += (uint64_t)((int64_t)v * v);
    a->fsq += (uint64_t)((int64_t)f * f);
    if (fa > a->peak) {
        a->peak = fa;
    }
    if (va > a->raw_peak) {
        a->raw_peak = va;
    }
    a->n++;
}

static uint32_t acc_rms_hpf(const diag_acc_t *a)
{
    return (a->n == 0U) ? 0U : isqrt64(a->fsq / a->n);
}

/* Rms of the raw samples with only the mean removed. */
static uint32_t acc_rms_raw(const diag_acc_t *a)
{
    uint64_t mean_sq;

    if (a->n == 0U) {
        return 0U;
    }
    mean_sq = (uint64_t)((a->sum * a->sum) / (int64_t)a->n);
    return isqrt64((a->sq > mean_sq) ? ((a->sq - mean_sq) / a->n) : 0U);
}

static int32_t acc_dc(const diag_acc_t *a)
{
    return (a->n == 0U) ? 0 : (int32_t)(a->sum / (int64_t)a->n);
}

/*
 * The diagnostic. The microphone is started ONCE and never stopped until the end, so its start-up
 * transient happens once. First a trace of APP_MIC_DIAG_SETTLE_MS shows how the DC and the noise
 * decay after the start. Then five phases of APP_MIC_DIAG_MS: mic only, mic with the speaker running
 * on digital silence, mic only, speaker again, mic only. Only the speaker is started and stopped
 * between phases; the microphone keeps being read, and the high-pass filter is never reset. Nothing is
 * printed while the microphone runs, all lines come at the end. There is no automatic conclusion:
 * the three mic only phases show how much the microphone alone varies, and only a clear step in the
 * speaker phases beyond that spread means something.
 */
static test_result_t run_diag(speaker_t *spk, inmp441_t *mic)
{
    static const char *const name[DIAG_PHASES] = { "A1 mic only", "B1 mic + speaker on silence", "A2 mic only",
                                                   "B2 mic + speaker on silence", "A3 mic only" };
    static const bool spk_on[DIAG_PHASES] = { false, true, false, true, false };
    static diag_acc_t trace[DIAG_TRACE_MAX];
    static diag_phase_res_t res[DIAG_PHASES];
    uint32_t trace_windows = (RATE_HZ / 1000U) * APP_MIC_DIAG_SETTLE_MS / DIAG_TRACE_WIN;
    uint32_t trace_total;
    uint32_t done = 0U;
    bool first = true;
    bool spk_running = false;
    bool spk_failed = false;
    int ret;
    unsigned k;
    size_t i;

    if ((spk == NULL) || (mic == NULL)) {
        printf("[mic] diag skipped: the speaker is not available\n");
        return RESULT_SKIPPED;
    }
    if (trace_windows > DIAG_TRACE_MAX) {
        trace_windows = DIAG_TRACE_MAX;
    }
    trace_total = trace_windows * DIAG_TRACE_WIN;
    memset(trace, 0, sizeof(trace));
    memset(res, 0, sizeof(res));
    memset(s_diag_out, 0, sizeof(s_diag_out));

    printf("[mic] diag starts in 3 s: KEEP THE ROOM SILENT for about %u s (do not talk, do not touch the board). "
           "Results are printed at the end\n",
           (unsigned)((APP_MIC_DIAG_SETTLE_MS + (DIAG_PHASES * (APP_MIC_DIAG_MS + DIAG_GUARD_MS + 100U))) / 1000U));
    sx_delay_ms(3000U);

    ret = inmp441_start(mic);
    while ((ret == INMP441_OK) && (done < trace_total)) {
        size_t got = 0U;

        ret = inmp441_read(mic, s_chunk, LIVE_CHUNK, &got, READ_TIMEOUT_MS);
        if (ret != INMP441_OK) {
            break;
        }
        if (first && (got > 0U)) {
            play_hpf_reset(s_chunk[0]);
            first = false;
        }
        for (i = 0U; (i < got) && (done < trace_total); i++) {
            acc_add(&trace[done / DIAG_TRACE_WIN], (int32_t)s_chunk[i], (int32_t)play_hpf_step(s_chunk[i]));
            done++;
        }
    }

    for (k = 0U; (k < DIAG_PHASES) && (ret == INMP441_OK); k++) {
        diag_phase_res_t *r = &res[k];
        uint32_t guard = (RATE_HZ / 1000U) * DIAG_GUARD_MS;
        uint32_t left = (RATE_HZ / 1000U) * APP_MIC_DIAG_MS;

        r->win_min = UINT32_MAX;
        if (spk_on[k]) {
            if ((speaker_set_volume(spk, (uint8_t)APP_MIC_LIVE_VOLUME) != SPEAKER_OK) ||
                (speaker_start(spk) != SPEAKER_OK)) {
                spk_failed = true;
                break;
            }
            spk_running = true;
            if (write_silence(spk, APP_MIC_LIVE_PREROLL_MS) != SPEAKER_OK) {
                spk_failed = true;
                break;
            }
        }
        while ((ret == INMP441_OK) && (left > 0U)) {
            size_t got = 0U;

            ret = inmp441_read(mic, s_chunk, LIVE_CHUNK, &got, READ_TIMEOUT_MS);
            if (ret != INMP441_OK) {
                break;
            }
            for (i = 0U; (i < got) && (left > 0U); i++) {
                const int32_t f = (int32_t)play_hpf_step(s_chunk[i]);

                if (guard > 0U) {
                    guard--;
                    continue;
                }
                acc_add(&r->acc, (int32_t)s_chunk[i], f);
                r->win_fsq += (uint64_t)((int64_t)f * f);
                r->win_n++;
                if (r->win_n >= DIAG_WINDOW) {
                    const uint32_t w = isqrt64(r->win_fsq / r->win_n);

                    if (w < r->win_min) {
                        r->win_min = w;
                    }
                    if (w > r->win_max) {
                        r->win_max = w;
                    }
                    r->win_fsq = 0U;
                    r->win_n = 0U;
                }
                left--;
            }
            if (spk_on[k] && (got > 0U)) {
                size_t written = 0U;

                if (speaker_write(spk, s_diag_out, got, &written, WRITE_TIMEOUT_MS) != SPEAKER_OK) {
                    spk_failed = true;
                    break;
                }
            }
        }
        if (spk_running) {
            (void)speaker_stop(spk);
            spk_running = false;
        }
        if (spk_failed) {
            break;
        }
    }
    (void)inmp441_stop(mic);
    if (spk_running) {
        (void)speaker_stop(spk);
    }
    if ((ret != INMP441_OK) || spk_failed) {
        printf("[mic] diag FAILED: mic code %d, speaker failed %d\n", ret, (int)spk_failed);
        return RESULT_FAILED;
    }

    printf("[mic] diag trace after the microphone start, one line per 250 ms (dc and rms of the raw signal, rms after the 100 Hz high-pass):\n");
    for (k = 0U; k < trace_windows; k++) {
        printf("[mic] diag trace t=%u ms: dc=%d ac=%u rms_hpf=%u peak_raw=%d\n", (unsigned)((k + 1U) * 250U),
               (int)acc_dc(&trace[k]), (unsigned)acc_rms_raw(&trace[k]), (unsigned)acc_rms_hpf(&trace[k]),
               (int)trace[k].raw_peak);
    }
    for (k = 0U; k < DIAG_PHASES; k++) {
        printf("[mic] diag %s: peak=%d rms_hpf=%u rms_raw=%u dc=%d win100_min=%u win100_max=%u\n", name[k],
               (int)res[k].acc.peak, (unsigned)acc_rms_hpf(&res[k].acc), (unsigned)acc_rms_raw(&res[k].acc),
               (int)acc_dc(&res[k].acc), (unsigned)((res[k].win_min == UINT32_MAX) ? 0U : res[k].win_min),
               (unsigned)res[k].win_max);
    }
    return RESULT_OK;
}
#endif

#if APP_MIC_COUPLING && APP_MIC_PLAY_HPF
#define CPL_TABLE        256U
#define CPL_FREQS        14U
#define CPL_AMPS         1U
#define CPL_PHASES       (1U + (CPL_FREQS * CPL_AMPS))
#define CPL_GUARD_MS     300U
#define CPL_MEASURE_MS   1000U
#define CPL_SETTLE_MS    3500U
#define CPL_PI           3.14159265f

static int16_t s_cpl_sine[CPL_TABLE];
static int16_t s_cpl_out[LIVE_CHUNK];

typedef struct cpl_goertzel {
    float coeff;
    float s1;
    float s2;
} cpl_goertzel_t;

typedef struct cpl_result {
    uint32_t freq;
    int32_t  out_amp;     /* digital amplitude sent to the speaker driver after the volume scaling */
    uint32_t mic_amp;     /* amplitude of the tone found in the microphone signal at freq */
    uint32_t off_amp;     /* amplitude found at 1.37 times freq, to show the floor of the measurement */
    uint32_t rms_hpf;
} cpl_result_t;

static void cpl_gz_init(cpl_goertzel_t *g, float freq)
{
    g->coeff = 2.0f * cosf((2.0f * CPL_PI * freq) / (float)RATE_HZ);
    g->s1 = 0.0f;
    g->s2 = 0.0f;
}

static void cpl_gz_add(cpl_goertzel_t *g, float x)
{
    const float s = x + (g->coeff * g->s1) - g->s2;

    g->s2 = g->s1;
    g->s1 = s;
}

/* Amplitude of the sine component, same unit as the samples. */
static uint32_t cpl_gz_amp(const cpl_goertzel_t *g, uint32_t n)
{
    float p = (g->s1 * g->s1) + (g->s2 * g->s2) - (g->coeff * g->s1 * g->s2);

    if ((p < 0.0f) || (n == 0U)) {
        return 0U;
    }
    return (uint32_t)((2.0f * sqrtf(p)) / (float)n);
}

/*
 * One phase: the speaker plays a sine of freq and digital amplitude amp (before the volume) while
 * the microphone keeps being read. The first CPL_GUARD_MS are dropped, the next CPL_MEASURE_MS are
 * measured with two Goertzel filters (at freq, and at 1.37 times freq). amp 0 is the baseline.
 * Returns false on an I/O error.
 */
static bool cpl_phase(speaker_t *spk, inmp441_t *mic, uint32_t freq, int32_t amp, cpl_result_t *res)
{
    const uint32_t phase_inc = (uint32_t)((((uint64_t)freq) << 32) / RATE_HZ);
    uint32_t guard = (RATE_HZ / 1000U) * CPL_GUARD_MS;
    uint32_t left = (RATE_HZ / 1000U) * CPL_MEASURE_MS;
    const uint32_t measure_total = left;
    uint32_t phase = 0U;
    uint64_t fsq = 0U;
    cpl_goertzel_t at_f;
    cpl_goertzel_t at_off;
    size_t i;

    cpl_gz_init(&at_f, (float)freq);
    cpl_gz_init(&at_off, ((float)freq) * 1.37f);
    memset(res, 0, sizeof(*res));
    res->freq = freq;
    res->out_amp = (amp * (int32_t)APP_MIC_LIVE_VOLUME) / 100;

    while (left > 0U) {
        size_t got = 0U;
        size_t written = 0U;

        if (inmp441_read(mic, s_chunk, LIVE_CHUNK, &got, READ_TIMEOUT_MS) != INMP441_OK) {
            return false;
        }
        for (i = 0U; i < got; i++) {
            const int32_t f = (int32_t)play_hpf_step(s_chunk[i]);

            s_cpl_out[i] = (int16_t)(((int32_t)s_cpl_sine[phase >> 24] * amp) / 32768);
            phase += phase_inc;
            if (guard > 0U) {
                guard--;
            } else if (left > 0U) {
                cpl_gz_add(&at_f, (float)f);
                cpl_gz_add(&at_off, (float)f);
                fsq += (uint64_t)((int64_t)f * f);
                left--;
            }
        }
        if ((got > 0U) && (speaker_write(spk, s_cpl_out, got, &written, WRITE_TIMEOUT_MS) != SPEAKER_OK)) {
            return false;
        }
    }
    res->mic_amp = cpl_gz_amp(&at_f, measure_total);
    res->off_amp = cpl_gz_amp(&at_off, measure_total);
    res->rms_hpf = isqrt64(fsq / measure_total);
    return true;
}

/*
 * Coupling probe, see APP_MIC_COUPLING in mic_app.h. Microphone and speaker both run all the time.
 * Nothing is printed while they run. There is no automatic conclusion.
 */
static test_result_t run_coupling(speaker_t *spk, inmp441_t *mic)
{
    static const uint32_t freqs[CPL_FREQS] = { 400U, 500U, 600U, 700U, 800U, 900U, 1000U, 1100U, 1200U, 1400U,
                                               1600U, 2000U, 2500U, 3000U };
    static const int32_t amps[CPL_AMPS] = { 3000 };
    static cpl_result_t res[CPL_PHASES];
    uint32_t settle = (RATE_HZ / 1000U) * CPL_SETTLE_MS;
    bool first = true;
    bool ok = true;
    bool spk_running = false;
    unsigned k = 0U;
    unsigned fi;
    unsigned ai;
    size_t i;
    int ret;

    if ((spk == NULL) || (mic == NULL)) {
        printf("[mic] coupling skipped: the speaker is not available\n");
        return RESULT_SKIPPED;
    }
    for (i = 0U; i < CPL_TABLE; i++) {
        s_cpl_sine[i] = (int16_t)(32767.0f * sinf((2.0f * CPL_PI * (float)i) / (float)CPL_TABLE));
    }
    memset(res, 0, sizeof(res));
    memset(s_cpl_out, 0, sizeof(s_cpl_out));

    printf("[mic] coupling probe starts in 3 s: KEEP THE ROOM SILENT for about %u s. Tones play at volume %u percent. "
           "Results are printed at the end\n",
           (unsigned)((CPL_SETTLE_MS + (CPL_PHASES * (CPL_GUARD_MS + CPL_MEASURE_MS))) / 1000U),
           (unsigned)APP_MIC_LIVE_VOLUME);
    sx_delay_ms(3000U);

    if ((speaker_set_volume(spk, (uint8_t)APP_MIC_LIVE_VOLUME) != SPEAKER_OK) || (speaker_start(spk) != SPEAKER_OK)) {
        printf("[mic] coupling: speaker start failed\n");
        return RESULT_FAILED;
    }
    spk_running = true;
    ret = inmp441_start(mic);
    if ((ret == INMP441_OK) && (write_silence(spk, APP_MIC_LIVE_PREROLL_MS) != SPEAKER_OK)) {
        ret = INMP441_ERR_BUS;
    }
    while ((ret == INMP441_OK) && (settle > 0U)) {
        size_t got = 0U;
        size_t written = 0U;

        ret = inmp441_read(mic, s_chunk, LIVE_CHUNK, &got, READ_TIMEOUT_MS);
        if (ret != INMP441_OK) {
            break;
        }
        if (first && (got > 0U)) {
            play_hpf_reset(s_chunk[0]);
            first = false;
        }
        for (i = 0U; i < got; i++) {
            (void)play_hpf_step(s_chunk[i]);
        }
        settle = (settle > (uint32_t)got) ? (settle - (uint32_t)got) : 0U;
        if ((got > 0U) && (speaker_write(spk, s_cpl_out, got, &written, WRITE_TIMEOUT_MS) != SPEAKER_OK)) {
            ret = INMP441_ERR_BUS;
        }
    }
    if (ret == INMP441_OK) {
        ok = cpl_phase(spk, mic, freqs[6], 0, &res[k]);
        k++;
    }
    for (fi = 0U; ok && (ret == INMP441_OK) && (fi < CPL_FREQS); fi++) {
        for (ai = 0U; ok && (ai < CPL_AMPS); ai++) {
            ok = cpl_phase(spk, mic, freqs[fi], amps[ai], &res[k]);
            k++;
        }
    }
    (void)inmp441_stop(mic);
    if (spk_running) {
        (void)speaker_stop(spk);
    }
    if ((ret != INMP441_OK) || !ok) {
        printf("[mic] coupling FAILED: mic code %d, phase ok %d\n", ret, (int)ok);
        return RESULT_FAILED;
    }

    printf("[mic] coupling results (amplitudes in 16 bit sample units; ratio is mic tone amplitude divided by output amplitude, in per mille):\n");
    for (k = 0U; k < CPL_PHASES; k++) {
        const uint32_t out = (res[k].out_amp > 0) ? (uint32_t)res[k].out_amp : 0U;

        printf("[mic] coupling f=%u out_amp=%u mic_tone=%u offbin=%u rms_hpf=%u ratio_permille=%u\n",
               (unsigned)res[k].freq, (unsigned)out, (unsigned)res[k].mic_amp, (unsigned)res[k].off_amp,
               (unsigned)res[k].rms_hpf, (unsigned)((out > 0U) ? ((res[k].mic_amp * 1000U) / out) : 0U));
    }
    return RESULT_OK;
}
#endif

#if APP_MIC_LIVE_AEC && APP_MIC_PLAY_HPF
/*
 * The build of this project uses -Og (CONFIG_COMPILER_OPTIMIZATION_DEBUG) at 160 MHz, which made the
 * first version of the filter about 20 to 30 cycles per multiply-add, too slow for 8 ms per chunk. The
 * three inner loops are therefore separate functions built with -O2 and unrolled by four. The
 * filter is also short (8 ms) and its weights are updated on every second chunk only.
 *
 * Live monitor with an echo canceller (APP_MIC_LIVE_AEC, see mic_app.h). The microphone and the
 * speaker run all the time and nothing is restarted. The measured coupling (speaker to microphone) is
 * linear and has a delay, so the echo of the output is estimated with an adaptive FIR filter fed by
 * the samples that were written to the speaker, and subtracted from the microphone signal.
 *
 * Stages: 0 plays calibration noise, 1 waits for the microphone to catch the end of its echo,
 * 2 correlates noise and microphone (a few lags per pass so that the loop never stalls for long),
 * 3 runs the canceller on the live signal. The correlation gives the delay of the echo and the first
 * weights of the filter. Every pass reads 128 samples and writes 128 samples.
 */
#define AEC_CHUNK         LIVE_CHUNK              /* samples per loop pass, 8 ms */
#define AEC_TAPS          128U                    /* filter length, 8 ms, a multiple of 4 */
#define AEC_MARGIN        16U                     /* taps placed before the strongest echo */
#define AEC_RING          4096U                   /* power of two, holds the recent output */
#define AEC_LAG_MAX       2048U                   /* longest delay searched, 128 ms */
#define AEC_CAL_PLAY      2560U                   /* samples of calibration noise, 20 chunks, a multiple of 4 */
#define AEC_CAL_MIC       (AEC_CAL_PLAY + AEC_LAG_MAX)
#define AEC_LAGS_PER_PASS 4U
#define AEC_SETTLE_MS     1500U
#define AEC_BAD_LIMIT     3U                      /* windows of 1 s in a row with a bad residual */
#define AEC_WINDOW_CHUNKS (RATE_HZ / AEC_CHUNK)   /* 1 s */
#define AEC_REG_POWER     50.0f                   /* regularisation, power per sample, rms about 7 */

#if ((AEC_CAL_PLAY % AEC_CHUNK) != 0U) || ((AEC_LAG_MAX % AEC_CHUNK) != 0U) || ((AEC_TAPS % 4U) != 0U)
#error "AEC_CAL_PLAY and AEC_LAG_MAX must be multiples of AEC_CHUNK"
#endif
#if ((AEC_LAG_MAX + AEC_CHUNK) > AEC_RING)
#error "AEC_RING is too small for AEC_LAG_MAX"
#endif

static int16_t s_cal_mic[AEC_CAL_MIC];
static int16_t s_cal_ref[AEC_CAL_PLAY];
static float   s_cal_corr[AEC_LAG_MAX];
static int16_t s_ref_ring[AEC_RING];              /* output after the volume, indexed by sample count */
static float   s_aec_w[AEC_TAPS];
static float   s_aec_xl[AEC_TAPS + AEC_CHUNK];
static float   s_aec_d[AEC_CHUNK];
static int16_t s_aec_out[AEC_CHUNK];

typedef struct aec_state {
    uint32_t lag_peak;     /* delay in samples from an output sample to its echo in the microphone */
    uint32_t lag0;         /* delay of the first tap, at least AEC_CHUNK */
    float    ref_energy;   /* energy of the calibration noise, the scale of the correlation */
    bool     ok;           /* calibration found the echo */
    uint32_t window_pct;   /* percent of the echo energy near the peak that the filter span holds */
} aec_state_t;

#define AEC_HOT __attribute__((optimize("O2")))

/* Dot product of n floats, n is a multiple of 4. */
static AEC_HOT float aec_dot(const float *a, const float *b, uint32_t n)
{
    float s0 = 0.0f;
    float s1 = 0.0f;
    float s2 = 0.0f;
    float s3 = 0.0f;
    uint32_t i;

    for (i = 0U; i < n; i += 4U) {
        s0 += a[i] * b[i];
        s1 += a[i + 1U] * b[i + 1U];
        s2 += a[i + 2U] * b[i + 2U];
        s3 += a[i + 3U] * b[i + 3U];
    }
    return (s0 + s1) + (s2 + s3);
}

/* a[i] += k * b[i] for n floats, n is a multiple of 4. */
static AEC_HOT void aec_axpy(float *a, const float *b, float k, uint32_t n)
{
    uint32_t i;

    for (i = 0U; i < n; i += 4U) {
        a[i] += k * b[i];
        a[i + 1U] += k * b[i + 1U];
        a[i + 2U] += k * b[i + 2U];
        a[i + 3U] += k * b[i + 3U];
    }
}

/* Correlation of two int16 blocks, n is a multiple of 4. */
static AEC_HOT float aec_corr(const int16_t *m, const int16_t *r, uint32_t n)
{
    float s0 = 0.0f;
    float s1 = 0.0f;
    float s2 = 0.0f;
    float s3 = 0.0f;
    uint32_t i;

    for (i = 0U; i < n; i += 4U) {
        s0 += (float)m[i] * (float)r[i];
        s1 += (float)m[i + 1U] * (float)r[i + 1U];
        s2 += (float)m[i + 2U] * (float)r[i + 2U];
        s3 += (float)m[i + 3U] * (float)r[i + 3U];
    }
    return (s0 + s1) + (s2 + s3);
}

static uint32_t aec_rng(uint32_t *s)
{
    uint32_t x = *s;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static int16_t aec_clip16(float v)
{
    if (v > 32767.0f) {
        return 32767;
    }
    if (v < -32768.0f) {
        return -32768;
    }
    return (int16_t)v;
}

/* Starts the filter from the correlation of the calibration, which is the echo path for a white noise. */
static void aec_load_weights(const aec_state_t *st)
{
    uint32_t q;

    for (q = 0U; q < AEC_TAPS; q++) {
        s_aec_w[q] = s_cal_corr[st->lag0 + ((AEC_TAPS - 1U) - q)] / st->ref_energy;
    }
}

/*
 * Looks at the finished correlation. Sets lag_peak, lag0 and ok. The echo is accepted when its peak
 * is at least 8 times the mean absolute value over all lags. Returns that ratio (rounded down).
 */
static uint32_t aec_find_echo(aec_state_t *st)
{
    float peak = 0.0f;
    float sum = 0.0f;
    uint32_t best = 0U;
    uint32_t lag;

    st->ok = false;
    for (lag = 0U; lag < AEC_LAG_MAX; lag++) {
        const float a = (s_cal_corr[lag] < 0.0f) ? -s_cal_corr[lag] : s_cal_corr[lag];

        sum += a;
        if ((lag >= AEC_CHUNK) && (a > peak)) {
            peak = a;
            best = lag;
        }
    }
    st->lag_peak = best;
    st->lag0 = (best > (AEC_CHUNK + AEC_MARGIN)) ? (best - AEC_MARGIN) : AEC_CHUNK;
    if ((st->lag0 + AEC_TAPS) > AEC_LAG_MAX) {
        st->lag0 = AEC_LAG_MAX - AEC_TAPS;
    }
    if ((st->ref_energy > 1.0f) && (peak > (8.0f * (sum / (float)AEC_LAG_MAX)))) {
        float in_win = 0.0f;
        float near_peak = 0.0f;
        const uint32_t lo = (best > 64U) ? (best - 64U) : 0U;
        const uint32_t hi = ((best + 512U) < AEC_LAG_MAX) ? (best + 512U) : AEC_LAG_MAX;

        st->ok = true;
        aec_load_weights(st);
        for (lag = lo; lag < hi; lag++) {
            const float e = s_cal_corr[lag] * s_cal_corr[lag];

            near_peak += e;
            if ((lag >= st->lag0) && (lag < (st->lag0 + AEC_TAPS))) {
                in_win += e;
            }
        }
        st->window_pct = (near_peak > 0.0f) ? (uint32_t)((100.0f * in_win) / near_peak) : 0U;
    }
    return (sum > 0.0f) ? (uint32_t)((peak * (float)AEC_LAG_MAX) / sum) : 0U;
}

/*
 * One block of the NLMS canceller over s_aec_d (AEC_CHUNK samples of the microphone after the
 * high-pass), using the output history before sample n0. The block is filtered with fixed weights,
 * s_aec_d becomes the residual, then the weights are updated once from the whole block. The update is
 * skipped when allow_adapt is false or when the microphone chunk is much louder than the loudest
 * reference sample in the filter span (a near-end voice, which would pull the weights away from
 * the echo path). Returns true when the update was skipped because of a near-end voice.
 */
static bool aec_nlms_block(const aec_state_t *st, uint32_t n0, float mu, bool allow_adapt)
{
    const int32_t base = (int32_t)n0 - (int32_t)st->lag0 - (int32_t)(AEC_TAPS - 1U);
    float etot = 0.0f;
    float maxx = 0.0f;
    float maxd = 0.0f;
    bool near_end;
    uint32_t m;
    uint32_t i;

    for (m = 0U; m < (AEC_TAPS + AEC_CHUNK); m++) {
        const int32_t u = base + (int32_t)m;
        float v = 0.0f;

        if ((u >= 0) && (u < (int32_t)n0)) {
            v = (float)s_ref_ring[(uint32_t)u & (AEC_RING - 1U)];
        }
        s_aec_xl[m] = v;
        etot += v * v;
        if (((v < 0.0f) ? -v : v) > maxx) {
            maxx = (v < 0.0f) ? -v : v;
        }
    }
    for (i = 0U; i < AEC_CHUNK; i++) {
        const float a = (s_aec_d[i] < 0.0f) ? -s_aec_d[i] : s_aec_d[i];

        if (a > maxd) {
            maxd = a;
        }
    }
    near_end = (maxd > ((0.8f * maxx) + 200.0f));

    for (i = 0U; i < AEC_CHUNK; i++) {
        s_aec_d[i] -= aec_dot(s_aec_w, &s_aec_xl[i], AEC_TAPS);
    }
    if (allow_adapt && !near_end) {
        /* Energy of one filter window, estimated from the whole span. */
        const float e1 = etot * ((float)AEC_TAPS / (float)(AEC_TAPS + AEC_CHUNK));
        const float k = mu / (((float)AEC_CHUNK * e1) + ((float)AEC_CHUNK * (float)AEC_TAPS * AEC_REG_POWER));

        for (i = 0U; i < AEC_CHUNK; i++) {
            aec_axpy(s_aec_w, &s_aec_xl[i], k * s_aec_d[i], AEC_TAPS);
        }
    }
    return near_end;
}

/* Runs until an error, so it returns only RESULT_FAILED or RESULT_SKIPPED. */
static test_result_t run_live_aec(speaker_t *spk, inmp441_t *mic)
{
    const float mu = (float)APP_MIC_AEC_MU_PERCENT / 100.0f;
    const int32_t vol = (int32_t)APP_MIC_LIVE_VOLUME;
    aec_state_t st;
    uint32_t rng = 2463534242U;
    uint32_t n = 0U;
    uint32_t stage = 0U;
    uint32_t lag_done = 0U;
    uint32_t bad = 0U;
    uint32_t howl = 0U;
    uint32_t mute_left = 0U;
    uint32_t win_chunks = 0U;
    uint32_t win_frozen = 0U;
    uint32_t win_count = 0U;
    uint32_t win_slow = 0U;
    uint32_t max_ms = 0U;
    float g_cur = (float)APP_MIC_AEC_GAIN;
    uint64_t sum_d = 0U;
    uint64_t sum_e = 0U;
    uint64_t sum_o = 0U;
    bool first = true;
    int ret;
    int sret;
    uint32_t i;

    if ((spk == NULL) || (mic == NULL)) {
        printf("[mic] live aec skipped: the speaker or the microphone is not available\n");
        return RESULT_SKIPPED;
    }
    memset(&st, 0, sizeof(st));
    memset(s_ref_ring, 0, sizeof(s_ref_ring));
    memset(s_aec_w, 0, sizeof(s_aec_w));
    printf("[mic] live aec: calibration noise for about 5 s, then live. Stay quiet, speaker at volume %u percent, "
           "gain x%d\n", (unsigned)APP_MIC_LIVE_VOLUME, (int)APP_MIC_AEC_GAIN);

    sret = speaker_set_volume(spk, (uint8_t)APP_MIC_LIVE_VOLUME);
    if (sret == SPEAKER_OK) {
        sret = speaker_start(spk);
    }
    if (sret != SPEAKER_OK) {
        printf("[mic] live aec: speaker start failed: %d\n", sret);
        return RESULT_FAILED;
    }
    ret = inmp441_start(mic);
    if (ret == INMP441_OK) {
        uint32_t left = (RATE_HZ / 1000U) * AEC_SETTLE_MS;

        while ((ret == INMP441_OK) && (left > 0U)) {
            size_t got = 0U;

            ret = inmp441_read(mic, s_chunk, AEC_CHUNK, &got, READ_TIMEOUT_MS);
            left = (left > (uint32_t)got) ? (left - (uint32_t)got) : 0U;
        }
    }
    if ((ret == INMP441_OK) && (write_silence(spk, APP_MIC_LIVE_PREROLL_MS) != SPEAKER_OK)) {
        ret = INMP441_ERR_BUS;
    }

    while (ret == INMP441_OK) {
        size_t got = 0U;
        size_t written = 0U;
        uint32_t t0 = 0U;
        uint32_t t1 = 0U;
        const bool use_aec = (stage == 3U) && st.ok;

        ret = inmp441_read(mic, s_chunk, AEC_CHUNK, &got, READ_TIMEOUT_MS);
        if ((ret != INMP441_OK) || (got != AEC_CHUNK)) {
            ret = (ret != INMP441_OK) ? ret : INMP441_ERR_BUS;
            break;
        }
        sx_get_time_ms(&t0);
        if (first) {
            play_hpf_reset(s_chunk[0]);
            first = false;
        }
        for (i = 0U; i < AEC_CHUNK; i++) {
            s_aec_d[i] = (float)play_hpf_step(s_chunk[i]);
        }

        if (stage <= 1U) {
            for (i = 0U; i < AEC_CHUNK; i++) {
                if ((n + i) < AEC_CAL_MIC) {
                    s_cal_mic[n + i] = aec_clip16(s_aec_d[i]);
                }
                s_aec_out[i] = (stage == 0U)
                    ? (int16_t)(((int32_t)(aec_rng(&rng) % ((2U * (uint32_t)APP_MIC_AEC_CAL_AMP) + 1U)))
                                - (int32_t)APP_MIC_AEC_CAL_AMP)
                    : (int16_t)0;
                if ((n + i) < AEC_CAL_PLAY) {
                    s_cal_ref[n + i] = (int16_t)(((int32_t)s_aec_out[i] * vol) / (int32_t)SPEAKER_VOLUME_MAX);
                }
            }
        } else if (stage == 2U) {
            uint32_t lag;
            const uint32_t end = ((lag_done + AEC_LAGS_PER_PASS) < AEC_LAG_MAX)
                                     ? (lag_done + AEC_LAGS_PER_PASS) : AEC_LAG_MAX;

            for (lag = lag_done; lag < end; lag++) {
                s_cal_corr[lag] = aec_corr(&s_cal_mic[lag], s_cal_ref, AEC_CAL_PLAY);
            }
            lag_done = end;
            memset(s_aec_out, 0, sizeof(s_aec_out));
        } else {
            const bool muted = (mute_left > 0U);
            uint64_t chunk_o = 0U;

            for (i = 0U; i < AEC_CHUNK; i++) {
                sum_d += (uint64_t)(s_aec_d[i] * s_aec_d[i]);
            }
            if (use_aec) {
                if (aec_nlms_block(&st, n, mu, (!muted) && ((win_chunks & 1U) != 0U))) {
                    win_frozen++;
                }
            }
            {
                /* Gain limiter: a loud voice is not amplified into clipping. The gain falls at once and
                   rises slowly (about 0.4 s). */
                const float g_set = use_aec ? (float)APP_MIC_AEC_GAIN : (float)APP_MIC_LIVE_GAIN;
                float pk = 0.0f;
                float g_want;

                for (i = 0U; i < AEC_CHUNK; i++) {
                    const float a = (s_aec_d[i] < 0.0f) ? -s_aec_d[i] : s_aec_d[i];

                    if (a > pk) {
                        pk = a;
                    }
                }
                g_want = ((pk * g_set) > (float)APP_MIC_AEC_TARGET) ? ((float)APP_MIC_AEC_TARGET / pk) : g_set;
                g_cur = (g_want < g_cur) ? g_want : (g_cur + ((g_want - g_cur) * 0.02f));
            }
            for (i = 0U; i < AEC_CHUNK; i++) {
                s_aec_out[i] = muted ? (int16_t)0 : aec_clip16(s_aec_d[i] * g_cur);
                sum_e += (uint64_t)(s_aec_d[i] * s_aec_d[i]);
                chunk_o += (uint64_t)((int64_t)s_aec_out[i] * (int64_t)s_aec_out[i]);
            }
            sum_o += chunk_o;
            if (muted) {
                mute_left--;
            } else if (isqrt64(chunk_o / AEC_CHUNK) >= (uint32_t)APP_MIC_AEC_HOWL_RMS) {
                howl++;
                if (howl >= 100U) {          /* about 0.8 s in a row, speech does not hold this level */
                    mute_left = (1500U * (RATE_HZ / 1000U)) / AEC_CHUNK;
                    howl = 0U;
                    if (st.ok) {
                        aec_load_weights(&st);
                    }
                }
            } else {
                howl = 0U;
            }
        }

        for (i = 0U; i < AEC_CHUNK; i++) {
            s_ref_ring[(n + i) & (AEC_RING - 1U)] =
                (int16_t)(((int32_t)s_aec_out[i] * vol) / (int32_t)SPEAKER_VOLUME_MAX);
        }
        sx_get_time_ms(&t1);
        if ((t1 - t0) > max_ms) {
            max_ms = t1 - t0;
        }
        if ((t1 - t0) > 6U) {
            win_slow++;
        }

        sret = speaker_write(spk, s_aec_out, AEC_CHUNK, &written, WRITE_TIMEOUT_MS);
        if (sret != SPEAKER_OK) {
            printf("[mic] live aec: speaker_write failed: %d\n", sret);
            ret = INMP441_ERR_BUS;
            break;
        }
        n += AEC_CHUNK;

        if ((stage == 0U) && (n >= AEC_CAL_PLAY)) {
            stage = 1U;
        } else if ((stage == 1U) && (n >= AEC_CAL_MIC)) {
            uint64_t en = 0U;
            uint32_t k;

            for (k = 0U; k < AEC_CAL_PLAY; k++) {
                en += (uint64_t)((int64_t)s_cal_ref[k] * (int64_t)s_cal_ref[k]);
            }
            st.ref_energy = (float)en;
            stage = 2U;
        } else if ((stage == 2U) && (lag_done >= AEC_LAG_MAX)) {
            const uint32_t ratio = aec_find_echo(&st);

            if (st.ok) {
                printf("[mic] live aec: echo at %u samples (%u ms), peak is %ux the mean, taps from %u, "
                       "the filter span holds %u percent of the echo energy near the peak. "
                       "Longest calibration pass %u ms of 8. Live now\n",
                       (unsigned)st.lag_peak, (unsigned)(st.lag_peak / (RATE_HZ / 1000U)), (unsigned)ratio,
                       (unsigned)st.lag0, (unsigned)st.window_pct, (unsigned)max_ms);
            } else {
                printf("[mic] live aec: no echo found (peak %ux the mean), is the speaker connected? "
                       "Live runs without the canceller at gain x%d\n", (unsigned)ratio, (int)APP_MIC_LIVE_GAIN);
            }
            stage = 3U;
            max_ms = 0U;
        } else if (stage == 3U) {
            win_chunks++;
            if (win_chunks >= AEC_WINDOW_CHUNKS) {
                const uint32_t span = win_chunks * AEC_CHUNK;

                if (use_aec) {
                    bad = (sum_e > ((4U * sum_d) + (100000ULL * span))) ? (bad + 1U) : 0U;
                }
                win_count++;
#if APP_MIC_AEC_LOG_S > 0
                if (win_count >= (uint32_t)APP_MIC_AEC_LOG_S) {
                    printf("[mic] aec: mic %u residual %u out %u (rms), gain x%u, frozen %u of %u blocks, "
                           "longest pass %u ms of 8, slow passes %u\n",
                           (unsigned)isqrt64(sum_d / span), (unsigned)isqrt64(sum_e / span),
                           (unsigned)isqrt64(sum_o / span), (unsigned)g_cur, (unsigned)win_frozen,
                           (unsigned)win_chunks, (unsigned)max_ms, (unsigned)win_slow);
                    win_count = 0U;
                }
#else
                win_count = 0U;
#endif
                win_chunks = 0U;
                win_frozen = 0U;
                win_slow = 0U;
                sum_d = 0U;
                sum_e = 0U;
                sum_o = 0U;
                max_ms = 0U;
                if ((bad >= AEC_BAD_LIMIT) && st.ok) {
                    aec_load_weights(&st);
                    bad = 0U;
                }
            }
        }
    }
    (void)inmp441_stop(mic);
    (void)speaker_stop(spk);
    printf("[mic] live aec ended: mic code %d\n", ret);
    return RESULT_FAILED;
}
#endif

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
#if APP_MIC_DIAG && APP_MIC_PLAY_HPF
    if ((r_meter == RESULT_OK) && speaker_free) {
        printf("[mic] diagnostic %s\n", result_text(run_diag(board_get_speaker(), board_get_mic())));
    }
#endif
#if APP_MIC_COUPLING && APP_MIC_PLAY_HPF
    if ((r_meter == RESULT_OK) && speaker_free) {
        printf("[mic] coupling probe %s\n", result_text(run_coupling(board_get_speaker(), board_get_mic())));
    }
#endif
    if ((r_meter == RESULT_OK) && (APP_MIC_LIVE_MS != 0U)) {
#if APP_MIC_LIVE_AEC && APP_MIC_PLAY_HPF
        r_live = speaker_free ? run_live_aec(board_get_speaker(), board_get_mic()) : RESULT_SKIPPED;
#elif APP_MIC_LIVE_SEGMENT
        r_live = speaker_free ? run_live_segments(board_get_speaker(), board_get_mic()) : RESULT_SKIPPED;
#else
        r_live = run_live_monitor(board_get_speaker(), board_get_mic(), speaker_free);
#endif
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