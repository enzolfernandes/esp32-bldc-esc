#include "hal_adc.h"

#include "board_config.h"

#include "driver/adc.h"
#include "esp_err.h"

#include <stddef.h>

#define ADC_FULL_SCALE_MV 3300U
#define ADC_MAX_RAW       4095U

static const adc1_channel_t s_channels[HAL_ADC_CHANNEL_COUNT] = {
    ADC1_CHANNEL_6, // PIN_ADC_IA 34
    ADC1_CHANNEL_7, // PIN_ADC_IB 35
    ADC1_CHANNEL_0, // PIN_ADC_IC 36
    ADC1_CHANNEL_3, // PIN_ADC_VBAT 39
};

static uint32_t raw_to_mv(int raw)
{
    if (raw < 0) {
        return 0U;
    }

    return (uint32_t)(((uint64_t)raw * ADC_FULL_SCALE_MV) / ADC_MAX_RAW);
}

bool hal_adc_init(void)
{
    esp_err_t err;

    err = adc1_config_width(ADC_WIDTH_BIT_12);
    if (err != ESP_OK) {
        return false;
    }

    for (int i = 0; i < HAL_ADC_CHANNEL_COUNT; i++) {
        err = adc1_config_channel_atten(s_channels[i], ADC_ATTEN_DB_12);
        if (err != ESP_OK) {
            return false;
        }
    }

    return true;
}

uint32_t hal_adc_read_mv(hal_adc_channel_t channel)
{
    int raw;

    if (channel >= HAL_ADC_CHANNEL_COUNT) {
        return 0U;
    }

    raw = adc1_get_raw(s_channels[channel]);
    return raw_to_mv(raw);
}
