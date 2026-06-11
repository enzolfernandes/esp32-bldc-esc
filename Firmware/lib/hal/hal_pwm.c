/*
 * hal_pwm.c — MCPWM: seis saídas complementares para inversor trifásico (IR2110).
 *
 * Camada: HAL. Chamado por motor_control (comutação 6-step) e fsm_system (arm/disarm).
 * Frequência 20 kHz, dead-time 500 ns. Modos por fase: OFF, SOURCE (PWM high-side), SINK (low-side ON).
 */

#include "hal_pwm.h"

#include "board_config.h"

#include "driver/mcpwm.h"

#include <stddef.h>

static bool s_armed = false;
static hal_pwm_conduction_t s_phase_mode[HAL_PWM_PHASE_COUNT];

static const mcpwm_io_signals_t s_phase_signals[HAL_PWM_PHASE_COUNT][2] = {
    {MCPWM0A, MCPWM0B},
    {MCPWM1A, MCPWM1B},
    {MCPWM2A, MCPWM2B},
};

static const mcpwm_timer_t s_phase_timers[HAL_PWM_PHASE_COUNT] = {
    MCPWM_TIMER_0,
    MCPWM_TIMER_1,
    MCPWM_TIMER_2,
};

static const int s_phase_pins[HAL_PWM_PHASE_COUNT][2] = {
    {PIN_PWM_AH, PIN_PWM_AL},
    {PIN_PWM_BH, PIN_PWM_BL},
    {PIN_PWM_CH, PIN_PWM_CL},
};

/** Converte DEAD_TIME_NS em ticks do MCPWM (passo de 100 ns no driver ESP-IDF). */
static uint32_t dead_time_ticks(void)
{
    uint32_t ticks = DEAD_TIME_NS / 100U;

    return (ticks == 0U) ? 1U : ticks;
}

/** Limita duty ao teto de 95 % (margem para bootstrap do IR2110). */
static float clamp_duty(float duty_percent)
{
    if (duty_percent < 0.0f) {
        return 0.0f;
    }
    if (duty_percent > MAX_DUTY_CYCLE_PERCENT) {
        return MAX_DUTY_CYCLE_PERCENT;
    }

    return duty_percent;
}

static void configure_phase_timer(mcpwm_timer_t timer)
{
    mcpwm_config_t pwm_config = {
        .frequency = PWM_FREQUENCY_HZ,
        .cmpr_a = 0.0f,
        .cmpr_b = 0.0f,
        .counter_mode = MCPWM_UP_COUNTER,
        .duty_mode = MCPWM_DUTY_MODE_0,
    };

    mcpwm_init(MCPWM_UNIT_0, timer, &pwm_config);
    mcpwm_set_duty_type(MCPWM_UNIT_0, timer, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty(MCPWM_UNIT_0, timer, MCPWM_OPR_A, 0.0f);
}

/**
 * @brief Inicializa três timers MCPWM (fases A/B/C) com dead-time complementar.
 * PWM permanece desarmado até hal_pwm_set_armed(true).
 */
bool hal_pwm_init(void)
{
    const uint32_t dead_ticks = dead_time_ticks();

    for (int phase = 0; phase < HAL_PWM_PHASE_COUNT; phase++) {
        esp_err_t err_a = mcpwm_gpio_init(MCPWM_UNIT_0, s_phase_signals[phase][0],
                                          s_phase_pins[phase][0]);
        esp_err_t err_b = mcpwm_gpio_init(MCPWM_UNIT_0, s_phase_signals[phase][1],
                                          s_phase_pins[phase][1]);

        if (err_a != ESP_OK || err_b != ESP_OK) {
            return false;
        }

        configure_phase_timer(s_phase_timers[phase]);

        mcpwm_deadtime_enable(MCPWM_UNIT_0, s_phase_timers[phase],
                              MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, dead_ticks, dead_ticks);
    }

    s_armed = false;

    for (int phase = 0; phase < HAL_PWM_PHASE_COUNT; phase++) {
        s_phase_mode[phase] = HAL_PWM_COND_OFF;
    }

    return true;
}

/** Autoriza saída PWM; false força todas as fases em OFF. */
void hal_pwm_set_armed(bool armed)
{
    s_armed = armed;

    if (!armed) {
        hal_pwm_disable_all();
    }
}

bool hal_pwm_is_armed(void)
{
    return s_armed;
}

/** Ambas pernas em nível baixo — fase flutuante (passo 6-step com C OFF, etc.). */
static void set_phase_off(hal_pwm_phase_t phase)
{
    const mcpwm_timer_t timer = s_phase_timers[phase];

    mcpwm_set_signal_low(MCPWM_UNIT_0, timer, MCPWM_OPR_A);
    mcpwm_set_signal_low(MCPWM_UNIT_0, timer, MCPWM_OPR_B);
    s_phase_mode[phase] = HAL_PWM_COND_OFF;
}

/** PWM na perna high-side (SOURCE); low-side complementar com dead-time. */
static void set_phase_source(hal_pwm_phase_t phase, float duty_percent)
{
    const mcpwm_timer_t timer = s_phase_timers[phase];

    mcpwm_set_duty_type(MCPWM_UNIT_0, timer, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty(MCPWM_UNIT_0, timer, MCPWM_OPR_A, clamp_duty(duty_percent));
    s_phase_mode[phase] = HAL_PWM_COND_SOURCE;
}

/** Low-side condutora contínua (SINK); high-side desligada. */
static void set_phase_sink(hal_pwm_phase_t phase)
{
    const mcpwm_timer_t timer = s_phase_timers[phase];

    mcpwm_set_duty_type(MCPWM_UNIT_0, timer, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty(MCPWM_UNIT_0, timer, MCPWM_OPR_A, 0.0f);
    s_phase_mode[phase] = HAL_PWM_COND_SINK;
}

void hal_pwm_set_phase_duty(hal_pwm_phase_t phase, float duty_percent)
{
    hal_pwm_set_phase_conduction(phase, HAL_PWM_COND_SOURCE, duty_percent);
}

/**
 * @brief Define modo de condução e duty de uma fase (mapeamento da tabela 6-step).
 * Se não armado, força OFF independente do modo solicitado (segurança).
 */
void hal_pwm_set_phase_conduction(hal_pwm_phase_t phase, hal_pwm_conduction_t mode,
                                  float duty_percent)
{
    if (phase >= HAL_PWM_PHASE_COUNT) {
        return;
    }

    if (!s_armed) {
        mode = HAL_PWM_COND_OFF;
        duty_percent = 0.0f;
    }

    switch (mode) {
    case HAL_PWM_COND_SOURCE:
        set_phase_source(phase, duty_percent);
        break;
    case HAL_PWM_COND_SINK:
        set_phase_sink(phase);
        break;
    case HAL_PWM_COND_OFF:
    default:
        set_phase_off(phase);
        break;
    }
}

/** Desliga todas as fases — usado em falha e disarm. */
void hal_pwm_disable_all(void)
{
    for (int phase = 0; phase < HAL_PWM_PHASE_COUNT; phase++) {
        set_phase_off((hal_pwm_phase_t)phase);
    }
}
