#ifndef SX_HPF_H
#define SX_HPF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/*
 * First-order high-pass filter for 16 kHz, 16 bit audio, cut-off near 100 Hz (integer arithmetic).
 * It removes the DC offset and the very low rumble of the microphone from the stream sent to the server
 * (the voice activity detector has its own filter and only measures). No heap, no globals.
 *
 *   y[n] = x[n] - x[n-1] + (R * y[n-1]) with R = 0.9607 (Q15: 31480), so fc = (1 - R) * fs / (2 pi) = about 100 Hz.
 *
 * Not thread safe: one filter object belongs to one task.
 */
typedef struct {
    int32_t prev_in;    /* x[n-1] */
    int32_t prev_out;   /* y[n-1] before clipping */
} sx_hpf_t;

/* Sets the state to zero (the next sample is treated as a step from 0). Does nothing when f is NULL. */
void sx_hpf_init(sx_hpf_t *f);

/*
 * Sets the state as if the input had always been first_sample, so a constant DC gives 0 from the first sample.
 * Call it at the start of a turn. Does nothing when f is NULL.
 */
void sx_hpf_reset(sx_hpf_t *f, int16_t first_sample);

/*
 * Filters n samples from in to out. in and out may be the same buffer. The output is clipped to 16 bits.
 * Does nothing when a pointer is NULL.
 */
void sx_hpf_process(sx_hpf_t *f, const int16_t *in, int16_t *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* SX_HPF_H */