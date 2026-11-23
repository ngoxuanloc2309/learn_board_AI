#ifndef SX_VAD_H
#define SX_VAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Energy based voice activity detector for one spoken turn (docs/Backend_Protocol.md section 7).
 * It is the safety net that ends a turn when the user forgets to touch STOP. It does not replace the
 * stop button. Pure C, no ESP-IDF header, no heap, no time source: time is counted in frames, so every
 * duration is exact only when no frame is lost between the microphone and this module.
 *
 * Input: signed 16 bit mono PCM in frames of exactly frame_ms milliseconds (for example 320 samples
 * at 16 kHz and 20 ms). The module removes DC and the lowest frequencies itself with a one pole
 * filter (cut off near 100 Hz at 16 kHz, it scales with the sample rate), then measures the RMS of
 * each frame. This filter is only for the measurement, the caller keeps its own copy of the audio.
 *
 * Noise floor: tracked while no turn runs (sx_vad_feed_idle) and on quiet frames during a turn.
 * It falls fast and rises slowly, and one frame can raise it only a little, so a short loud sound
 * does not lift the speech threshold. Threshold = max(min_threshold_rms, noise * ratio).
 *
 * A turn: sx_vad_start_turn, then one sx_vad_process per frame. The first terminal event ends the
 * turn, after which sx_vad_process returns SX_VAD_ERR_STATE until the next sx_vad_start_turn.
 * All default numbers are DESIGN values chosen by the app, none is measured on the board.
 *
 * Not thread safe. One task must call everything on one sx_vad_t.
 */

#define SX_VAD_OK              0
#define SX_VAD_ERR_PARAM      (-1)   /* NULL pointer, bad configuration, or wrong frame size */
#define SX_VAD_ERR_STATE      (-2)   /* not initialised, or no turn is running */

/* Largest frame the module accepts, in samples. */
#define SX_VAD_MAX_FRAME_SAMPLES  2048U

typedef enum sx_vad_event {
    SX_VAD_EVENT_NONE = 0,
    SX_VAD_EVENT_SPEECH_START,      /* speech seen for onset_ms in a row. The turn goes on */
    SX_VAD_EVENT_END_SILENCE,       /* speech seen, then end_silence_ms of quiet. Turn ended (reason "vad") */
    SX_VAD_EVENT_NO_SPEECH_TIMEOUT, /* no speech within wait_speech_ms. Turn ended (reason "timeout") */
    SX_VAD_EVENT_MAX_LENGTH         /* the turn reached max_turn_ms. Turn ended (reason "max_length") */
} sx_vad_event_t;

typedef struct sx_vad_config {
    uint32_t sample_rate_hz;        /* a multiple of 1000, for example 16000 */
    uint32_t frame_ms;              /* frame length, for example 20 */
    uint32_t onset_ms;              /* speech must stay above the threshold this long to count as started */
    uint32_t end_silence_ms;        /* quiet time after speech that ends the turn */
    uint32_t wait_speech_ms;        /* turn is cancelled when no speech started within this time */
    uint32_t max_turn_ms;           /* hard limit of one turn, larger than wait_speech_ms */
    uint32_t initial_noise_rms;     /* noise floor used until the first frames were measured */
    uint32_t min_threshold_rms;     /* the speech threshold never goes below this RMS value */
    uint32_t threshold_ratio_x10;   /* threshold = noise * ratio / 10, for example 30 means 3.0 times */
} sx_vad_config_t;

typedef struct sx_vad_stats {
    uint32_t rms;                   /* RMS of the last frame after the filter */
    uint32_t noise_rms;             /* current noise floor */
    uint32_t threshold_rms;         /* current speech threshold */
    uint32_t peak_rms;              /* highest frame RMS since sx_vad_start_turn */
    uint32_t turn_frames;           /* frames processed in the running or last turn */
    bool     in_turn;
    bool     speech_started;        /* SX_VAD_EVENT_SPEECH_START was reported in this turn */
} sx_vad_stats_t;

typedef struct sx_vad {
    sx_vad_config_t cfg;            /* private from here on */
    uint32_t frame_samples;
    uint32_t onset_frames;
    uint32_t end_frames;
    uint32_t wait_frames;
    uint32_t max_frames;
    int32_t  hp_prev_in;
    int32_t  hp_prev_out;
    int32_t  hp_coef;               /* Q15 */
    uint32_t noise_q8;              /* noise floor RMS in Q8 */
    uint32_t last_rms;
    uint32_t peak_rms;
    uint32_t turn_frames;
    uint32_t above_run;             /* consecutive frames above the threshold */
    uint32_t quiet_run;             /* consecutive frames at or below the threshold after speech started */
    bool     ready;
    bool     in_turn;
    bool     speech_started;
} sx_vad_t;

/*
 * Checks the configuration and prepares the detector. frame_ms and the durations must be non zero,
 * sample_rate_hz a multiple of 1000, the frame at most SX_VAD_MAX_FRAME_SAMPLES samples, max_turn_ms
 * larger than wait_speech_ms, threshold_ratio_x10 at least 10. Durations are rounded up to whole frames.
 * The sx_vad_t may hold anything before the call.
 */
int sx_vad_init(sx_vad_t *vad, const sx_vad_config_t *config);

/* Measures one frame while no turn runs: keeps the filter and the noise floor current. Needs exactly one frame. */
int sx_vad_feed_idle(sx_vad_t *vad, const int16_t *samples, size_t count);

/* Starts a turn. The noise floor and the filter state are kept, the turn counters are cleared. */
int sx_vad_start_turn(sx_vad_t *vad);

/* Ends a running turn without an event (for example the user touched STOP). Does nothing when no turn runs. */
void sx_vad_cancel_turn(sx_vad_t *vad);

/* Processes one frame of a running turn. *event is the result. Needs exactly one frame. */
int sx_vad_process(sx_vad_t *vad, const int16_t *samples, size_t count, sx_vad_event_t *event);

/* Copies the current numbers. Safe to call at any time after init. */
int sx_vad_get_stats(const sx_vad_t *vad, sx_vad_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* SX_VAD_H */