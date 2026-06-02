#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

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
    MOTOR_COMM_OPEN_LOOP = 0,
    MOTOR_COMM_ZCD_CLOSED
} motor_comm_mode_t;

bool motor_control_init(void);
void motor_control_on_arm(void);
void motor_control_on_disarm(void);

/** Atualiza PI, comutação e PWM. Chamar só em ESC_STATE_RUNNING. */
void motor_control_tick(void);

/** Corrente desejada (A); 0 desliga torque (duty mínimo). */
void motor_control_set_target_amps(float amps);

/** Força malha aberta (ignora ZCD até novo arm). */
void motor_control_force_open_loop(void);

float motor_control_get_target_amps(void);
float motor_control_get_measured_amps(void);
float motor_control_get_duty_percent(void);
uint8_t motor_control_get_commutation_step(void);
motor_comm_mode_t motor_control_get_comm_mode(void);
const char *motor_control_comm_mode_name(motor_comm_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif // MOTOR_CONTROL_H
