#include "bemf_zcd.h"

#include "board_config.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include <stddef.h>
#include <stdint.h>

#if BOARD_ENABLE_BEMF_ZCD

static bool s_ready = false;
static volatile uint8_t s_pending_edge_phase = 0xFFU;
static portMUX_TYPE s_zcd_spinlock = portMUX_INITIALIZER_UNLOCKED;

static const int s_zcd_pins[INA240_PHASE_COUNT] = {
    PIN_ZCD_A,
    PIN_ZCD_B,
    PIN_ZCD_C,
};

static const ina240_phase_t s_float_phase_for_step[6] = {
    INA240_PHASE_C,
    INA240_PHASE_B,
    INA240_PHASE_A,
    INA240_PHASE_C,
    INA240_PHASE_B,
    INA240_PHASE_A,
};

static void IRAM_ATTR zcd_isr_handler(void *arg)
{
    const uintptr_t phase = (uintptr_t)arg;

    if (phase < INA240_PHASE_COUNT) {
        s_pending_edge_phase = (uint8_t)phase;
    }
}

#endif // BOARD_ENABLE_BEMF_ZCD

bool bemf_zcd_init(void)
{
#if !BOARD_ENABLE_BEMF_ZCD
    return false;
#else
    const uint64_t pin_mask =
        (1ULL << PIN_ZCD_A) | (1ULL << PIN_ZCD_B) | (1ULL << PIN_ZCD_C);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (gpio_config(&io_conf) != ESP_OK) {
        s_ready = false;
        return false;
    }

    esp_err_t err = gpio_install_isr_service(0);

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        s_ready = false;
        return false;
    }

    for (ina240_phase_t phase = INA240_PHASE_A; phase < INA240_PHASE_COUNT; phase++) {
        const int pin = s_zcd_pins[phase];

        gpio_isr_handler_remove(pin);
        err = gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE);

        if (err != ESP_OK) {
            s_ready = false;
            return false;
        }

        err = gpio_isr_handler_add(pin, zcd_isr_handler, (void *)(uintptr_t)phase);

        if (err != ESP_OK) {
            s_ready = false;
            return false;
        }
    }

    s_pending_edge_phase = 0xFFU;
    s_ready = true;
    return true;
#endif
}

bool bemf_zcd_is_ready(void)
{
#if !BOARD_ENABLE_BEMF_ZCD
    return false;
#else
    return s_ready;
#endif
}

ina240_phase_t bemf_zcd_floating_phase_for_step(uint8_t comm_step)
{
#if !BOARD_ENABLE_BEMF_ZCD
    (void)comm_step;
    return INA240_PHASE_A;
#else
    return s_float_phase_for_step[comm_step % 6U];
#endif
}

bool bemf_zcd_consume_edge(ina240_phase_t expected_phase)
{
#if !BOARD_ENABLE_BEMF_ZCD
    (void)expected_phase;
    return false;
#else
    bool matched = false;
    uint8_t pending;

    if (!s_ready || expected_phase >= INA240_PHASE_COUNT) {
        return false;
    }

    portENTER_CRITICAL(&s_zcd_spinlock);
    pending = s_pending_edge_phase;

    if (pending == (uint8_t)expected_phase) {
        s_pending_edge_phase = 0xFFU;
        matched = true;
    }

    portEXIT_CRITICAL(&s_zcd_spinlock);
    return matched;
#endif
}

bool bemf_zcd_phase_asserted(ina240_phase_t phase)
{
#if !BOARD_ENABLE_BEMF_ZCD
    (void)phase;
    return false;
#else
    if (!s_ready || phase >= INA240_PHASE_COUNT) {
        return false;
    }

    return gpio_get_level(s_zcd_pins[phase]) == 0;
#endif
}
