#ifndef HAL_PWM_H
#define HAL_PWM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_PWM_PHASE_A = 0,
    HAL_PWM_PHASE_B,
    HAL_PWM_PHASE_C,
    HAL_PWM_PHASE_COUNT
} hal_pwm_phase_t;

bool hal_pwm_init(void);
void hal_pwm_set_armed(bool armed);
bool hal_pwm_is_armed(void);
void hal_pwm_set_phase_duty(hal_pwm_phase_t phase, float duty_percent);
void hal_pwm_disable_all(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_PWM_H
