#include "hal_pwm.h"

#include "board_config.h"

#include "driver/mcpwm.h"

#include <stddef.h>

static bool s_armed = false;

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

static uint32_t dead_time_ticks(void)
{
    // MCPWM dead-time counter step = 100 ns (ESP-IDF driver).
    uint32_t ticks = DEAD_TIME_NS / 100U;

    return (ticks == 0U) ? 1U : ticks;
}

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
    return true;
}

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

void hal_pwm_set_phase_duty(hal_pwm_phase_t phase, float duty_percent)
{
    if (phase >= HAL_PWM_PHASE_COUNT) {
        return;
    }

    if (!s_armed) {
        duty_percent = 0.0f;
    }

    mcpwm_set_duty(MCPWM_UNIT_0, s_phase_timers[phase], MCPWM_OPR_A,
                   clamp_duty(duty_percent));
}

void hal_pwm_disable_all(void)
{
    for (int phase = 0; phase < HAL_PWM_PHASE_COUNT; phase++) {
        mcpwm_set_duty(MCPWM_UNIT_0, s_phase_timers[phase], MCPWM_OPR_A, 0.0f);
    }
}
