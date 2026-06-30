#ifndef HAL_MOTOR_H
#define HAL_MOTOR_H

#include "hal_pwm.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Boot seguro: SD=HIGH (shutdown), PWM pins LOW, MCPWM init sem attach. */
bool hal_motor_init(void);
/** Reconfigura SD após stack BT — todos SD=HIGH. */
bool hal_motor_reclaim_outputs(void);
/** Attach MCPWM aos GPIOs (SD gerido por apply_step). */
bool hal_motor_arm(void);
/** SD=HIGH, PWM off, detach MCPWM. */
void hal_motor_disarm(void);
bool hal_motor_is_armed(void);

/** Aplica modo 6-step numa fase (SD + MCPWM). */
void hal_motor_apply_conduction(hal_pwm_phase_t phase, hal_pwm_conduction_t mode,
                                float duty_percent);
/** Passo 6-step atómico (três fases). */
void hal_motor_apply_step(hal_pwm_conduction_t mode_a, hal_pwm_conduction_t mode_b,
                          hal_pwm_conduction_t mode_c, float duty_percent);
/** Todas as fases OFF + SD=HIGH (mantém MCPWM attach se armado). */
void hal_motor_halt_outputs(void);
/** ISR OCP: SD=HIGH imediato (IRAM). */
void hal_motor_emergency_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_MOTOR_H
