/*
 * hal_adc.c — ADC1: leitura de tensões em milivolts (corrente INA240, VBAT).
 *
 * Camada: HAL. Usa ADC1 (não ADC2) pois o Bluetooth do PS4 mantém o rádio ativo.
 * Conversão via esp_adc_cal (eFuse ou ADC_DEFAULT_VREF_MV); fallback linear se characterize falhar.
 */

#include "hal_adc.h"

#include "board_config.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/portmacro.h"

#include <stddef.h>

static const char *TAG = "hal_adc";

#define ADC_FULL_SCALE_MV 3300U
#define ADC_MAX_RAW       4095U

static const adc1_channel_t s_channels[HAL_ADC_CHANNEL_COUNT] = {
    ADC1_CHANNEL_6, // PIN_ADC_IA 34 — corrente fase A
    ADC1_CHANNEL_7, // PIN_ADC_IB 35 — corrente fase B
    ADC1_CHANNEL_0, // PIN_ADC_IC 36 — corrente fase C
    ADC1_CHANNEL_3, // PIN_ADC_VBAT 39 — tensão do barramento
};

static esp_adc_cal_characteristics_t s_adc_chars;
static esp_adc_cal_value_t s_cal_type = ESP_ADC_CAL_VAL_NOT_SUPPORTED;
static bool s_adc_initialized = false;
static bool s_cali_enabled = false;
static hal_adc_channel_t s_last_channel = HAL_ADC_CHANNEL_COUNT;
static portMUX_TYPE s_adc_mux = portMUX_INITIALIZER_UNLOCKED;

static const char *cal_type_name(esp_adc_cal_value_t cal_type)
{
    switch (cal_type) {
    case ESP_ADC_CAL_VAL_EFUSE_TP:
        return "EFUSE_TP";
    case ESP_ADC_CAL_VAL_EFUSE_VREF:
        return "EFUSE_VREF";
    case ESP_ADC_CAL_VAL_DEFAULT_VREF:
        return "DEFAULT_VREF";
    case ESP_ADC_CAL_VAL_NOT_SUPPORTED:
    default:
        return "NOT_SUPPORTED";
    }
}

/** Escala linear ideal (pré-calibração) — usada em fallback e diagnóstico. */
static uint32_t raw_to_mv_linear(int raw)
{
    if (raw < 0) {
        return 0U;
    }

    return (uint32_t)(((uint64_t)raw * ADC_FULL_SCALE_MV) / ADC_MAX_RAW);
}

/** Converte raw em mV com esp_adc_cal ou fallback linear. */
static uint32_t raw_to_mv(int raw)
{
    if (raw < 0) {
        return 0U;
    }

    if (s_cali_enabled) {
        return (uint32_t)esp_adc_cal_raw_to_voltage((uint32_t)raw, &s_adc_chars);
    }

    return raw_to_mv_linear(raw);
}

bool hal_adc_is_calibrated(void)
{
    return s_cali_enabled;
}

const char *hal_adc_cal_scheme_name(void)
{
    return cal_type_name(s_cal_type);
}

uint32_t hal_adc_raw_to_mv_linear(int raw)
{
    return raw_to_mv_linear(raw);
}

uint32_t hal_adc_raw_to_mv(int raw)
{
    return raw_to_mv(raw);
}

/** Configura resolução 12 bits, atenuação e esp_adc_cal (idempotente). */
bool hal_adc_init(void)
{
    esp_err_t err;

    if (s_adc_initialized) {
        return true;
    }

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

    s_cali_enabled = false;
    s_cal_type = esp_adc_cal_characterize(ADC_UNIT_1,
                                          ADC_ATTEN_DB_12,
                                          ADC_WIDTH_BIT_12,
                                          ADC_DEFAULT_VREF_MV,
                                          &s_adc_chars);

    if (s_cal_type != ESP_ADC_CAL_VAL_NOT_SUPPORTED) {
        s_cali_enabled = true;
        ESP_LOGI(TAG, "ADC1 calibrado (%s, default_vref=%u mV)",
                 cal_type_name(s_cal_type),
                 (unsigned)ADC_DEFAULT_VREF_MV);
    } else {
        ESP_LOGW(TAG, "esp_adc_cal characterize falhou — fallback linear");
    }

    s_adc_initialized = true;
    return true;
}

int hal_adc_read_raw(hal_adc_channel_t channel)
{
    int raw;

    if (channel >= HAL_ADC_CHANNEL_COUNT) {
        return -1;
    }

    portENTER_CRITICAL(&s_adc_mux);

    if (s_last_channel != channel) {
        (void)adc1_get_raw(s_channels[channel]);
        s_last_channel = channel;
    }

    raw = adc1_get_raw(s_channels[channel]);
    portEXIT_CRITICAL(&s_adc_mux);
    return raw;
}

uint32_t hal_adc_read_mv(hal_adc_channel_t channel)
{
    int raw;

    if (channel >= HAL_ADC_CHANNEL_COUNT) {
        return 0U;
    }

    raw = hal_adc_read_raw(channel);
    if (raw < 0) {
        return 0U;
    }

    return hal_adc_raw_to_mv(raw);
}
