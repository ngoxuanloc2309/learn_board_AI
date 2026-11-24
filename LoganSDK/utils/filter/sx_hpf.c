#include "sx_hpf.h"

#define HPF_R_Q15    31480   /* 0.9607 in Q15: 1 - 2 pi 100 / 16000 = 0.96073 */
#define HPF_SHIFT    15

void sx_hpf_init(sx_hpf_t *f)
{
    if (f == NULL) {
        return;
    }
    f->prev_in = 0;
    f->prev_out = 0;
}

void sx_hpf_reset(sx_hpf_t *f, int16_t first_sample)
{
    if (f == NULL) {
        return;
    }
    f->prev_in = (int32_t)first_sample;
    f->prev_out = 0;
}

void sx_hpf_process(sx_hpf_t *f, const int16_t *in, int16_t *out, size_t n)
{
    size_t i;

    if ((f == NULL) || (in == NULL) || (out == NULL)) {
        return;
    }
    for (i = 0U; i < n; i++) {
        int32_t x = (int32_t)in[i];
        int32_t y = (x - f->prev_in) + (int32_t)(((int64_t)HPF_R_Q15 * (int64_t)f->prev_out) >> HPF_SHIFT);

        f->prev_in = x;
        /* The state keeps the unclipped value, so a clipped peak does not make a step in the filter.
         * The state is bounded: |y| <= |x - x_prev| / (1 - R), far below 2^31 for 16 bit input. */
        f->prev_out = y;
        if (y > 32767) {
            out[i] = 32767;
        } else if (y < -32768) {
            out[i] = -32768;
        } else {
            out[i] = (int16_t)y;
        }
    }
}