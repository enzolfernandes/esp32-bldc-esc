/*
 * motor_control.h — API do núcleo de controle BLDC (ver motor_control.c para passo a passo).
 */

#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "board_config.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Taxa da malha de corrente / PWM (Hz). */
#define MOTOR_CONTROL_LOOP_HZ 1000.0f

/** Corrente alvo máxima na bancada (A). */
#define MOTOR_CONTROL_MAX_TARGET_AMPS 5.0f

/** Duty mínimo (%) para tentar handover ZCD. */
#define MOTOR_CONTROL_MIN_DUTY_ZCD_HANDOVER 8.0f

typedef enum {
    MOTOR_CONTROL_MODE_CURRENT = 0,
    MOTOR_CONTROL_MODE_SPEED = 1
} motor_control_mode_t;

#if MOTOR_CONTROL_USE_SPEED_MODE
#define MOTOR_CONTROL_DEFAULT_MODE MOTOR_CONTROL_MODE_SPEED
#else
#define MOTOR_CONTROL_DEFAULT_MODE MOTOR_CONTROL_MODE_CURRENT
#endif

typedef enum {
    MOTOR_COMM_OPEN_LOOP = 0,
    MOTOR_COMM_ZCD_CLOSED
} motor_comm_mode_t;

/**
 * Sub-FSM de partida (dentro de ESC_STATE_RUNNING):
 * IDLE → ALIGN → RUN (CURRENT) ou RUN_OPEN → RUN_SPEED (SPEED).
 */
typedef enum {
    MOTOR_START_IDLE = 0,
    MOTOR_START_ALIGN,
    MOTOR_START_RUN,
    MOTOR_START_RUN_OPEN,
    MOTOR_START_RUN_SPEED
} motor_start_phase_t;

typedef enum {
    MOTOR_FAULT_NONE = 0,
    MOTOR_FAULT_OVERCURRENT,
    MOTOR_FAULT_STALL,
    MOTOR_FAULT_UNDERVOLTAGE
} motor_fault_reason_t;

bool motor_control_init(void);
void motor_control_on_arm(void);
void motor_control_on_disarm(void);

/** Atualiza PI, comutação e PWM. Chamar só em ESC_STATE_RUNNING. */
void motor_control_tick(void);

/** Corrente desejada (A); modo CURRENT. */
void motor_control_set_target_amps(float amps);

/** RPM desejado; modo SPEED. */
void motor_control_set_target_rpm(float rpm);

/** Força malha aberta (ignora ZCD até novo arm). */
void motor_control_force_open_loop(void);

/** +1 = CW (padrão), -1 = CCW. Só altera com comando de torque zero. */
bool motor_control_set_direction(int8_t direction);
int8_t motor_control_get_direction(void);
const char *motor_control_direction_name(int8_t direction);

bool motor_control_consume_software_fault(void);
void motor_control_trip_uvlo_fault(void);
motor_fault_reason_t motor_control_get_last_fault_reason(void);
const char *motor_control_fault_reason_name(motor_fault_reason_t reason);

bool motor_control_torque_command_active(void);

float motor_control_get_target_amps(void);
float motor_control_get_target_command_amps(void);
float motor_control_get_target_rpm(void);
float motor_control_get_target_command_rpm(void);
float motor_control_get_measured_rpm(void);
motor_control_mode_t motor_control_get_control_mode(void);
const char *motor_control_control_mode_name(motor_control_mode_t mode);

uint8_t motor_control_get_align_step(void);

float motor_control_get_pi_kp(void);
float motor_control_get_pi_ki(void);
float motor_control_get_pi_integral(void);
bool motor_control_set_pi_kp(float kp);
bool motor_control_set_pi_ki(float ki);
void motor_control_reset_pi_integral(void);

float motor_control_get_measured_amps(void);
float motor_control_get_duty_percent(void);
uint8_t motor_control_get_commutation_step(void);
float motor_control_get_open_loop_comm_hz(void);
motor_start_phase_t motor_control_get_start_phase(void);
const char *motor_control_start_phase_name(motor_start_phase_t phase);
motor_comm_mode_t motor_control_get_comm_mode(void);
const char *motor_control_comm_mode_name(motor_comm_mode_t mode);

float motor_control_get_open_loop_comm_hz_max(void);
float motor_control_get_open_loop_comm_ramp_per_step(void);
float motor_control_get_align_duty_percent(void);
uint32_t motor_control_get_align_duration_ms(void);
float motor_control_get_target_slew_amps_per_s(void);
bool motor_control_set_open_loop_comm_hz_max(float hz);
bool motor_control_set_open_loop_comm_ramp_per_step(float hz_per_step);
bool motor_control_set_align_duty_percent(float duty_percent);
bool motor_control_set_align_duration_ms(uint32_t duration_ms);
bool motor_control_set_target_slew_amps_per_s(float amps_per_s);

#ifdef __cplusplus
}
#endif

#endif // MOTOR_CONTROL_H
