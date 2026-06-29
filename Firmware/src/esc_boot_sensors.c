/*
 * esc_boot_sensors.c — Calibração INA240 no boot, antes de Wi-Fi e Bluetooth explícito.
 *
 * Camada: aplicação. Invocado em initVariant() (main.cpp) sem dependência de Serial.
 */

#include "esc_boot_sensors.h"

#include "board_config.h"
#include "hal_adc.h"
#include "hal_gpio.h"
#include "hal_pwm.h"
#include "ina240_current_sensors.h"

bool esc_boot_early_calibrate(void)
{
    if (!hal_gpio_init()) {
        return false;
    }

    if (!hal_pwm_hold_pins_low()) {
        return false;
    }

    if (!hal_adc_init()) {
        return false;
    }

    if (!ina240_init()) {
        return false;
    }

    return ina240_calibrate_offset(INA240_CALIBRATION_SAMPLES);
}
