#include "motor_control.h"

#include "bemf_zcd.h"
#include "board_config.h"
#include "esp_timer.h"
#include "hal_pwm.h"
#include "ina240_current_sensors.h"
#include "pid_regulator.h"

#include <math.h>
#include <stddef.h>

#define COMMUTATION_STEP_COUNT 6U

/** Passos elétricos por segundo na partida em malha aberta. */
#define DEFAULT_COMMUTATION_HZ 30.0f

/** Período inicial estimado entre comutações (µs) — 30 Hz elétrico ≈ 333 ms / 6. */
#define DEFAULT_STEP_PERIOD_US 55000U

/** Timeout sem ZCD antes de voltar à malha aberta (múltiplo do período de passo). */
#define ZCD_WATCHDOG_STEP_MULTIPLIER 4U

typedef struct {
    hal_pwm_conduction_t phase_a;
    hal_pwm_conduction_t phase_b;
    hal_pwm_conduction_t phase_c;
} commutation_pattern_t;

static const commutation_pattern_t s_commutation_table[COMMUTATION_STEP_COUNT] = {
    {HAL_PWM_COND_SOURCE, HAL_PWM_COND_SINK, HAL_PWM_COND_OFF},
    {HAL_PWM_COND_SOURCE, HAL_PWM_COND_OFF, HAL_PWM_COND_SINK},
    {HAL_PWM_COND_OFF, HAL_PWM_COND_SOURCE, HAL_PWM_COND_SINK},
    {HAL_PWM_COND_SINK, HAL_PWM_COND_SOURCE, HAL_PWM_COND_OFF},
    {HAL_PWM_COND_SINK, HAL_PWM_COND_OFF, HAL_PWM_COND_SOURCE},
    {HAL_PWM_COND_OFF, HAL_PWM_COND_SINK, HAL_PWM_COND_SOURCE},
};

static pi_controller_t s_current_pi;
static float s_target_amps = 0.0f;
static float s_measured_amps = 0.0f;
static float s_duty_percent = 0.0f;
static uint8_t s_comm_step = 0U;
static uint32_t s_open_loop_tick_counter = 0U;
static motor_comm_mode_t s_comm_mode = MOTOR_COMM_OPEN_LOOP;
static uint8_t s_zcd_handover_count = 0U;
static uint32_t s_step_period_us = DEFAULT_STEP_PERIOD_US;
static uint64_t s_last_comm_us = 0U;
static uint64_t s_comm_deadline_us = 0U;
static bool s_active = false;
static esp_timer_handle_t s_control_timer = NULL;

static const float s_control_dt_s = 1.0f / MOTOR_CONTROL_LOOP_HZ;

static void control_timer_cb(void *arg)
{
    (void)arg;
    motor_control_tick();
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

static float read_bus_current_amps(void)
{
    const float ia = fabsf(ina240_read_amps(INA240_PHASE_A));
    const float ib = fabsf(ina240_read_amps(INA240_PHASE_B));
    const float ic = fabsf(ina240_read_amps(INA240_PHASE_C));

    float max_phase = ia;

    if (ib > max_phase) {
        max_phase = ib;
    }
    if (ic > max_phase) {
        max_phase = ic;
    }

    return max_phase;
}

static void apply_commutation_step(float duty_percent)
{
    const commutation_pattern_t pattern = s_commutation_table[s_comm_step];

    hal_pwm_set_phase_conduction(HAL_PWM_PHASE_A, pattern.phase_a, duty_percent);
    hal_pwm_set_phase_conduction(HAL_PWM_PHASE_B, pattern.phase_b, duty_percent);
    hal_pwm_set_phase_conduction(HAL_PWM_PHASE_C, pattern.phase_c, duty_percent);
}

static void advance_commutation_step(void)
{
    const uint64_t now_us = esp_timer_get_time();

    if (s_last_comm_us != 0U) {
        const uint32_t measured = (uint32_t)(now_us - s_last_comm_us);

        s_step_period_us = (s_step_period_us * 7U + measured) / 8U;
    }

    s_last_comm_us = now_us;
    s_comm_step = (uint8_t)((s_comm_step + 1U) % COMMUTATION_STEP_COUNT);
}

static void advance_open_loop_if_due(void)
{
    const float comm_hz = DEFAULT_COMMUTATION_HZ;
    const uint32_t ticks_per_step =
        (uint32_t)((MOTOR_CONTROL_LOOP_HZ / comm_hz) + 0.5f);

    if (ticks_per_step < 1U) {
        return;
    }

    s_open_loop_tick_counter++;

    if (s_open_loop_tick_counter < ticks_per_step) {
        return;
    }

    s_open_loop_tick_counter = 0U;
    advance_commutation_step();
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

static void run_open_loop_commutation(void)
{
    advance_open_loop_if_due();

    if (zcd_handover_enabled()) {
        try_zcd_handover();
    }
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

bool motor_control_init(void)
{
    s_current_pi = (pi_controller_t){
        .kp = 8.0f,
        .ki = 120.0f,
        .dt = s_control_dt_s,
        .integral_term = 0.0f,
        .out_min = 0.0f,
        .out_max = MAX_DUTY_CYCLE_PERCENT,
        .integ_min = -40.0f,
        .integ_max = 40.0f,
    };

    s_target_amps = 0.0f;
    s_measured_amps = 0.0f;
    s_duty_percent = 0.0f;
    s_comm_step = 0U;
    s_open_loop_tick_counter = 0U;
    s_comm_mode = MOTOR_COMM_OPEN_LOOP;
    s_zcd_handover_count = 0U;
    s_step_period_us = DEFAULT_STEP_PERIOD_US;
    s_last_comm_us = 0U;
    s_comm_deadline_us = 0U;
    s_active = false;

    if (s_control_timer != NULL) {
        esp_timer_stop(s_control_timer);
        esp_timer_delete(s_control_timer);
        s_control_timer = NULL;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = control_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "motor_ctrl",
    };

    if (esp_timer_create(&timer_args, &s_control_timer) != ESP_OK) {
        return false;
    }

    const uint64_t period_us = (uint64_t)(1000000.0f / MOTOR_CONTROL_LOOP_HZ);

    if (esp_timer_start_periodic(s_control_timer, period_us) != ESP_OK) {
        esp_timer_delete(s_control_timer);
        s_control_timer = NULL;
        return false;
    }

    return true;
}

void motor_control_on_arm(void)
{
    s_current_pi.integral_term = 0.0f;
    s_comm_step = 0U;
    s_open_loop_tick_counter = 0U;
    s_comm_mode = MOTOR_COMM_OPEN_LOOP;
    s_zcd_handover_count = 0U;
    s_step_period_us = DEFAULT_STEP_PERIOD_US;
    s_last_comm_us = 0U;
    s_comm_deadline_us = 0U;
    s_duty_percent = 0.0f;
    s_active = true;
    apply_commutation_step(0.0f);
}

void motor_control_on_disarm(void)
{
    s_active = false;
    s_target_amps = 0.0f;
    s_duty_percent = 0.0f;
    s_comm_deadline_us = 0U;
    hal_pwm_disable_all();
}

void motor_control_force_open_loop(void)
{
    s_comm_mode = MOTOR_COMM_OPEN_LOOP;
    s_zcd_handover_count = 0U;
    s_open_loop_tick_counter = 0U;
    s_comm_deadline_us = 0U;
}

void motor_control_tick(void)
{
    if (!s_active) {
        return;
    }

    s_measured_amps = read_bus_current_amps();

    if (s_target_amps <= 0.0f) {
        s_duty_percent = 0.0f;
        s_current_pi.integral_term = 0.0f;
        s_comm_mode = MOTOR_COMM_OPEN_LOOP;
        s_zcd_handover_count = 0U;
        s_comm_deadline_us = 0U;
    } else {
        s_duty_percent =
            pi_compute(&s_current_pi, s_target_amps, s_measured_amps);
    }

    if (s_comm_mode == MOTOR_COMM_ZCD_CLOSED && bemf_zcd_is_ready()) {
        run_zcd_closed_commutation();
    } else {
        s_comm_mode = MOTOR_COMM_OPEN_LOOP;
        run_open_loop_commutation();
    }

    apply_commutation_step(s_duty_percent);
}

void motor_control_set_target_amps(float amps)
{
    s_target_amps = clamp_target_amps(amps);
}

float motor_control_get_target_amps(void)
{
    return s_target_amps;
}

float motor_control_get_measured_amps(void)
{
    return s_measured_amps;
}

float motor_control_get_duty_percent(void)
{
    return s_duty_percent;
}

uint8_t motor_control_get_commutation_step(void)
{
    return s_comm_step;
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
