#include <stdbool.h>
#include <stdint.h>

#include "driver/ledc.h"
#include "esp_err.h"
#include "sx_pwm.h"

static bool valid_config(const sx_pwm_config_t *config)
{
    return config != NULL && config->channel >= 0 && config->channel < LEDC_CHANNEL_MAX &&
           config->timer >= 0 && config->timer < LEDC_TIMER_MAX &&
           config->output_pin >= 0 && config->frequency_hz > 0U &&
           config->resolution_bits > 0U && config->resolution_bits <= LEDC_TIMER_BIT_MAX;
}

int sx_pwm_init(sx_pwm_t *pwm, const sx_pwm_config_t *config)
{
    if (pwm == NULL || !valid_config(config) || pwm->ready) {
        return pwm != NULL && pwm->ready ? SX_PWM_ERR_STATE : SX_PWM_ERR_PARAM;
    }

    uint32_t max_duty = (1UL << config->resolution_bits) - 1UL;
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)config->resolution_bits,
        .timer_num = (ledc_timer_t)config->timer,
        .freq_hz = config->frequency_hz,
        .clk_cfg = LEDC_AUTO_CLK
    };
    if (ledc_timer_config(&timer) != ESP_OK) {
        return SX_PWM_ERR_HW;
    }

    ledc_channel_config_t channel = {
        .gpio_num = config->output_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)config->channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)config->timer,
        .duty = config->duty > max_duty ? max_duty : config->duty,
        .hpoint = 0
    };
    if (ledc_channel_config(&channel) != ESP_OK) {
        return SX_PWM_ERR_HW;
    }
    pwm->config = config;
    pwm->max_duty = max_duty;
    pwm->ready = 1U;
    return SX_PWM_OK;
}

int sx_pwm_set_duty(sx_pwm_t *pwm, uint32_t duty)
{
    if (pwm == NULL || !pwm->ready || duty > pwm->max_duty) {
        return SX_PWM_ERR_PARAM;
    }
    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwm->config->channel, duty) != ESP_OK ||
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwm->config->channel) != ESP_OK) {
        return SX_PWM_ERR_HW;
    }
    return SX_PWM_OK;
}

int sx_pwm_set_frequency(sx_pwm_t *pwm, uint32_t frequency_hz)
{
    if (pwm == NULL || !pwm->ready || frequency_hz == 0U) {
        return SX_PWM_ERR_PARAM;
    }
    if (ledc_set_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)pwm->config->timer, frequency_hz) == 0U) {
        return SX_PWM_ERR_HW;
    }
    return SX_PWM_OK;
}

int sx_pwm_deinit(sx_pwm_t *pwm)
{
    if (pwm == NULL) {
        return SX_PWM_ERR_PARAM;
    }
    if (!pwm->ready) {
        return SX_PWM_OK;
    }
    if (ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwm->config->channel, 0) != ESP_OK) {
        return SX_PWM_ERR_HW;
    }
    pwm->config = NULL;
    pwm->ready = 0U;
    return SX_PWM_OK;
}
