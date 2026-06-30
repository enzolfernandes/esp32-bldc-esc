/*
 * motor_control.c — Núcleo do controle de motor BLDC (comutação 6-step + malhas PI).
 *
 * Camada: controle. Cadência: task FreeRTOS 1 kHz no Core 1 (motor_control_tick).
 * Chamadores: fsm_system (init, arm/disarm), main/ps4 (setpoints), timer interno (tick).
 *
 * Fluxo do tick (1 ms): medição → slew → proteções → partida (ALIGN→RUN) → PI → comutação.
 * Ver comentários numerados em motor_control_tick() para o passo a passo completo.
 */

#include "motor_control.h"

#include "bemf_zcd.h"
#include "board_config.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal_motor.h"
#include "ina240_current_sensors.h"
#include "pid_regulator.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#define COMMUTATION_STEP_COUNT 6U

/** Período inicial estimado entre comutações (µs) — deriva de MOTOR_OPEN_LOOP_COMM_HZ_START. */
#define DEFAULT_STEP_PERIOD_US \
    ((uint32_t)(1000000.0f / (6.0f * MOTOR_OPEN_LOOP_COMM_HZ_START)))

/** Timeout sem ZCD antes de voltar à malha aberta (múltiplo do período de passo). */
#define ZCD_WATCHDOG_STEP_MULTIPLIER 4U

/** Padrão de condução das três fases em um passo da sequência trapezoidal. */
typedef struct {
    hal_pwm_conduction_t phase_a;
    hal_pwm_conduction_t phase_b;
    hal_pwm_conduction_t phase_c;
} commutation_pattern_t;

/*
 * Tabela 6-step: passos 0–5, sentido CW (avanço +1). CCW usa índice decrescente.
 * Cada linha: (fase A, fase B, fase C) → SOURCE / SINK / OFF conforme energização do estator.
 * Passo 0 CW: A+ B- C flutuante — padrão clássico de comutação BLDC trapezoidal.
 */
static const commutation_pattern_t s_commutation_table[COMMUTATION_STEP_COUNT] = {
    {HAL_PWM_COND_SOURCE, HAL_PWM_COND_SINK, HAL_PWM_COND_OFF},
    {HAL_PWM_COND_SOURCE, HAL_PWM_COND_OFF, HAL_PWM_COND_SINK},
    {HAL_PWM_COND_OFF, HAL_PWM_COND_SOURCE, HAL_PWM_COND_SINK},
    {HAL_PWM_COND_SINK, HAL_PWM_COND_SOURCE, HAL_PWM_COND_OFF},
    {HAL_PWM_COND_SINK, HAL_PWM_COND_OFF, HAL_PWM_COND_SOURCE},
    {HAL_PWM_COND_OFF, HAL_PWM_COND_SINK, HAL_PWM_COND_SOURCE},
};

/* --- Estado global do módulo (alocação estática, sem malloc) --- */
static pi_controller_t s_current_pi;
static pi_controller_t s_speed_pi;
static float s_target_amps_cmd = 0.0f;
static float s_target_amps = 0.0f;
static float s_target_rpm_cmd = 0.0f;
static float s_target_rpm = 0.0f;
static float s_measured_rpm = 0.0f;
static float s_target_slew_rpm_per_s = MOTOR_SPEED_SLEW_RPM_PER_S;
static float s_measured_amps = 0.0f;
static float s_phase_amps_a = 0.0f;
static float s_phase_amps_b = 0.0f;
static float s_phase_amps_c = 0.0f;
static float s_duty_percent = 0.0f;
static uint8_t s_comm_step = 0U;
static uint32_t s_open_loop_tick_counter = 0U;
static float s_open_loop_comm_hz = MOTOR_OPEN_LOOP_COMM_HZ_START;
static float s_open_loop_comm_hz_max = MOTOR_OPEN_LOOP_COMM_HZ_MAX;
static float s_open_loop_comm_ramp_per_step = MOTOR_OPEN_LOOP_COMM_HZ_RAMP_PER_STEP;
static float s_align_duty_percent = MOTOR_ALIGN_DUTY_PERCENT;
static uint32_t s_align_duration_ms = MOTOR_ALIGN_DURATION_MS;
static float s_target_slew_amps_per_s = MOTOR_TARGET_SLEW_AMPS_PER_S;
static motor_comm_mode_t s_comm_mode = MOTOR_COMM_OPEN_LOOP;
static uint8_t s_zcd_handover_count = 0U;
static uint32_t s_step_period_us = DEFAULT_STEP_PERIOD_US;
static uint64_t s_last_comm_us = 0U;
static uint64_t s_comm_deadline_us = 0U;
static uint64_t s_align_end_us = 0U;
static uint64_t s_align_start_us = 0U;
static motor_start_phase_t s_start_phase = MOTOR_START_IDLE;
static int8_t s_comm_direction = 1;
static volatile bool s_sw_fault_pending = false;

/* --- Instrumentação de latência do tick (Sub-teste 5.1 — esp_timer_get_time) --- */
static uint32_t s_tick_latency_us     = 0U;
static uint32_t s_tick_latency_min_us = UINT32_MAX;
static uint32_t s_tick_latency_max_us = 0U;
static motor_fault_reason_t s_last_fault_reason = MOTOR_FAULT_NONE;
static uint64_t s_stall_begin_us = 0U;
static uint64_t s_low_rpm_stall_begin_us = 0U;
static uint64_t s_last_step_change_us = 0U;
static uint64_t s_handover_begin_us = 0U;
static uint64_t s_desync_begin_us = 0U;
static bool s_active = false;
static TaskHandle_t s_control_task = NULL;

#define MOTOR_CONTROL_TASK_STACK 4096U
#define MOTOR_CONTROL_TASK_PRIO  2
#define MOTOR_CONTROL_TASK_CORE  1

static const float s_control_dt_s = 1.0f / MOTOR_CONTROL_LOOP_HZ;

/** Loop 1 kHz no Core 1, prio 2 — Core 0 reservado ao stack BT/Wi-Fi (rwbt). loopTask prio 1. */
static void motor_control_task_fn(void *arg)
{
    (void)arg;

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period_ticks = pdMS_TO_TICKS(1);

    for (;;) {
        motor_control_tick();
        vTaskDelayUntil(&last_wake, period_ticks);
    }
}

static float clamp_target_amps(float amps)
{
    if (amps < 0.0f) {
        return 0.0f;
    }
    if (amps > MOTOR_CONTROL_MAX_TARGET_AMPS) {
        return MOTOR_CONTROL_MAX_TARGET_AMPS;
    }

    return amps;
}

static float clamp_target_rpm(float rpm)
{
    if (rpm < 0.0f) {
        return 0.0f;
    }
    if (rpm > MOTOR_SPEED_MAX_RPM) {
        return MOTOR_SPEED_MAX_RPM;
    }

    return rpm;
}

static bool is_speed_control_mode(void)
{
#if MOTOR_CONTROL_DEFAULT_MODE == MOTOR_CONTROL_MODE_SPEED
    return true;
#else
    return false;
#endif
}

static bool is_run_phase(motor_start_phase_t phase)
{
    return phase == MOTOR_START_RUN || phase == MOTOR_START_RUN_OPEN ||
           phase == MOTOR_START_RUN_SPEED;
}

static float measure_rpm_from_commutation(void)
{
    if (!is_run_phase(s_start_phase) || s_step_period_us == 0U) {
        return 0.0f;
    }

    const float f_el = 1000000.0f / (6.0f * (float)s_step_period_us);

    return f_el * 60.0f / (float)MOTOR_POLE_PAIRS;
}

static float rpm_to_f_el_hz(float rpm)
{
    float f_el = rpm * (float)MOTOR_POLE_PAIRS / 60.0f;

    if (f_el < MOTOR_OPEN_LOOP_COMM_HZ_START) {
        f_el = MOTOR_OPEN_LOOP_COMM_HZ_START;
    }
    if (f_el > s_open_loop_comm_hz_max) {
        f_el = s_open_loop_comm_hz_max;
    }

    return f_el;
}

static void update_target_slew_rpm(void)
{
    const float max_delta = s_target_slew_rpm_per_s / MOTOR_CONTROL_LOOP_HZ;

    if (s_target_rpm_cmd <= 0.0f) {
        s_target_rpm = 0.0f;
        return;
    }

    if (s_target_rpm < s_target_rpm_cmd) {
        s_target_rpm += max_delta;

        if (s_target_rpm > s_target_rpm_cmd) {
            s_target_rpm = s_target_rpm_cmd;
        }
    } else if (s_target_rpm > s_target_rpm_cmd) {
        s_target_rpm -= max_delta;

        if (s_target_rpm < s_target_rpm_cmd) {
            s_target_rpm = s_target_rpm_cmd;
        }
    }
}

static float clamp_pi_kp(float kp)
{
    if (kp < MOTOR_PI_KP_MIN) {
        return MOTOR_PI_KP_MIN;
    }
    if (kp > MOTOR_PI_KP_MAX) {
        return MOTOR_PI_KP_MAX;
    }

    return kp;
}

static float clamp_pi_ki(float ki)
{
    if (ki < MOTOR_PI_KI_MIN) {
        return MOTOR_PI_KI_MIN;
    }
    if (ki > MOTOR_PI_KI_MAX) {
        return MOTOR_PI_KI_MAX;
    }

    return ki;
}

/** Lê INA240 nas três fases e retorna o valor absoluto máximo (proxy de corrente de barramento). */
static float read_bus_current_amps(void)
{
    const float ia = ina240_read_amps(INA240_PHASE_A);
    const float ib = ina240_read_amps(INA240_PHASE_B);
    const float ic = ina240_read_amps(INA240_PHASE_C);

    s_phase_amps_a = ia;
    s_phase_amps_b = ib;
    s_phase_amps_c = ic;

    float max_phase = fabsf(ia);

    if (fabsf(ib) > max_phase) {
        max_phase = fabsf(ib);
    }
    if (fabsf(ic) > max_phase) {
        max_phase = fabsf(ic);
    }

    return max_phase;
}

/** Aplica o passo atual da tabela 6-step ao MCPWM com o duty cycle calculado pelo PI. */
static void apply_commutation_step(float duty_percent)
{
    const commutation_pattern_t pattern = s_commutation_table[s_comm_step];
    hal_pwm_conduction_t mode_b = pattern.phase_b;
    hal_pwm_conduction_t mode_c = pattern.phase_c;

#if MOTOR_SWAP_PHASES_BC
    mode_b = pattern.phase_c;
    mode_c = pattern.phase_b;
#endif

    hal_motor_apply_step(pattern.phase_a, mode_b, mode_c, duty_percent);
}

static void reset_open_loop_ramp(void)
{
    s_open_loop_tick_counter = 0U;
    s_open_loop_comm_hz = MOTOR_OPEN_LOOP_COMM_HZ_START;
}

static uint8_t align_step_for_direction(void)
{
    if (s_comm_direction >= 0) {
        return MOTOR_ALIGN_STEP_CW;
    }

    return MOTOR_ALIGN_STEP_CCW;
}

static void init_bench_params(void)
{
    s_open_loop_comm_hz_max = MOTOR_OPEN_LOOP_COMM_HZ_MAX;
    s_open_loop_comm_ramp_per_step = MOTOR_OPEN_LOOP_COMM_HZ_RAMP_PER_STEP;
    s_align_duty_percent = MOTOR_ALIGN_DUTY_PERCENT;
    s_align_duration_ms = MOTOR_ALIGN_DURATION_MS;
    s_target_slew_amps_per_s = MOTOR_TARGET_SLEW_AMPS_PER_S;
}

static void update_target_slew(void)
{
    const float max_delta = s_target_slew_amps_per_s / MOTOR_CONTROL_LOOP_HZ;

    if (s_target_amps_cmd <= 0.0f) {
        s_target_amps = 0.0f;
        return;
    }

    if (s_target_amps < s_target_amps_cmd) {
        s_target_amps += max_delta;

        if (s_target_amps > s_target_amps_cmd) {
            s_target_amps = s_target_amps_cmd;
        }
    } else if (s_target_amps > s_target_amps_cmd) {
        s_target_amps -= max_delta;

        if (s_target_amps < s_target_amps_cmd) {
            s_target_amps = s_target_amps_cmd;
        }
    }
}

/** Inicia ALIGN: vetor fixo no estator por MOTOR_ALIGN_DURATION_MS para posicionar o rotor. */
static void begin_align_sequence(void)
{
    s_start_phase = MOTOR_START_ALIGN;
    s_comm_step = align_step_for_direction();
    s_comm_mode = MOTOR_COMM_OPEN_LOOP;
    s_zcd_handover_count = 0U;
    s_comm_deadline_us = 0U;
    s_last_comm_us = 0U;
    reset_open_loop_ramp();
    s_align_start_us = esp_timer_get_time();
    s_align_end_us =
        s_align_start_us + (uint64_t)s_align_duration_ms * 1000ULL;
    s_stall_begin_us = 0U;
}

static void finish_align_sequence(void)
{
    s_current_pi.integral_term = 0.0f;
    reset_open_loop_ramp();
    s_last_step_change_us = esp_timer_get_time();
    s_stall_begin_us = 0U;
    s_handover_begin_us = 0U;
    s_desync_begin_us = 0U;

    if (is_speed_control_mode()) {
        s_start_phase = MOTOR_START_RUN_OPEN;
        s_target_amps_cmd = MOTOR_SPEED_OPEN_LOOP_I_AMPS;
    } else {
        s_start_phase = MOTOR_START_RUN;
    }
}

static float align_ramp_duty_percent(uint64_t now_us)
{
    if (s_align_start_us == 0U || s_align_duration_ms == 0U) {
        return 0.0f;
    }

    const uint64_t duration_us = (uint64_t)s_align_duration_ms * 1000ULL;
    const uint64_t elapsed_us = now_us - s_align_start_us;

    if (elapsed_us >= duration_us) {
        return s_align_duty_percent;
    }

    const float t = (float)elapsed_us / (float)duration_us;

    return s_align_duty_percent * t;
}

static bool run_align_phase(void)
{
    const uint64_t now_us = esp_timer_get_time();

    s_duty_percent = align_ramp_duty_percent(now_us);
    apply_commutation_step(s_duty_percent);

    if (now_us < s_align_end_us) {
        return true;
    }

    finish_align_sequence();
    return false;
}

static void advance_commutation_step(void)
{
    const uint64_t now_us = esp_timer_get_time();

    if (s_last_comm_us != 0U) {
        const uint32_t measured = (uint32_t)(now_us - s_last_comm_us);

        s_step_period_us = (s_step_period_us * 7U + measured) / 8U;
    }

    s_last_comm_us = now_us;
    s_last_step_change_us = now_us;

    if (s_comm_direction >= 0) {
        s_comm_step = (uint8_t)((s_comm_step + 1U) % COMMUTATION_STEP_COUNT);
    } else {
        s_comm_step = (uint8_t)((s_comm_step + COMMUTATION_STEP_COUNT - 1U) %
                                COMMUTATION_STEP_COUNT);
    }
}

/** Registra falha de software, sinaliza FSM e desarma PWM imediatamente. */
static void trip_software_fault(motor_fault_reason_t reason)
{
    s_last_fault_reason = reason;
    s_sw_fault_pending = true;
    motor_control_on_disarm();
}

static bool trip_software_overcurrent(void)
{
    if (s_measured_amps < MOTOR_SOFTWARE_OC_AMPS) {
        return false;
    }

    trip_software_fault(MOTOR_FAULT_OVERCURRENT);
    return true;
}

static bool trip_stall_high_current(void)
{
    if (!is_run_phase(s_start_phase)) {
        s_stall_begin_us = 0U;
        return false;
    }

    if (s_measured_amps < MOTOR_STALL_CURRENT_AMPS) {
        s_stall_begin_us = 0U;
        return false;
    }

    const uint64_t now_us = esp_timer_get_time();

    if (s_stall_begin_us == 0U) {
        s_stall_begin_us = now_us;
        return false;
    }

    if ((now_us - s_stall_begin_us) <
        (uint64_t)MOTOR_STALL_TIMEOUT_MS * 1000ULL) {
        return false;
    }

    trip_software_fault(MOTOR_FAULT_STALL);
    return true;
}

static bool trip_stall_no_commutation(void)
{
    uint64_t step_period_us;
    uint64_t timeout_us;

    if (!is_run_phase(s_start_phase) || s_open_loop_comm_hz <= 0.0f) {
        return false;
    }

    step_period_us = (uint64_t)(1000000.0f / s_open_loop_comm_hz);
    timeout_us = step_period_us * (uint64_t)MOTOR_STALL_STEP_TIMEOUT_MULT;

    if (s_last_step_change_us == 0U) {
        return false;
    }

    if ((esp_timer_get_time() - s_last_step_change_us) < timeout_us) {
        return false;
    }

    trip_software_fault(MOTOR_FAULT_STALL);
    return true;
}

static bool trip_stall_low_rpm(void)
{
    const uint64_t now_us = esp_timer_get_time();

    if (s_start_phase != MOTOR_START_RUN_SPEED) {
        return false;
    }

    if (s_target_rpm_cmd <= MOTOR_SPEED_OPEN_LOOP_HANDOVER_RPM) {
        return false;
    }

    if (s_measured_rpm >= MOTOR_SPEED_MIN_RPM) {
        s_low_rpm_stall_begin_us = 0U;
        return false;
    }

    if (s_low_rpm_stall_begin_us == 0U) {
        s_low_rpm_stall_begin_us = now_us;
        return false;
    }

    if ((now_us - s_low_rpm_stall_begin_us) <
        (uint64_t)MOTOR_STALL_TIMEOUT_MS * 1000ULL) {
        return false;
    }

    trip_software_fault(MOTOR_FAULT_STALL);
    return true;
}

static bool check_stall_conditions(void)
{
    /* RUN_OPEN: comutação forçada por timer — não exige avanço mecânico; só OCP sustentada. */
    if (s_start_phase == MOTOR_START_RUN_OPEN) {
        return trip_stall_high_current();
    }

    if (trip_stall_high_current()) {
        return true;
    }

    if (trip_stall_no_commutation()) {
        return true;
    }

    return trip_stall_low_rpm();
}

static void ramp_open_loop_comm_hz(void)
{
    s_open_loop_comm_hz += s_open_loop_comm_ramp_per_step;

    if (s_open_loop_comm_hz > s_open_loop_comm_hz_max) {
        s_open_loop_comm_hz = s_open_loop_comm_hz_max;
    }
}

/** RUN_OPEN (SPEED): f_el segue R2 com slew — sem rampa automática por passo. */
static void update_open_loop_hz_from_rpm_in_run_open(void)
{
    if (s_start_phase != MOTOR_START_RUN_OPEN || !is_speed_control_mode()) {
        return;
    }

#if MOTOR_BENCH_FIX_RUN_OPEN_F_EL
    const float f_cmd = MOTOR_BENCH_RUN_OPEN_F_EL_HZ;
#else
    float f_cmd = rpm_to_f_el_hz(s_target_rpm);

    if (f_cmd > MOTOR_OPEN_LOOP_RUN_OPEN_RAMP_MAX_HZ) {
        f_cmd = MOTOR_OPEN_LOOP_RUN_OPEN_RAMP_MAX_HZ;
    }
#endif

    const float max_delta =
        MOTOR_OPEN_LOOP_RUN_OPEN_F_EL_SLEW_HZ_PER_S / MOTOR_CONTROL_LOOP_HZ;

    if (s_open_loop_comm_hz < f_cmd) {
        s_open_loop_comm_hz += max_delta;

        if (s_open_loop_comm_hz > f_cmd) {
            s_open_loop_comm_hz = f_cmd;
        }
    } else if (s_open_loop_comm_hz > f_cmd) {
        s_open_loop_comm_hz -= max_delta;

        if (s_open_loop_comm_hz < f_cmd) {
            s_open_loop_comm_hz = f_cmd;
        }
    }
}

static void advance_open_loop_if_due(bool ramp_step)
{
    const float steps_per_sec = 6.0f * s_open_loop_comm_hz;
    const uint32_t ticks_per_step =
        (steps_per_sec > 0.0f)
            ? (uint32_t)((MOTOR_CONTROL_LOOP_HZ / steps_per_sec) + 0.5f)
            : 1U;

    if (ticks_per_step < 1U) {
        return;
    }

    s_open_loop_tick_counter++;

    if (s_open_loop_tick_counter < ticks_per_step) {
        return;
    }

    s_open_loop_tick_counter = 0U;
    advance_commutation_step();

    if (ramp_step) {
        if (s_start_phase != MOTOR_START_RUN_OPEN) {
            ramp_open_loop_comm_hz();
        }
    }
}

static uint32_t comm_delay_us_from_period(void)
{
    const float scale = BEMF_COMM_DELAY_DEG_ELEC / 60.0f;

    if (scale <= 0.0f) {
        return 1U;
    }

    const uint32_t delay = (uint32_t)((float)s_step_period_us * scale);

    return (delay < 1U) ? 1U : delay;
}

static void schedule_commutation_after_zcd(void)
{
    const uint64_t now_us = esp_timer_get_time();

    s_comm_deadline_us = now_us + comm_delay_us_from_period();
}

static bool zcd_handover_enabled(void)
{
    return bemf_zcd_is_ready() && (s_duty_percent >= MOTOR_CONTROL_MIN_DUTY_ZCD_HANDOVER);
}

static void try_zcd_handover(void)
{
    const ina240_phase_t float_phase = bemf_zcd_floating_phase_for_step(s_comm_step);

    if (!bemf_zcd_consume_edge(float_phase)) {
        return;
    }

    s_zcd_handover_count++;

    if (s_zcd_handover_count >= BEMF_ZCD_HANDOVER_COUNT) {
        s_comm_mode = MOTOR_COMM_ZCD_CLOSED;
        s_open_loop_tick_counter = 0U;
        s_comm_deadline_us = 0U;
        s_last_comm_us = esp_timer_get_time();
    }
}

static void run_open_loop_commutation(bool ramp_step)
{
    const bool torque_on =
        (s_target_amps > 0.0f) ||
        (is_speed_control_mode() && s_target_rpm > 0.0f);

    if (torque_on) {
        advance_open_loop_if_due(ramp_step);
    }

    if (zcd_handover_enabled()) {
        try_zcd_handover();
    }
}

static void try_speed_handover(void)
{
    const uint64_t now_us = esp_timer_get_time();

    if (s_start_phase != MOTOR_START_RUN_OPEN) {
        s_handover_begin_us = 0U;
        return;
    }

    s_measured_rpm = measure_rpm_from_commutation();

    if (s_measured_rpm < MOTOR_SPEED_OPEN_LOOP_HANDOVER_RPM) {
        s_handover_begin_us = 0U;
        return;
    }

    if (s_handover_begin_us == 0U) {
        s_handover_begin_us = now_us;
        return;
    }

    if ((now_us - s_handover_begin_us) <
        (uint64_t)MOTOR_SPEED_HANDOVER_MS * 1000ULL) {
        return;
    }

    s_start_phase = MOTOR_START_RUN_SPEED;
    s_speed_pi.integral_term = 0.0f;
    s_handover_begin_us = 0U;
    s_desync_begin_us = 0U;
}

static void try_speed_desync_fallback(void)
{
    const uint64_t now_us = esp_timer_get_time();
    const float rpm_err = fabsf(s_measured_rpm - s_target_rpm);

    if (s_start_phase != MOTOR_START_RUN_SPEED) {
        s_desync_begin_us = 0U;
        return;
    }

    if (s_target_rpm <= MOTOR_SPEED_OPEN_LOOP_HANDOVER_RPM) {
        s_desync_begin_us = 0U;
        return;
    }

    if (rpm_err <= MOTOR_SPEED_DESYNC_RPM) {
        s_desync_begin_us = 0U;
        return;
    }

    if (s_desync_begin_us == 0U) {
        s_desync_begin_us = now_us;
        return;
    }

    if ((now_us - s_desync_begin_us) <
        (uint64_t)MOTOR_SPEED_DESYNC_TIMEOUT_MS * 1000ULL) {
        return;
    }

    s_start_phase = MOTOR_START_RUN_OPEN;
    s_speed_pi.integral_term = 0.0f;
    s_target_amps_cmd = MOTOR_SPEED_OPEN_LOOP_I_AMPS;
    reset_open_loop_ramp();
    s_handover_begin_us = 0U;
    s_desync_begin_us = 0U;
}

static void run_zcd_closed_commutation(void)
{
    const uint64_t now_us = esp_timer_get_time();
    const ina240_phase_t float_phase = bemf_zcd_floating_phase_for_step(s_comm_step);

    if (bemf_zcd_consume_edge(float_phase)) {
        schedule_commutation_after_zcd();
    }

    if (s_comm_deadline_us != 0U && now_us >= s_comm_deadline_us) {
        s_comm_deadline_us = 0U;
        advance_commutation_step();
        apply_commutation_step(s_duty_percent);
    }

    if (s_last_comm_us != 0U) {
        const uint64_t watchdog_us =
            (uint64_t)s_step_period_us * ZCD_WATCHDOG_STEP_MULTIPLIER;

        if (now_us - s_last_comm_us > watchdog_us) {
            s_comm_mode = MOTOR_COMM_OPEN_LOOP;
            s_zcd_handover_count = 0U;
            s_open_loop_tick_counter = 0U;
            s_comm_deadline_us = 0U;
        }
    }
}

/**
 * @brief Inicializa PIs, estado e temporizador periódico de 1 kHz.
 * Chamado no fim de fsm_system_init(). O timer roda sempre; tick só atua se s_active.
 */
bool motor_control_init(void)
{
    init_bench_params();

    // PI de corrente: saída = duty cycle (%)
    s_current_pi = (pi_controller_t){
        .kp = MOTOR_PI_KP_DEFAULT,
        .ki = MOTOR_PI_KI_DEFAULT,
        .dt = s_control_dt_s,
        .integral_term = 0.0f,
        .out_min = 0.0f,
        .out_max = MAX_DUTY_CYCLE_PERCENT,
        .integ_min = MOTOR_PI_INTEG_MIN,
        .integ_max = MOTOR_PI_INTEG_MAX,
    };

    // PI de velocidade (modo SPEED): saída = corrente de comando (A)
    s_speed_pi = (pi_controller_t){
        .kp = MOTOR_SPEED_PI_KP_DEFAULT,
        .ki = MOTOR_SPEED_PI_KI_DEFAULT,
        .dt = s_control_dt_s,
        .integral_term = 0.0f,
        .out_min = 0.0f,
        .out_max = MOTOR_CONTROL_MAX_TARGET_AMPS,
        .integ_min = MOTOR_SPEED_PI_INTEG_MIN,
        .integ_max = MOTOR_SPEED_PI_INTEG_MAX,
    };

    s_target_amps_cmd = 0.0f;
    s_target_amps = 0.0f;
    s_target_rpm_cmd = 0.0f;
    s_target_rpm = 0.0f;
    s_measured_rpm = 0.0f;
    s_target_slew_rpm_per_s = MOTOR_SPEED_SLEW_RPM_PER_S;
    s_measured_amps = 0.0f;
    s_duty_percent = 0.0f;
    s_comm_step = 0U;
    s_open_loop_tick_counter = 0U;
    s_open_loop_comm_hz = MOTOR_OPEN_LOOP_COMM_HZ_START;
    s_comm_mode = MOTOR_COMM_OPEN_LOOP;
    s_zcd_handover_count = 0U;
    s_step_period_us = DEFAULT_STEP_PERIOD_US;
    s_last_comm_us = 0U;
    s_comm_deadline_us = 0U;
    s_align_end_us = 0U;
    s_start_phase = MOTOR_START_IDLE;
    s_comm_direction = 1;
    s_sw_fault_pending = false;
    s_last_fault_reason = MOTOR_FAULT_NONE;
    s_stall_begin_us = 0U;
    s_last_step_change_us = 0U;
    s_handover_begin_us = 0U;
    s_desync_begin_us = 0U;
    s_low_rpm_stall_begin_us = 0U;
    s_active = false;

    if (s_control_task == NULL) {
        const BaseType_t ok = xTaskCreatePinnedToCore(
            motor_control_task_fn,
            "motor_ctrl",
            MOTOR_CONTROL_TASK_STACK,
            NULL,
            MOTOR_CONTROL_TASK_PRIO,
            &s_control_task,
            MOTOR_CONTROL_TASK_CORE);

        if (ok != pdPASS) {
            s_control_task = NULL;
            return false;
        }
    }

    return true;
}

/**
 * @brief Arma a malha de controle (transição IDLE→RUNNING na FSM).
 * Zera integradores e inicia sub-FSM em MOTOR_START_IDLE (próximo tick → ALIGN).
 */
void motor_control_on_arm(void)
{
    s_current_pi.integral_term = 0.0f;
    s_speed_pi.integral_term = 0.0f;
    s_comm_step = 0U;
    s_open_loop_tick_counter = 0U;
    s_open_loop_comm_hz = MOTOR_OPEN_LOOP_COMM_HZ_START;
    s_comm_mode = MOTOR_COMM_OPEN_LOOP;
    s_zcd_handover_count = 0U;
    s_step_period_us = DEFAULT_STEP_PERIOD_US;
    s_last_comm_us = 0U;
    s_comm_deadline_us = 0U;
    s_duty_percent = 0.0f;
    s_start_phase = MOTOR_START_IDLE;
    s_align_end_us = 0U;
    s_align_start_us = 0U;
    s_last_fault_reason = MOTOR_FAULT_NONE;
    s_stall_begin_us = 0U;
    s_last_step_change_us = 0U;
    s_handover_begin_us = 0U;
    s_desync_begin_us = 0U;
    s_low_rpm_stall_begin_us = 0U;
    apply_commutation_step(0.0f);
    s_active = true;
}

/** Desarma: para referências, zera duty e desliga todas as pernas PWM via HAL. */
void motor_control_on_disarm(void)
{
    s_active = false;
    s_target_amps_cmd = 0.0f;
    s_target_amps = 0.0f;
    s_target_rpm_cmd = 0.0f;
    s_target_rpm = 0.0f;
    s_measured_rpm = 0.0f;
    s_duty_percent = 0.0f;
    s_comm_deadline_us = 0U;
    s_start_phase = MOTOR_START_IDLE;
    s_align_end_us = 0U;
    s_align_start_us = 0U;
    s_low_rpm_stall_begin_us = 0U;
    hal_motor_halt_outputs();
}

void motor_control_force_open_loop(void)
{
    s_comm_mode = MOTOR_COMM_OPEN_LOOP;
    s_zcd_handover_count = 0U;
    s_open_loop_tick_counter = 0U;
    s_open_loop_comm_hz = MOTOR_OPEN_LOOP_COMM_HZ_START;
    s_comm_deadline_us = 0U;
}

bool motor_control_set_direction(int8_t direction)
{
    if (s_active && motor_control_torque_command_active()) {
        return false;
    }

    s_comm_direction = (direction >= 0) ? 1 : -1;
    return true;
}

bool motor_control_torque_command_active(void)
{
#if MOTOR_CONTROL_DEFAULT_MODE == MOTOR_CONTROL_MODE_SPEED
    return s_target_rpm_cmd > 0.0f;
#else
    return s_target_amps_cmd > 0.0f;
#endif
}

int8_t motor_control_get_direction(void)
{
    return s_comm_direction;
}

const char *motor_control_direction_name(int8_t direction)
{
    return (direction >= 0) ? "CW" : "CCW";
}

bool motor_control_consume_software_fault(void)
{
    if (!s_sw_fault_pending) {
        return false;
    }

    s_sw_fault_pending = false;
    return true;
}

void motor_control_trip_uvlo_fault(void)
{
    s_last_fault_reason = MOTOR_FAULT_UNDERVOLTAGE;
    motor_control_on_disarm();
}

motor_fault_reason_t motor_control_get_last_fault_reason(void)
{
    return s_last_fault_reason;
}

const char *motor_control_fault_reason_name(motor_fault_reason_t reason)
{
    switch (reason) {
    case MOTOR_FAULT_OVERCURRENT:
        return "OC_SW";
    case MOTOR_FAULT_STALL:
        return "STALL";
    case MOTOR_FAULT_UNDERVOLTAGE:
        return "UVLO";
    default:
        return "none";
    }
}

/**
 * @brief Malha de controle periódica (1 kHz, task FreeRTOS no Core 1).
 * Executa somente com s_active == true (após motor_control_on_arm).
 */
void motor_control_tick(void)
{
    // --- Etapa 1: guarda — timer ativo mas ESC desarmado não atua no PWM ---
    if (!s_active) {
        return;
    }

    // Marca o início do caminho ativo do tick para medir a latência real do controle.
    const int64_t t_tick_start_us = esp_timer_get_time();

    // --- Etapa 2: aquisição — corrente máxima entre fases A/B/C (INA240 → ADC) ---
    s_measured_amps = read_bus_current_amps();

    // --- Etapa 3: slew — limita taxa de mudança do comando (RPM ou corrente) ---
    if (is_speed_control_mode()) {
        update_target_slew_rpm();
    } else {
        update_target_slew();
    }

    // --- Etapa 4: proteção OC em software (até 1 ms; complementa LM339) ---
    if (motor_control_torque_command_active() && trip_software_overcurrent()) {
        return;
    }

    // --- Etapa 5: detecção de stall (corrente alta, passo parado ou RPM baixo) ---
    if (motor_control_torque_command_active() && check_stall_conditions()) {
        return;
    }

    // --- Etapa 6: sem torque (R2 solto) — reset completo e PWM zerado ---
    if (!motor_control_torque_command_active()) {
        s_stall_begin_us = 0U;
        s_handover_begin_us = 0U;
        s_desync_begin_us = 0U;
        s_low_rpm_stall_begin_us = 0U;
        s_duty_percent = 0.0f;
        s_current_pi.integral_term = 0.0f;
        s_speed_pi.integral_term = 0.0f;
        s_target_amps = 0.0f;
        s_target_rpm = 0.0f;
        s_measured_rpm = 0.0f;
        s_comm_mode = MOTOR_COMM_OPEN_LOOP;
        s_zcd_handover_count = 0U;
        s_comm_deadline_us = 0U;
        s_start_phase = MOTOR_START_IDLE;
        s_align_end_us = 0U;
        reset_open_loop_ramp();
        apply_commutation_step(0.0f);
        return;
    }

    // --- Etapa 7: partida — sub-FSM ALIGN → RUN_OPEN / RUN / RUN_SPEED ---
    if (s_start_phase == MOTOR_START_IDLE) {
        begin_align_sequence();
    }

    if (s_start_phase == MOTOR_START_ALIGN) {
        if (run_align_phase()) {
            return;
        }
    }

    // --- Etapa 8: referência de corrente (cascata SPEED ou direta CURRENT) ---
    if (is_speed_control_mode() && s_start_phase == MOTOR_START_RUN_OPEN) {
        s_target_amps_cmd = MOTOR_SPEED_OPEN_LOOP_I_AMPS;
        update_target_slew();
        update_open_loop_hz_from_rpm_in_run_open();
        s_measured_rpm = measure_rpm_from_commutation();
        try_speed_handover();
    } else if (is_speed_control_mode() && s_start_phase == MOTOR_START_RUN_SPEED) {
        s_measured_rpm = measure_rpm_from_commutation();
        s_open_loop_comm_hz = rpm_to_f_el_hz(s_target_rpm);
        s_target_amps_cmd =
            pi_compute(&s_speed_pi, s_target_rpm, s_measured_rpm);
        update_target_slew();
        try_speed_desync_fallback();
    } else {
        update_target_slew();
    }

    // --- Etapa 9: duty — ALIGN/RUN_OPEN usam tensão fixa; PI só em RUN / RUN_SPEED ---
    if (s_start_phase == MOTOR_START_ALIGN ||
        s_start_phase == MOTOR_START_RUN_OPEN) {
        s_duty_percent = s_align_duty_percent;
        s_current_pi.integral_term = 0.0f;
    } else {
        s_duty_percent =
            pi_compute(&s_current_pi, s_target_amps, s_measured_amps);
    }

    // --- Etapa 10: comutação — malha aberta (timer) ou fechada por ZCD/BEMF ---
    if (s_comm_mode == MOTOR_COMM_ZCD_CLOSED && bemf_zcd_is_ready()) {
        run_zcd_closed_commutation();
    } else {
        s_comm_mode = MOTOR_COMM_OPEN_LOOP;
        const bool ramp_step =
            !is_speed_control_mode() || s_start_phase == MOTOR_START_RUN;

        run_open_loop_commutation(ramp_step);
    }

    // --- Etapa 11: aplica passo 6-step e duty ao MCPWM (três fases) ---
    apply_commutation_step(s_duty_percent);

    // Registra latência do caminho completo do tick (Etapas 2–11).
    const uint32_t elapsed_us = (uint32_t)(esp_timer_get_time() - t_tick_start_us);
    s_tick_latency_us = elapsed_us;
    if (elapsed_us < s_tick_latency_min_us) { s_tick_latency_min_us = elapsed_us; }
    if (elapsed_us > s_tick_latency_max_us) { s_tick_latency_max_us = elapsed_us; }
}

/* --- API pública: setpoints (chamada por main/ps4 a cada poll de 20 ms) --- */
void motor_control_set_target_amps(float amps)
{
    s_target_amps_cmd = clamp_target_amps(amps);
}

void motor_control_set_target_rpm(float rpm)
{
    s_target_rpm_cmd = clamp_target_rpm(rpm);
}

float motor_control_get_target_rpm(void)
{
    return s_target_rpm;
}

float motor_control_get_target_command_rpm(void)
{
    return s_target_rpm_cmd;
}

float motor_control_get_measured_rpm(void)
{
    return s_measured_rpm;
}

motor_control_mode_t motor_control_get_control_mode(void)
{
    return MOTOR_CONTROL_DEFAULT_MODE;
}

const char *motor_control_control_mode_name(motor_control_mode_t mode)
{
    switch (mode) {
    case MOTOR_CONTROL_MODE_CURRENT:
        return "CURRENT";
    case MOTOR_CONTROL_MODE_SPEED:
        return "SPEED";
    default:
        return "?";
    }
}

/* --- Getters e setters de telemetria/ajuste (usados por main.cpp na serial) --- */
float motor_control_get_target_amps(void)
{
    return s_target_amps;
}

float motor_control_get_target_command_amps(void)
{
    return s_target_amps_cmd;
}

uint8_t motor_control_get_align_step(void)
{
    return align_step_for_direction();
}

float motor_control_get_pi_kp(void)
{
    return s_current_pi.kp;
}

float motor_control_get_pi_ki(void)
{
    return s_current_pi.ki;
}

float motor_control_get_pi_integral(void)
{
    return s_current_pi.integral_term;
}

bool motor_control_set_pi_kp(float kp)
{
    s_current_pi.kp = clamp_pi_kp(kp);
    s_current_pi.integral_term = 0.0f;
    return true;
}

bool motor_control_set_pi_ki(float ki)
{
    s_current_pi.ki = clamp_pi_ki(ki);
    s_current_pi.integral_term = 0.0f;
    return true;
}

void motor_control_reset_pi_integral(void)
{
    s_current_pi.integral_term = 0.0f;
}

float motor_control_get_measured_amps(void)
{
    return s_measured_amps;
}

float motor_control_get_phase_amps_a(void)
{
    return s_phase_amps_a;
}

float motor_control_get_phase_amps_b(void)
{
    return s_phase_amps_b;
}

float motor_control_get_phase_amps_c(void)
{
    return s_phase_amps_c;
}

float motor_control_get_duty_percent(void)
{
    return s_duty_percent;
}

uint8_t motor_control_get_commutation_step(void)
{
    return s_comm_step;
}

float motor_control_get_open_loop_comm_hz(void)
{
    return s_open_loop_comm_hz;
}

motor_start_phase_t motor_control_get_start_phase(void)
{
    return s_start_phase;
}

const char *motor_control_start_phase_name(motor_start_phase_t phase)
{
    switch (phase) {
    case MOTOR_START_IDLE:
        return "idle";
    case MOTOR_START_ALIGN:
        return "ALIGN";
    case MOTOR_START_RUN:
        return "RUN";
    case MOTOR_START_RUN_OPEN:
        return "RUN_OPEN";
    case MOTOR_START_RUN_SPEED:
        return "RUN_SPEED";
    default:
        return "?";
    }
}

motor_comm_mode_t motor_control_get_comm_mode(void)
{
    return s_comm_mode;
}

const char *motor_control_comm_mode_name(motor_comm_mode_t mode)
{
    switch (mode) {
    case MOTOR_COMM_OPEN_LOOP:
        return "OPEN";
    case MOTOR_COMM_ZCD_CLOSED:
        return "ZCD";
    default:
        return "?";
    }
}

float motor_control_get_open_loop_comm_hz_max(void)
{
    return s_open_loop_comm_hz_max;
}

float motor_control_get_open_loop_comm_ramp_per_step(void)
{
    return s_open_loop_comm_ramp_per_step;
}

float motor_control_get_align_duty_percent(void)
{
    return s_align_duty_percent;
}

uint32_t motor_control_get_align_duration_ms(void)
{
    return s_align_duration_ms;
}

float motor_control_get_target_slew_amps_per_s(void)
{
    return s_target_slew_amps_per_s;
}

bool motor_control_set_open_loop_comm_hz_max(float hz)
{
    if (hz < MOTOR_OPEN_LOOP_COMM_HZ_START) {
        hz = MOTOR_OPEN_LOOP_COMM_HZ_START;
    }
    if (hz > MOTOR_OPEN_LOOP_COMM_HZ_MAX_LIMIT) {
        hz = MOTOR_OPEN_LOOP_COMM_HZ_MAX_LIMIT;
    }

    s_open_loop_comm_hz_max = hz;

    if (s_open_loop_comm_hz > s_open_loop_comm_hz_max) {
        s_open_loop_comm_hz = s_open_loop_comm_hz_max;
    }

    return true;
}

bool motor_control_set_open_loop_comm_ramp_per_step(float hz_per_step)
{
    if (hz_per_step < MOTOR_OPEN_LOOP_COMM_RAMP_MIN) {
        hz_per_step = MOTOR_OPEN_LOOP_COMM_RAMP_MIN;
    }
    if (hz_per_step > MOTOR_OPEN_LOOP_COMM_RAMP_MAX) {
        hz_per_step = MOTOR_OPEN_LOOP_COMM_RAMP_MAX;
    }

    s_open_loop_comm_ramp_per_step = hz_per_step;
    return true;
}

bool motor_control_set_align_duty_percent(float duty_percent)
{
    if (duty_percent < MOTOR_ALIGN_DUTY_MIN) {
        duty_percent = MOTOR_ALIGN_DUTY_MIN;
    }
    if (duty_percent > MOTOR_ALIGN_DUTY_MAX) {
        duty_percent = MOTOR_ALIGN_DUTY_MAX;
    }

    s_align_duty_percent = duty_percent;
    return true;
}

bool motor_control_set_align_duration_ms(uint32_t duration_ms)
{
    if (duration_ms < MOTOR_ALIGN_DURATION_MS_MIN) {
        duration_ms = MOTOR_ALIGN_DURATION_MS_MIN;
    }
    if (duration_ms > MOTOR_ALIGN_DURATION_MS_MAX) {
        duration_ms = MOTOR_ALIGN_DURATION_MS_MAX;
    }

    s_align_duration_ms = duration_ms;
    return true;
}

bool motor_control_set_target_slew_amps_per_s(float amps_per_s)
{
    if (amps_per_s < MOTOR_TARGET_SLEW_MIN) {
        amps_per_s = MOTOR_TARGET_SLEW_MIN;
    }
    if (amps_per_s > MOTOR_TARGET_SLEW_MAX) {
        amps_per_s = MOTOR_TARGET_SLEW_MAX;
    }

    s_target_slew_amps_per_s = amps_per_s;
    return true;
}

/* --- Getters de latência do tick (Sub-teste 5.1) --- */
uint32_t motor_control_get_tick_latency_us(void)
{
    return s_tick_latency_us;
}

uint32_t motor_control_get_tick_latency_min_us(void)
{
    return (s_tick_latency_min_us == UINT32_MAX) ? 0U : s_tick_latency_min_us;
}

uint32_t motor_control_get_tick_latency_max_us(void)
{
    return s_tick_latency_max_us;
}
