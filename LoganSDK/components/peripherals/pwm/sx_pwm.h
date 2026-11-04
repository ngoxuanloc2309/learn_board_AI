#ifndef SX_PWM_H
#define SX_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SX_PWM_OK           0
#define SX_PWM_ERR_PARAM   (-1)
#define SX_PWM_ERR_STATE   (-2)
#define SX_PWM_ERR_HW      (-3)

typedef struct {
    int channel;
    int timer;
    int output_pin;
    uint32_t frequency_hz;
    uint8_t resolution_bits;
    uint32_t duty;
} sx_pwm_config_t;

typedef struct {
    const sx_pwm_config_t *config;
    uint32_t max_duty;
    uint8_t ready;
} sx_pwm_t;

int sx_pwm_init(sx_pwm_t *pwm, const sx_pwm_config_t *config);
int sx_pwm_set_duty(sx_pwm_t *pwm, uint32_t duty);
int sx_pwm_set_frequency(sx_pwm_t *pwm, uint32_t frequency_hz);
int sx_pwm_deinit(sx_pwm_t *pwm);

#ifdef __cplusplus
}
#endif

#endif
