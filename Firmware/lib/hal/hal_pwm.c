/*
 * hal_pwm.c — MCPWM: seis saídas complementares para inversor trifásico (IR2110).
 *
 * Camada: HAL. Chamado por motor_control (comutação 6-step) e fsm_system (arm/disarm).
 * Frequência 20 kHz. Dead-time no IR2110 (~520 ns); MCPWM sem complement (HO/LO independentes).
 * Modos por fase: OFF, SOURCE (PWM high-side), SINK (low-side ON).
 *
 * Boot: pinos permanecem GPIO LOW até hal_pwm_set_armed(true) (attach MCPWM só no arm).
 */

#include "hal_pwm.h"

#include "board_config.h"

#include "driver/gpio.h"
#include "driver/mcpwm.h"

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
static esp_err_t configure_phase_timer(mcpwm_timer_t timer)
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

    /*
     * Complement MCPWM acopla HO/LO — inviável para 6-step (HO PWM + LO fixo em fases distintas).
     * Dead-time fica a cargo do IR2110 (datasheet ~520 ns).
     */
    mcpwm_deadtime_disable(MCPWM_UNIT_0, timer);

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

        esp_err_t err_a = mcpwm_gpio_init(MCPWM_UNIT_0, s_phase_signals[phase][0],
                                          s_phase_pins[phase][0]);
        if (err_a != ESP_OK) {
            return false;
        }

        force_phase_outputs_low(timer);

        esp_err_t err_b = mcpwm_gpio_init(MCPWM_UNIT_0, s_phase_signals[phase][1],
                                          s_phase_pins[phase][1]);
        if (err_b != ESP_OK) {
            return false;
        }

        force_phase_outputs_low(timer);
    }

    for (int phase = 0; phase < HAL_PWM_PHASE_COUNT; phase++) {
        const esp_err_t start_err =
            mcpwm_start(MCPWM_UNIT_0, s_phase_timers[phase]);

        if (start_err != ESP_OK) {
            printf("[HAL] MCPWM start timer %d FALHOU err=%d\n", phase,
                   (int)start_err);
            return false;
        }
    }

    s_gpio_attached = true;

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
}

/**
 * @brief Inicializa timers MCPWM internamente; pinos permanecem GPIO LOW até o arm.
 */
bool hal_pwm_init(void)
{
    if (!hal_pwm_hold_pins_low()) {
        return false;
    }

    for (int phase = 0; phase < HAL_PWM_PHASE_COUNT; phase++) {
        if (configure_phase_timer(s_phase_timers[phase]) != ESP_OK) {
            return false;
        }
    }

    s_gpio_attached = false;
    s_armed = false;

    for (int phase = 0; phase < HAL_PWM_PHASE_COUNT; phase++) {
        s_phase_mode[phase] = HAL_PWM_COND_OFF;
    }

    return true;
}

/** Autoriza saída PWM; true faz attach MCPWM→GPIO (SD por fase via hal_motor). */
bool hal_pwm_set_armed(bool armed)
{
    if (armed) {
        if (!attach_pwm_gpios()) {
            s_armed = false;
            printf("[HAL] MCPWM attach FALHOU — PWM inativo\n");
            return false;
        }
        s_armed = true;
        printf("[HAL] MCPWM attach OK  freq=%u Hz\n", (unsigned)PWM_FREQUENCY_HZ);
        return true;
    }

    s_armed = false;
    hal_pwm_disable_all();
    detach_pwm_gpios();
    return true;
}

bool hal_pwm_is_armed(void)
{
    return s_armed;
}

/** Força gerador em nível baixo (fase OFF ou perna inativa). */
static void force_generator_off(mcpwm_timer_t timer, mcpwm_generator_t gen)
{
    mcpwm_set_signal_low(MCPWM_UNIT_0, timer, gen);
}

/**
 * PWM no gerador: duty_type antes de duty libera force de set_signal_low/high.
 * Ref.: ESP-IDF legacy MCPWM — ordem invertida deixa o gerador preso em LOW.
 */
static void set_generator_pwm(mcpwm_timer_t timer, mcpwm_generator_t gen, float duty_percent)
{
    const float duty = clamp_duty(duty_percent);

    mcpwm_set_duty_type(MCPWM_UNIT_0, timer, gen, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty(MCPWM_UNIT_0, timer, gen, duty);
}

/** Ambas pernas em nível baixo — fase flutuante (passo 6-step com C OFF, etc.). */
static void set_phase_off(hal_pwm_phase_t phase)
{
    if (s_gpio_attached) {
        const mcpwm_timer_t timer = s_phase_timers[phase];

        force_generator_off(timer, MCPWM_GEN_A);
        force_generator_off(timer, MCPWM_GEN_B);
    }

    s_phase_mode[phase] = HAL_PWM_COND_OFF;
}

/** PWM na perna high-side (SOURCE); low-side desligada. */
static void set_phase_source(hal_pwm_phase_t phase, float duty_percent)
{
    if (!s_gpio_attached) {
        s_phase_mode[phase] = HAL_PWM_COND_SOURCE;
        return;
    }

    const mcpwm_timer_t timer = s_phase_timers[phase];

    force_generator_off(timer, MCPWM_GEN_B);
    set_generator_pwm(timer, MCPWM_GEN_A, duty_percent);
    s_phase_mode[phase] = HAL_PWM_COND_SOURCE;
}

/** Low-side condutora contínua (SINK): HO off, LO nível alto fixo (não duty 100 %). */
static void set_phase_sink(hal_pwm_phase_t phase)
{
    if (!s_gpio_attached) {
        s_phase_mode[phase] = HAL_PWM_COND_SINK;
        return;
    }

    const mcpwm_timer_t timer = s_phase_timers[phase];

    force_generator_off(timer, MCPWM_GEN_A);
    mcpwm_set_signal_high(MCPWM_UNIT_0, timer, MCPWM_GEN_B);
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
