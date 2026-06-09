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

/** Período inicial estimado entre comutações (µs) — deriva de MOTOR_OPEN_LOOP_COMM_HZ_START. */
#define DEFAULT_STEP_PERIOD_US \
    ((uint32_t)(1000000.0f / (6.0f * MOTOR_OPEN_LOOP_COMM_HZ_START)))

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
static pi_controller_t s_speed_pi;
static float s_target_amps_cmd = 0.0f;
static float s_target_amps = 0.0f;
static float s_target_rpm_cmd = 0.0f;
static float s_target_rpm = 0.0f;
static float s_measured_rpm = 0.0f;
static float s_target_slew_rpm_per_s = MOTOR_SPEED_SLEW_RPM_PER_S;
static float s_measured_amps = 0.0f;
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
static motor_start_phase_t s_start_phase = MOTOR_START_IDLE;
static int8_t s_comm_direction = 1;
static volatile bool s_sw_fault_pending = false;
static motor_fault_reason_t s_last_fault_reason = MOTOR_FAULT_NONE;
static uint64_t s_stall_begin_us = 0U;
static uint64_t s_low_rpm_stall_begin_us = 0U;
static uint64_t s_last_step_change_us = 0U;
static uint64_t s_handover_begin_us = 0U;
static uint64_t s_desync_begin_us = 0U;
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

static void begin_align_sequence(void)
{
    s_start_phase = MOTOR_START_ALIGN;
    s_comm_step = align_step_for_direction();
    s_comm_mode = MOTOR_COMM_OPEN_LOOP;
    s_zcd_handover_count = 0U;
    s_comm_deadline_us = 0U;
    s_last_comm_us = 0U;
    reset_open_loop_ramp();
    s_align_end_us =
        esp_timer_get_time() + (uint64_t)s_align_duration_ms * 1000ULL;
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

static bool run_align_phase(void)
{
    const uint64_t now_us = esp_timer_get_time();

    s_duty_percent = s_align_duty_percent;
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

static void advance_open_loop_if_due(bool ramp_step)
{
    const uint32_t ticks_per_step =
        (uint32_t)((MOTOR_CONTROL_LOOP_HZ / s_open_loop_comm_hz) + 0.5f);

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
        ramp_open_loop_comm_hz();
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

bool motor_control_init(void)
{
    init_bench_params();

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
    s_last_fault_reason = MOTOR_FAULT_NONE;
    s_stall_begin_us = 0U;
    s_last_step_change_us = 0U;
    s_handover_begin_us = 0U;
    s_desync_begin_us = 0U;
    s_low_rpm_stall_begin_us = 0U;
    s_active = true;
    apply_commutation_step(0.0f);
}

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
    s_low_rpm_stall_begin_us = 0U;
    hal_pwm_disable_all();
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

void motor_control_tick(void)
{
    if (!s_active) {
        return;
    }

    s_measured_amps = read_bus_current_amps();

    if (is_speed_control_mode()) {
        update_target_slew_rpm();
    } else {
        update_target_slew();
    }

    if (motor_control_torque_command_active() && trip_software_overcurrent()) {
        return;
    }

    if (motor_control_torque_command_active() && check_stall_conditions()) {
        return;
    }

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

    if (s_start_phase == MOTOR_START_IDLE) {
        begin_align_sequence();
    }

    if (s_start_phase == MOTOR_START_ALIGN) {
        if (run_align_phase()) {
            return;
        }
    }

    if (is_speed_control_mode() && s_start_phase == MOTOR_START_RUN_OPEN) {
        s_target_amps_cmd = MOTOR_SPEED_OPEN_LOOP_I_AMPS;
        update_target_slew();
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

    s_duty_percent = pi_compute(&s_current_pi, s_target_amps, s_measured_amps);

    if (s_comm_mode == MOTOR_COMM_ZCD_CLOSED && bemf_zcd_is_ready()) {
        run_zcd_closed_commutation();
    } else {
        s_comm_mode = MOTOR_COMM_OPEN_LOOP;
        const bool ramp_step =
            !is_speed_control_mode() ||
            s_start_phase == MOTOR_START_RUN_OPEN ||
            s_start_phase == MOTOR_START_RUN;

        run_open_loop_commutation(ramp_step);
    }

    apply_commutation_step(s_duty_percent);
}

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
