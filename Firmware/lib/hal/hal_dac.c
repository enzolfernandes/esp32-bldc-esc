/*
 * hal_dac.c — DAC1 (GPIO 25): tensão de referência Vdac para comparadores OCP LM339.
 *
 * Camada: HAL. Chamado por lm339_protection_init para programar limiar de corrente hardware.
 * Equação (shunt 1 mΩ, ganho INA240 20 V/V, offset 1,65 V):
 *   Vdac = 1,65 + I_limit × 0,001 × 20  [V]
 */

#include "hal_dac.h"

#include "board_config.h"

#include "driver/dac.h"

#include <stddef.h>

#define DAC_FULL_SCALE_V  3.3f
#define DAC_MAX_RAW       255U

static float s_output_volts = 0.0f;
static bool s_initialized = false;

static uint8_t volts_to_dac_raw(float volts)
{
    if (volts <= 0.0f) {
        return 0U;
    }
    if (volts >= DAC_FULL_SCALE_V) {
        return (uint8_t)DAC_MAX_RAW;
    }

    return (uint8_t)((volts / DAC_FULL_SCALE_V) * (float)DAC_MAX_RAW);
}

/** Habilita DAC1 no GPIO 25 e zera saída. */
bool hal_dac_init(void)
{
#if PIN_VDAC_REF == 25
    const dac_channel_t channel = DAC_CHANNEL_1;
#else
#error "PIN_VDAC_REF must be GPIO25 (DAC1) for this PCB layout"
#endif

    if (dac_output_enable(channel) != ESP_OK) {
        return false;
    }

    s_initialized = true;
    return hal_dac_set_voltage(0.0f);
}

/** Programa DAC1 com valor bruto 0–255. */
bool hal_dac_set_raw(uint8_t raw)
{
#if PIN_VDAC_REF == 25
    const dac_channel_t channel = DAC_CHANNEL_1;
#else
#error "PIN_VDAC_REF must be GPIO25 (DAC1) for this PCB layout"
#endif

    if (!s_initialized) {
        return false;
    }

    if (dac_output_voltage(channel, raw) != ESP_OK) {
        return false;
    }

    s_output_volts = ((float)raw / (float)DAC_MAX_RAW) * DAC_FULL_SCALE_V;
    return true;
}

/** Programa tensão de referência dos comparadores LM339 (limiar OCP ajustável). */
bool hal_dac_set_voltage(float volts)
{
#if PIN_VDAC_REF == 25
    const dac_channel_t channel = DAC_CHANNEL_1;
#else
#error "PIN_VDAC_REF must be GPIO25 (DAC1) for this PCB layout"
#endif

    if (!s_initialized) {
        return false;
    }

    if (volts < 0.0f) {
        volts = 0.0f;
    }
    if (volts > DAC_FULL_SCALE_V) {
        volts = DAC_FULL_SCALE_V;
    }

    const uint8_t raw = volts_to_dac_raw(volts);

    if (dac_output_voltage(channel, raw) != ESP_OK) {
        return false;
    }

    s_output_volts = volts;
    return true;
}

float hal_dac_get_voltage(void)
{
    return s_output_volts;
}
