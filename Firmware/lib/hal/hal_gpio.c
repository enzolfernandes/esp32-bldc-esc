/*
 * hal_gpio.c — GPIO: shutdown dos IR2110 e interrupção OC Trip (LM339).
 *
 * Camada: HAL. Chamado por fsm_system (init, arm/disarm) e lm339_protection (ISR OCP).
 * A ISR de sobrecorrente deve ser mínima: apenas sinaliza callback registrado pela FSM.
 */

#include "hal_gpio.h"

#include "board_config.h"

#include "driver/gpio.h"
#include "esp_err.h"

#include <stdio.h>
#include <stddef.h>

static hal_gpio_isr_cb_t s_oc_trip_cb = NULL;
static void *s_oc_trip_arg = NULL;
static bool s_isr_attached = false;

static const int s_shutdown_pins[] = {
    PIN_SD_A,
    PIN_SD_B,
    PIN_SD_C,
};

/**
 * @brief Aplica nível HIGH/LOW nos três pinos SD dos IR2110.
 * @param enabled true = HIGH (drivers habilitados); false = LOW (shutdown, fail-safe).
 */
static void apply_shutdown_level(bool enabled)
{
    const int level = enabled ? 1 : 0;

    for (size_t i = 0; i < (sizeof(s_shutdown_pins) / sizeof(s_shutdown_pins[0])); i++) {
        gpio_set_level(s_shutdown_pins[i], level);
    }
}

/**
 * @brief ISR de sobrecorrente — executada na borda de descida do OC Trip (ativo baixo).
 * IRAM_ATTR: código na RAM interna para resposta em microssegundos.
 * Apenas repassa ao callback; desarme de PWM fica no handler registrado (fsm_system).
 */
static void IRAM_ATTR oc_trip_isr_handler(void *arg)
{
    (void)arg;

    if (s_oc_trip_cb != NULL) {
        s_oc_trip_cb(s_oc_trip_arg);
    }
}

/**
 * @brief Configura GPIOs de saída (SD) e entrada (OC Trip).
 * SD inicia em LOW (shutdown) por segurança no boot.
 */
bool hal_gpio_init(void)
{
    uint64_t output_mask = 0U;

    for (size_t i = 0; i < (sizeof(s_shutdown_pins) / sizeof(s_shutdown_pins[0])); i++) {
        output_mask |= (1ULL << s_shutdown_pins[i]);
    }

    gpio_config_t output_conf = {
        .pin_bit_mask = output_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (gpio_config(&output_conf) != ESP_OK) {
        return false;
    }

    // Fail-safe: drivers desligados até arm explícito
    apply_shutdown_level(false);

    gpio_config_t input_conf = {
        .pin_bit_mask = (1ULL << PIN_OC_TRIP),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&input_conf) == ESP_OK;
}

/** Habilita ou desabilita os drivers IR2110 via pinos SD. */
void hal_shutdown_set_enabled(bool enabled)
{
    apply_shutdown_level(enabled);
}

/**
 * @brief Registra ISR na borda de descida do pino OC Trip (GPIO 26).
 * Chamado após lm339_protection_init, no final da sequência de boot.
 */
bool hal_gpio_attach_oc_trip_isr(hal_gpio_isr_cb_t cb, void *arg)
{
    esp_err_t err;

    if (cb == NULL) {
        return false;
    }

    // #region agent log
    printf("{\"sessionId\":\"5f7e08\",\"runId\":\"deferred-v2\",\"hypothesisId\":\"I\","
           "\"location\":\"hal_gpio.c:attach\",\"message\":\"oc-trip-before-arm\","
           "\"data\":{\"OC_TRIP\":%d},\"timestamp\":0}\n", gpio_get_level(PIN_OC_TRIP));
    // #endregion

    err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return false;
    }

    if (s_isr_attached) {
        gpio_isr_handler_remove(PIN_OC_TRIP);
        s_isr_attached = false;
    }

    s_oc_trip_cb = cb;
    s_oc_trip_arg = arg;

    err = gpio_set_intr_type(PIN_OC_TRIP, GPIO_INTR_NEGEDGE);
    if (err != ESP_OK) {
        return false;
    }

    err = gpio_isr_handler_add(PIN_OC_TRIP, oc_trip_isr_handler, NULL);
    if (err != ESP_OK) {
        return false;
    }

    s_isr_attached = true;

    // #region agent log
    printf("{\"sessionId\":\"5f7e08\",\"runId\":\"deferred-v2\",\"hypothesisId\":\"I\","
           "\"location\":\"hal_gpio.c:attach\",\"message\":\"oc-trip-armed\","
           "\"data\":{\"OC_TRIP\":%d},\"timestamp\":0}\n", gpio_get_level(PIN_OC_TRIP));
    // #endregion

    return true;
}

void hal_gpio_detach_oc_trip_isr(void)
{
    if (s_isr_attached) {
        gpio_isr_handler_remove(PIN_OC_TRIP);
        s_isr_attached = false;
    }

    gpio_set_intr_type(PIN_OC_TRIP, GPIO_INTR_DISABLE);
    s_oc_trip_cb = NULL;
    s_oc_trip_arg = NULL;
}

/** Retorna true se OC Trip está em nível baixo (comparador LM339 disparou). */
bool hal_gpio_oc_trip_asserted(void)
{
    return gpio_get_level(PIN_OC_TRIP) == 0;
}
