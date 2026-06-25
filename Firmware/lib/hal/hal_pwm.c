/*
 * hal_pwm.c — MCPWM: seis saídas complementares para inversor trifásico (IR2110).
 *
 * Camada: HAL. Chamado por motor_control (comutação 6-step) e fsm_system (arm/disarm).
 * Frequência 20 kHz, dead-time 500 ns. Modos por fase: OFF, SOURCE (PWM high-side), SINK (low-side ON).
 *
 * Boot: pinos permanecem GPIO LOW até hal_pwm_set_armed(true) (attach MCPWM só no arm).
 */

#include "hal_pwm.h"

#include "board_config.h"

#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "esp_timer.h"

#include <stdio.h>
#include <stddef.h>

static bool s_armed = false;
static bool s_gpio_attached = false;
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

static const int s_all_pwm_pins[] = {
    PIN_PWM_AH,
    PIN_PWM_AL,
    PIN_PWM_BH,
    PIN_PWM_BL,
    PIN_PWM_CH,
    PIN_PWM_CL,
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

// #region agent log
static void agent_log_boot_state(const char *step, const char *hypothesis_id, int phase)
{
    printf("{\"sessionId\":\"5f7e08\",\"runId\":\"deferred-v2\",\"hypothesisId\":\"%s\","
           "\"location\":\"hal_pwm.c\",\"message\":\"%s\",\"data\":{\"phase\":%d,"
           "\"AH\":%d,\"AL\":%d,\"BH\":%d,\"BL\":%d,\"CH\":%d,\"CL\":%d,"
           "\"SD_A\":%d,\"SD_B\":%d,\"SD_C\":%d,\"gpio_attached\":%d,\"armed\":%d},"
           "\"timestamp\":%lld}\n",
           hypothesis_id, step, phase,
           gpio_get_level(PIN_PWM_AH), gpio_get_level(PIN_PWM_AL),
           gpio_get_level(PIN_PWM_BH), gpio_get_level(PIN_PWM_BL),
           gpio_get_level(PIN_PWM_CH), gpio_get_level(PIN_PWM_CL),
           gpio_get_level(PIN_SD_A), gpio_get_level(PIN_SD_B), gpio_get_level(PIN_SD_C),
           s_gpio_attached ? 1 : 0, s_armed ? 1 : 0,
           (long long)(esp_timer_get_time() / 1000LL));
}
// #endregion

/** Força os seis pinos MCPWM como saída GPIO digital LOW (sem mux para MCPWM). */
bool hal_pwm_hold_pins_low(void)
{
    uint64_t mask = 0U;

    for (size_t i = 0; i < (sizeof(s_all_pwm_pins) / sizeof(s_all_pwm_pins[0])); i++) {
        mask |= (1ULL << s_all_pwm_pins[i]);
    }

    const gpio_config_t output_conf = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (gpio_config(&output_conf) != ESP_OK) {
        return false;
    }

    for (size_t i = 0; i < (sizeof(s_all_pwm_pins) / sizeof(s_all_pwm_pins[0])); i++) {
        gpio_set_level(s_all_pwm_pins[i], 0);
    }

    return true;
}

/** Força ambos os geradores MCPWM em nível baixo (Active-High, sem inversão). */
static void force_phase_outputs_low(mcpwm_timer_t timer)
{
    mcpwm_set_signal_low(MCPWM_UNIT_0, timer, MCPWM_GEN_A);
    mcpwm_set_signal_low(MCPWM_UNIT_0, timer, MCPWM_GEN_B);
}

/**
 * @brief Configura timer, duty 0 % em A/B, dead-time e saídas LOW (sem attach GPIO).
 */
static esp_err_t configure_phase_timer(mcpwm_timer_t timer, uint32_t dead_ticks)
{
    const mcpwm_config_t pwm_config = {
        .frequency = PWM_FREQUENCY_HZ,
        .cmpr_a = 0.0f,
        .cmpr_b = 0.0f,
        .counter_mode = MCPWM_UP_COUNTER,
        .duty_mode = MCPWM_DUTY_MODE_0,
    };

    esp_err_t err = mcpwm_init(MCPWM_UNIT_0, timer, &pwm_config);
    if (err != ESP_OK) {
        return err;
    }

    mcpwm_set_duty_type(MCPWM_UNIT_0, timer, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(MCPWM_UNIT_0, timer, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty(MCPWM_UNIT_0, timer, MCPWM_OPR_A, 0.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, timer, MCPWM_OPR_B, 0.0f);

    err = mcpwm_deadtime_enable(MCPWM_UNIT_0, timer, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE,
                                dead_ticks, dead_ticks);
    if (err != ESP_OK) {
        return err;
    }

    force_phase_outputs_low(timer);

    return ESP_OK;
}

/** Conecta saídas MCPWM aos pinos físicos (somente com SD em shutdown / drivers desabilitados). */
static bool attach_pwm_gpios(void)
{
    if (s_gpio_attached) {
        return true;
    }

    for (int phase = 0; phase < HAL_PWM_PHASE_COUNT; phase++) {
        const mcpwm_timer_t timer = s_phase_timers[phase];

        force_phase_outputs_low(timer);

        // #region agent log
        agent_log_boot_state("before-mcpwm_gpio_init", "F", phase);
        // #endregion

        esp_err_t err_a = mcpwm_gpio_init(MCPWM_UNIT_0, s_phase_signals[phase][0],
                                          s_phase_pins[phase][0]);
        if (err_a != ESP_OK) {
            return false;
        }

        force_phase_outputs_low(timer);

        // #region agent log
        agent_log_boot_state("after-mcpwm_gpio_init-AH", "F", phase);
        // #endregion

        esp_err_t err_b = mcpwm_gpio_init(MCPWM_UNIT_0, s_phase_signals[phase][1],
                                          s_phase_pins[phase][1]);
        if (err_b != ESP_OK) {
            return false;
        }

        force_phase_outputs_low(timer);

        // #region agent log
        agent_log_boot_state("after-mcpwm_gpio_init-AL", "F", phase);
        // #endregion
    }

    s_gpio_attached = true;

    // #region agent log
    agent_log_boot_state("gpio-attach-complete", "F", -1);
    // #endregion

    return true;
}

/** Devolve pinos ao GPIO genérico em LOW (desconecta mux MCPWM). */
static void detach_pwm_gpios(void)
{
    if (!s_gpio_attached) {
        return;
    }

    for (int phase = 0; phase < HAL_PWM_PHASE_COUNT; phase++) {
        force_phase_outputs_low(s_phase_timers[phase]);
    }

    for (size_t i = 0; i < (sizeof(s_all_pwm_pins) / sizeof(s_all_pwm_pins[0])); i++) {
        gpio_reset_pin((gpio_num_t)s_all_pwm_pins[i]);
    }

    (void)hal_pwm_hold_pins_low();
    s_gpio_attached = false;

    // #region agent log
    agent_log_boot_state("gpio-detach-complete", "G", -1);
    // #endregion
}

/**
 * @brief Inicializa timers MCPWM internamente; pinos permanecem GPIO LOW até o arm.
 */
bool hal_pwm_init(void)
{
    const uint32_t dead_ticks = dead_time_ticks();

    // #region agent log
    agent_log_boot_state("hal_pwm_init-start", "G", -1);
    // #endregion

    if (!hal_pwm_hold_pins_low()) {
        return false;
    }

    for (int phase = 0; phase < HAL_PWM_PHASE_COUNT; phase++) {
        if (configure_phase_timer(s_phase_timers[phase], dead_ticks) != ESP_OK) {
            return false;
        }
    }

    s_gpio_attached = false;
    s_armed = false;

    for (int phase = 0; phase < HAL_PWM_PHASE_COUNT; phase++) {
        s_phase_mode[phase] = HAL_PWM_COND_OFF;
    }

    // #region agent log
    agent_log_boot_state("hal_pwm_init-complete-no-gpio-attach", "G", -1);
    // #endregion

    return true;
}

/** Autoriza saída PWM; true faz attach MCPWM→GPIO (drivers ainda via hal_shutdown). */
void hal_pwm_set_armed(bool armed)
{
    if (armed) {
        if (!attach_pwm_gpios()) {
            s_armed = false;
            return;
        }
        s_armed = true;

        // #region agent log
        agent_log_boot_state("hal_pwm_set_armed-true", "F", -1);
        // #endregion
        return;
    }

    s_armed = false;
    hal_pwm_disable_all();
    detach_pwm_gpios();

    // #region agent log
    agent_log_boot_state("hal_pwm_set_armed-false", "G", -1);
    // #endregion
}

bool hal_pwm_is_armed(void)
{
    return s_armed;
}

/** Ambas pernas em nível baixo — fase flutuante (passo 6-step com C OFF, etc.). */
static void set_phase_off(hal_pwm_phase_t phase)
{
    if (s_gpio_attached) {
        const mcpwm_timer_t timer = s_phase_timers[phase];

        mcpwm_set_signal_low(MCPWM_UNIT_0, timer, MCPWM_OPR_A);
        mcpwm_set_signal_low(MCPWM_UNIT_0, timer, MCPWM_OPR_B);
    }

    s_phase_mode[phase] = HAL_PWM_COND_OFF;
}

/** PWM na perna high-side (SOURCE); low-side complementar com dead-time. */
static void set_phase_source(hal_pwm_phase_t phase, float duty_percent)
{
    if (!s_gpio_attached) {
        s_phase_mode[phase] = HAL_PWM_COND_SOURCE;
        return;
    }

    const mcpwm_timer_t timer = s_phase_timers[phase];

    mcpwm_set_duty_type(MCPWM_UNIT_0, timer, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty(MCPWM_UNIT_0, timer, MCPWM_OPR_A, clamp_duty(duty_percent));
    s_phase_mode[phase] = HAL_PWM_COND_SOURCE;
}

/** Low-side condutora contínua (SINK); high-side desligada. */
static void set_phase_sink(hal_pwm_phase_t phase)
{
    if (!s_gpio_attached) {
        s_phase_mode[phase] = HAL_PWM_COND_SINK;
        return;
    }

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
