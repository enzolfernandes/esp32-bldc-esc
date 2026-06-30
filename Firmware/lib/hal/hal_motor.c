/*
 * hal_motor.c — Fachada HAL do inversor trifásico (IR2110 + MCPWM 6-step).
 *
 * Orquestra hal_pwm (HIN/LIN) e hal_gpio (SD por fase).
 * SD HIGH = shutdown; SD LOW = fase ativa (SOURCE/SINK).
 */

#include "hal_motor.h"

#include "hal_gpio.h"
#include "hal_pwm.h"

#include "esp_attr.h"

bool hal_motor_init(void)
{
    if (!hal_gpio_init()) {
        return false;
    }

    if (!hal_pwm_hold_pins_low()) {
        return false;
    }

    return hal_pwm_init();
}

bool hal_motor_reclaim_outputs(void)
{
    return hal_gpio_reclaim_shutdown_outputs();
}

bool hal_motor_arm(void)
{
    return hal_pwm_set_armed(true);
}

void hal_motor_disarm(void)
{
    hal_phase_shutdown_all(true);
    hal_pwm_disable_all();
    hal_pwm_set_armed(false);
}

bool hal_motor_is_armed(void)
{
    return hal_pwm_is_armed();
}

void hal_motor_apply_conduction(hal_pwm_phase_t phase, hal_pwm_conduction_t mode,
                                float duty_percent)
{
    if (mode == HAL_PWM_COND_OFF) {
        hal_phase_shutdown_set(phase, true);
        hal_pwm_set_phase_conduction(phase, HAL_PWM_COND_OFF, 0.0f);
        return;
    }

    hal_phase_shutdown_set(phase, false);
    hal_pwm_set_phase_conduction(phase, mode, duty_percent);
}

static void force_all_phases_off(void)
{
    hal_motor_apply_conduction(HAL_PWM_PHASE_A, HAL_PWM_COND_OFF, 0.0f);
    hal_motor_apply_conduction(HAL_PWM_PHASE_B, HAL_PWM_COND_OFF, 0.0f);
    hal_motor_apply_conduction(HAL_PWM_PHASE_C, HAL_PWM_COND_OFF, 0.0f);
}

void hal_motor_apply_step(hal_pwm_conduction_t mode_a, hal_pwm_conduction_t mode_b,
                          hal_pwm_conduction_t mode_c, float duty_percent)
{
    if (duty_percent <= 0.0f) {
        force_all_phases_off();
        return;
    }

    hal_motor_apply_conduction(HAL_PWM_PHASE_A, mode_a, duty_percent);
    hal_motor_apply_conduction(HAL_PWM_PHASE_B, mode_b, duty_percent);
    hal_motor_apply_conduction(HAL_PWM_PHASE_C, mode_c, duty_percent);
}

void hal_motor_halt_outputs(void)
{
    force_all_phases_off();
}

void IRAM_ATTR hal_motor_emergency_shutdown(void)
{
    hal_phase_shutdown_emergency();
}
