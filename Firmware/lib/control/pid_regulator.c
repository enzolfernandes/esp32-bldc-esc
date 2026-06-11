/*
 * pid_regulator.c — Controlador PI proporcional-integral (sem termo D).
 *
 * Camada: controle. Agnóstico de hardware — recebe apenas floats (referência, medição).
 * Chamado por motor_control a 1 kHz para malhas de corrente e velocidade.
 *
 * Equação discreta (a cada amostra k, período dt):
 *   e_k = r - y_k
 *   P_k = Kp * e_k
 *   I_k = clamp(I_{k-1} + Ki * e_k * dt, I_min, I_max)   ← anti-windup
 *   u_k = clamp(P_k + I_k, u_min, u_max)
 */

#include "pid_regulator.h"

#include <stddef.h>

/** Limita x ao intervalo [min_v, max_v] — usado no integrador e na saída. */
static inline float clampf(float x, float min_v, float max_v)
{
    if (x > max_v) {
        return max_v;
    }
    if (x < min_v) {
        return min_v;
    }
    return x;
}

/**
 * @brief Calcula a saída do controlador PI para uma amostra.
 * @param pi       Estado do controlador (ganhos, integrador, limites).
 * @param setpoint Referência r (ex.: corrente ou RPM desejados).
 * @param measurement Medição y_k (ex.: corrente lida pelo INA240).
 * @return Saída u_k saturada (ex.: duty % ou corrente de comando).
 */
float pi_compute(pi_controller_t *pi, float setpoint, float measurement)
{
    float error;
    float p_term;
    float u_unsat;
    float u_sat;

    // Proteção defensiva: ponteiro inválido retorna zero (sem atuar no motor)
    if (pi == NULL) {
        return 0.0f;
    }

    // Passo 1: erro entre referência e medição
    error = setpoint - measurement;

    // Passo 2: termo proporcional — resposta imediata ao erro atual
    p_term = pi->kp * error;

    // Passo 3: integrador Euler com anti-windup por clamping do estado integral
    // Acumula Ki * e * dt; limita I antes de somar à saída (evita overshoot pós-saturação)
    pi->integral_term += (pi->ki * error * pi->dt);
    pi->integral_term = clampf(pi->integral_term, pi->integ_min, pi->integ_max);

    // Passo 4: soma P + I e satura a saída (ex.: 0–95 % duty)
    u_unsat = p_term + pi->integral_term;
    u_sat = clampf(u_unsat, pi->out_min, pi->out_max);

    return u_sat;
}
