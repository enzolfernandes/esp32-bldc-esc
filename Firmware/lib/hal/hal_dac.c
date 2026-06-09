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
