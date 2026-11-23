#include "sx_vad.h"

#include <string.h>

/* One pole DC blocker y = x - x_prev + R * y_prev. R = 1 - 2 * pi * fc / fs with fc near 100 Hz at 16 kHz. */
#define HP_COEF_16K_Q15     31457          /* 0.96 */

/* Noise floor tracking (Q8 values). Rises slowly, falls fast, one frame can only lift it a little. */
#define NOISE_RISE_SHIFT    7U
#define NOISE_FALL_SHIFT    4U
#define NOISE_CLAMP_MUL     2U
#define NOISE_CLAMP_ADD     10U

static uint32_t ms_to_frames(uint32_t ms, uint32_t frame_ms)
{
    return (ms + frame_ms - 1U) / frame_ms;
}

static uint32_t isqrt_u64(uint64_t v)
{
    uint64_t res = 0U;
    uint64_t bit = (uint64_t)1U << 62;

    while (bit > v) {
        bit >>= 2;
    }
    while (bit != 0U) {
        if (v >= (res + bit)) {
            v -= (res + bit);
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)res;
}

/* Filters one frame and returns its RMS. */
static uint32_t measure_frame(sx_vad_t *vad, const int16_t *samples)
{
    uint64_t sum = 0U;
    uint32_t i;

    for (i = 0U; i < vad->frame_samples; i++) {
        int32_t x = (int32_t)samples[i];
        int32_t y = x - vad->hp_prev_in + (int32_t)(((int64_t)vad->hp_coef * vad->hp_prev_out) >> 15);

        vad->hp_prev_in = x;
        vad->hp_prev_out = y;
        sum += (uint64_t)((int64_t)y * (int64_t)y);
    }
    return isqrt_u64(sum / vad->frame_samples);
}

static uint32_t noise_rms(const sx_vad_t *vad)
{
    return (vad->noise_q8 + 128U) >> 8;
}

static uint32_t threshold_rms(const sx_vad_t *vad)
{
    uint32_t thr = (noise_rms(vad) * vad->cfg.threshold_ratio_x10) / 10U;

    return (thr > vad->cfg.min_threshold_rms) ? thr : vad->cfg.min_threshold_rms;
}

static void update_noise(sx_vad_t *vad, uint32_t rms)
{
    uint32_t cap = (noise_rms(vad) * NOISE_CLAMP_MUL) + NOISE_CLAMP_ADD;
    uint32_t in_q8 = ((rms < cap) ? rms : cap) << 8;

    if (in_q8 > vad->noise_q8) {
        vad->noise_q8 += (in_q8 - vad->noise_q8) >> NOISE_RISE_SHIFT;
    } else {
        vad->noise_q8 -= (vad->noise_q8 - in_q8) >> NOISE_FALL_SHIFT;
    }
}

int sx_vad_init(sx_vad_t *vad, const sx_vad_config_t *config)
{
    uint32_t frame_samples;

    if ((vad == NULL) || (config == NULL)) {
        return SX_VAD_ERR_PARAM;
    }
    if ((config->sample_rate_hz == 0U) || ((config->sample_rate_hz % 1000U) != 0U) ||
        (config->frame_ms == 0U) || (config->onset_ms == 0U) || (config->end_silence_ms == 0U) ||
        (config->wait_speech_ms == 0U) || (config->max_turn_ms <= config->wait_speech_ms) ||
        (config->threshold_ratio_x10 < 10U)) {
        return SX_VAD_ERR_PARAM;
    }
    frame_samples = (config->sample_rate_hz / 1000U) * config->frame_ms;
    if ((frame_samples == 0U) || (frame_samples > SX_VAD_MAX_FRAME_SAMPLES)) {
        return SX_VAD_ERR_PARAM;
    }

    memset(vad, 0, sizeof(*vad));
    vad->cfg = *config;
    vad->frame_samples = frame_samples;
    vad->onset_frames = ms_to_frames(config->onset_ms, config->frame_ms);
    vad->end_frames = ms_to_frames(config->end_silence_ms, config->frame_ms);
    vad->wait_frames = ms_to_frames(config->wait_speech_ms, config->frame_ms);
    vad->max_frames = ms_to_frames(config->max_turn_ms, config->frame_ms);
    /* The coefficient is tuned for 16 kHz. Other rates scale the cut off frequency, see the header. */
    vad->hp_coef = HP_COEF_16K_Q15;
    vad->noise_q8 = config->initial_noise_rms << 8;
    vad->ready = true;
    return SX_VAD_OK;
}

int sx_vad_feed_idle(sx_vad_t *vad, const int16_t *samples, size_t count)
{
    if ((vad == NULL) || (samples == NULL)) {
        return SX_VAD_ERR_PARAM;
    }
    if (!vad->ready) {
        return SX_VAD_ERR_STATE;
    }
    if (count != vad->frame_samples) {
        return SX_VAD_ERR_PARAM;
    }
    vad->last_rms = measure_frame(vad, samples);
    update_noise(vad, vad->last_rms);
    return SX_VAD_OK;
}

int sx_vad_start_turn(sx_vad_t *vad)
{
    if (vad == NULL) {
        return SX_VAD_ERR_PARAM;
    }
    if (!vad->ready) {
        return SX_VAD_ERR_STATE;
    }
    vad->in_turn = true;
    vad->speech_started = false;
    vad->turn_frames = 0U;
    vad->above_run = 0U;
    vad->quiet_run = 0U;
    vad->peak_rms = 0U;
    return SX_VAD_OK;
}

void sx_vad_cancel_turn(sx_vad_t *vad)
{
    if (vad != NULL) {
        vad->in_turn = false;
    }
}

int sx_vad_process(sx_vad_t *vad, const int16_t *samples, size_t count, sx_vad_event_t *event)
{
    uint32_t rms;
    uint32_t thr;
    bool loud;

    if ((vad == NULL) || (samples == NULL) || (event == NULL)) {
        return SX_VAD_ERR_PARAM;
    }
    *event = SX_VAD_EVENT_NONE;
    if (!vad->ready || !vad->in_turn) {
        return SX_VAD_ERR_STATE;
    }
    if (count != vad->frame_samples) {
        return SX_VAD_ERR_PARAM;
    }

    rms = measure_frame(vad, samples);
    vad->last_rms = rms;
    if (rms > vad->peak_rms) {
        vad->peak_rms = rms;
    }
    vad->turn_frames++;

    thr = threshold_rms(vad);
    loud = (rms > thr);
    if (!loud) {
        update_noise(vad, rms);
    }

    if (!vad->speech_started) {
        vad->above_run = loud ? (vad->above_run + 1U) : 0U;
        if (vad->above_run >= vad->onset_frames) {
            vad->speech_started = true;
            vad->quiet_run = 0U;
            *event = SX_VAD_EVENT_SPEECH_START;
        }
    } else {
        vad->quiet_run = loud ? 0U : (vad->quiet_run + 1U);
        if (vad->quiet_run >= vad->end_frames) {
            vad->in_turn = false;
            *event = SX_VAD_EVENT_END_SILENCE;
            return SX_VAD_OK;
        }
    }

    if (vad->turn_frames >= vad->max_frames) {
        vad->in_turn = false;
        *event = SX_VAD_EVENT_MAX_LENGTH;
    } else if ((!vad->speech_started) && (vad->turn_frames >= vad->wait_frames)) {
        vad->in_turn = false;
        *event = SX_VAD_EVENT_NO_SPEECH_TIMEOUT;
    }
    return SX_VAD_OK;
}

int sx_vad_get_stats(const sx_vad_t *vad, sx_vad_stats_t *stats)
{
    if ((vad == NULL) || (stats == NULL)) {
        return SX_VAD_ERR_PARAM;
    }
    if (!vad->ready) {
        return SX_VAD_ERR_STATE;
    }
    stats->rms = vad->last_rms;
    stats->noise_rms = noise_rms(vad);
    stats->threshold_rms = threshold_rms(vad);
    stats->peak_rms = vad->peak_rms;
    stats->turn_frames = vad->turn_frames;
    stats->in_turn = vad->in_turn;
    stats->speech_started = vad->speech_started;
    return SX_VAD_OK;
}