/*
 * lm339_protection.c — OCP em hardware via LM339 + Vdac (DAC1).
 *
 * Camada: drivers. Programa limiar de corrente no DAC; ISR no OC Trip desarma PWM.
 * Resposta em microssegundos, independente da malha de 1 kHz.
 */

#include "lm339_protection.h"

#include "board_config.h"
#include "hal_dac.h"
#include "hal_gpio.h"
#include "hal_pwm.h"

#include <stddef.h>

#define INA240_NOMINAL_OFFSET_V  1.65f
#define INA240_GAIN_V_PER_V      20.0f
#define INA240_SHUNT_OHMS        0.001f

static lm339_fault_cb_t s_fault_cb = NULL;
static void *s_fault_arg = NULL;
static volatile bool s_fault_latched = false;
static bool s_initialized = false;

/** Converte limiar em ampères para tensão Vdac na entrada (+) dos comparadores LM339. */
static float amps_to_vdac(float amps)
{
    if (amps < 0.0f) {
        amps = 0.0f;
    }

    return INA240_NOMINAL_OFFSET_V +
           (amps * INA240_SHUNT_OHMS * INA240_GAIN_V_PER_V);
}

/**
 * @brief Handler de OCP — registrado como ISR no OC Trip (via hal_gpio).
 * Desarma shutdown e PWM imediatamente; sinaliza FSM por callback.
 */
static void oc_trip_handler(void *arg)
{
    (void)arg;

    s_fault_latched = true;
    hal_shutdown_set_enabled(false);
    hal_pwm_set_armed(false);
    hal_pwm_disable_all();

    if (s_fault_cb != NULL) {
        s_fault_cb(s_fault_arg);
    }
}

bool lm339_protection_init(void)
{
    if (!hal_dac_init()) {
        return false;
    }

    if (!lm339_protection_set_oc_threshold_amps(LM339_HW_OC_AMPS)) {
        return false;
    }

    s_initialized = true;
    s_fault_latched = false;
    return true;
}

bool lm339_protection_set_oc_threshold_amps(float amps)
{
    return hal_dac_set_voltage(amps_to_vdac(amps));
}

/** Habilita EXTI no GPIO 26 e associa callback de falha (fsm_system). */
bool lm339_protection_arm(lm339_fault_cb_t cb, void *arg)
{
    if (!s_initialized) {
        return false;
    }

    s_fault_cb = cb;
    s_fault_arg = arg;

    return hal_gpio_attach_oc_trip_isr(oc_trip_handler, NULL);
}

void lm339_protection_disarm(void)
{
    hal_gpio_detach_oc_trip_isr();
    s_fault_cb = NULL;
    s_fault_arg = NULL;
}

bool lm339_protection_fault_active(void)
{
    return s_fault_latched || hal_gpio_oc_trip_asserted();
}

/** Libera latch somente quando pino OC Trip retornou a HIGH (hardware OK). */
void lm339_protection_clear_fault(void)
{
    if (!hal_gpio_oc_trip_asserted()) {
        s_fault_latched = false;
    }
}
