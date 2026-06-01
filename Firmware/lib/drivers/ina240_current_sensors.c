#include "ina240_current_sensors.h"

#include "hal_adc.h"

#include <stddef.h>

// INA240A1DR: ganho 20 V/V, shunt 1 mOhm, offset nominal 1,65 V (REF1/REF2).
#define INA240_NOMINAL_OFFSET_MV 1650.0f
#define INA240_GAIN_V_PER_V      20.0f
#define INA240_SHUNT_OHMS        0.001f
#define INA240_MV_TO_AMPS_SCALE  (INA240_GAIN_V_PER_V * INA240_SHUNT_OHMS * 1000.0f)

static const hal_adc_channel_t s_adc_channels[INA240_PHASE_COUNT] = {
    HAL_ADC_PHASE_IA,
    HAL_ADC_PHASE_IB,
    HAL_ADC_PHASE_IC,
};

static float s_offset_mv[INA240_PHASE_COUNT] = {
    INA240_NOMINAL_OFFSET_MV,
    INA240_NOMINAL_OFFSET_MV,
    INA240_NOMINAL_OFFSET_MV,
};

static bool s_initialized = false;

bool ina240_init(void)
{
    s_initialized = true;
    return true;
}

bool ina240_calibrate_offset(uint16_t sample_count)
{
    uint32_t sums[INA240_PHASE_COUNT] = {0U, 0U, 0U};

    if (!s_initialized || sample_count == 0U) {
        return false;
    }

    for (uint16_t sample = 0; sample < sample_count; sample++) {
        for (int phase = 0; phase < INA240_PHASE_COUNT; phase++) {
            sums[phase] += hal_adc_read_mv(s_adc_channels[phase]);
        }
    }

    for (int phase = 0; phase < INA240_PHASE_COUNT; phase++) {
        s_offset_mv[phase] = (float)sums[phase] / (float)sample_count;
    }

    return true;
}

float ina240_get_offset_mv(ina240_phase_t phase)
{
    if (phase >= INA240_PHASE_COUNT) {
        return INA240_NOMINAL_OFFSET_MV;
    }

    return s_offset_mv[phase];
}

float ina240_read_amps(ina240_phase_t phase)
{
    uint32_t mv;

    if (!s_initialized || phase >= INA240_PHASE_COUNT) {
        return 0.0f;
    }

    mv = hal_adc_read_mv(s_adc_channels[phase]);
    return ((float)mv - s_offset_mv[phase]) / INA240_MV_TO_AMPS_SCALE;
}
