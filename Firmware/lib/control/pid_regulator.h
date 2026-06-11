/*
 * pid_regulator.h — API do controlador PI (ver pid_regulator.c para algoritmo linha a linha).
 */

#ifndef PID_REGULATOR_H
#define PID_REGULATOR_H

#ifdef __cplusplus
extern "C" {
#endif

/** Estado de um controlador PI; uma instância por malha (corrente, velocidade). */
typedef struct {
    float kp;             /**< Ganho proporcional */
    float ki;             /**< Ganho integral */
    float dt;             /**< Período de amostragem [s] — alinhado ao esp_timer (1 ms) */
    float integral_term;  /**< Estado do integrador I_{k-1} */
    float out_max;        /**< Saturação máxima da saída u (ex.: 95 % duty) */
    float out_min;        /**< Saturação mínima da saída u */
    float integ_max;      /**< Teto do integrador (anti-windup) */
    float integ_min;      /**< Piso do integrador (anti-windup) */
} pi_controller_t;

float pi_compute(pi_controller_t *pi, float setpoint, float measurement);

#ifdef __cplusplus
}
#endif

#endif // PID_REGULATOR_H
