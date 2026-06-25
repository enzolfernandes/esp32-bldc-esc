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

/** Condução de uma perna da ponte para comutação BLDC 6-step. */
typedef enum {
    HAL_PWM_COND_OFF = 0,     /**< Ambos os drivers em nível baixo (repouso). */
    HAL_PWM_COND_SOURCE,      /**< High-side em PWM (duty em %). */
    HAL_PWM_COND_SINK         /**< Low-side condutando (complementar, duty efetivo 0 %). */
} hal_pwm_conduction_t;

/** Mantém AH/AL/BH/BL/CH/CL em GPIO OUTPUT LOW (sem mux MCPWM). Chamar no início do boot. */
bool hal_pwm_hold_pins_low(void);

bool hal_pwm_init(void);
void hal_pwm_set_armed(bool armed);
bool hal_pwm_is_armed(void);
void hal_pwm_set_phase_duty(hal_pwm_phase_t phase, float duty_percent);
void hal_pwm_set_phase_conduction(hal_pwm_phase_t phase, hal_pwm_conduction_t mode,
                                  float duty_percent);
void hal_pwm_disable_all(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_PWM_H
