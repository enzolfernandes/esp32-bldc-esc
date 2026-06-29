/*
 * ina240_current_sensors.c — Conversão mV (ADC) → ampères de fase.
 *
 * Camada: drivers. Amplificador INA240A1: ganho 20 V/V, shunt 1 mΩ, offset ~1,65 V.
 * Filtro EMA por fase (ALPHA_A=0,05 / ALPHA_BC=0,25). Mediana 8× só fase A.
 * Com INA240_USE_MANUAL_OFFSET: offset serial = multímetro; bench_corr preserva zero relativo.
 *
 * I [A] = (V_adc_mV - V_offset_mV) / (20 × 0,001 × 1000)
 */

#include "ina240_current_sensors.h"

#include "board_config.h"
#include "hal_adc.h"

#include <stdio.h>
#include <stddef.h>

#define INA240_GAIN_V_PER_V      20.0f
#define INA240_SHUNT_OHMS        0.001f
#define INA240_MV_TO_AMPS_SCALE  (INA240_GAIN_V_PER_V * INA240_SHUNT_OHMS * 1000.0f)

static const hal_adc_channel_t s_adc_channels[INA240_PHASE_COUNT] = {
    HAL_ADC_PHASE_IA,
    HAL_ADC_PHASE_IB,
    HAL_ADC_PHASE_IC,
};

static const char *s_phase_names[INA240_PHASE_COUNT] = {"A", "B", "C"};

static const float s_ema_alpha[INA240_PHASE_COUNT] = {
    INA240_MV_EMA_ALPHA_A,
    INA240_MV_EMA_ALPHA_BC,
    INA240_MV_EMA_ALPHA_BC,
};

#if INA240_USE_MANUAL_OFFSET
static const float s_manual_offset_mv[INA240_PHASE_COUNT] = {
    INA240_MANUAL_OFFSET_A_MV,
    INA240_MANUAL_OFFSET_B_MV,
    INA240_MANUAL_OFFSET_C_MV,
};
#endif

static float s_offset_mv[INA240_PHASE_COUNT] = {
    INA240_NOMINAL_OFFSET_MV,
    INA240_NOMINAL_OFFSET_MV,
    INA240_NOMINAL_OFFSET_MV,
};

static float s_adc_zero_mv[INA240_PHASE_COUNT] = {
    INA240_NOMINAL_OFFSET_MV,
    INA240_NOMINAL_OFFSET_MV,
    INA240_NOMINAL_OFFSET_MV,
};

static float s_bench_corr_mv[INA240_PHASE_COUNT] = {0.0f, 0.0f, 0.0f};

static float s_filtered_mv[INA240_PHASE_COUNT] = {
    INA240_NOMINAL_OFFSET_MV,
    INA240_NOMINAL_OFFSET_MV,
    INA240_NOMINAL_OFFSET_MV,
};

static uint32_t s_boot_raw_avg[INA240_PHASE_COUNT];
static uint32_t s_boot_mv_linear[INA240_PHASE_COUNT];
static uint32_t s_boot_mv_cal[INA240_PHASE_COUNT];

static bool s_initialized = false;
static bool s_offset_calibrated = false;

static void reset_filtered_to_adc_zero(void)
{
    for (int phase = 0; phase < INA240_PHASE_COUNT; phase++) {
        s_filtered_mv[phase] = s_adc_zero_mv[phase];
    }
}

#if INA240_A_MEDIAN_SAMPLES >= 3U
/** Mediana de N leituras mV no GPIO 34 (rejeita impulsos RF). */
static uint32_t read_mv_median_phase_a(void)
{
    uint32_t samples[INA240_A_MEDIAN_SAMPLES];

    for (uint32_t i = 0U; i < INA240_A_MEDIAN_SAMPLES; i++) {
        samples[i] = hal_adc_read_mv(HAL_ADC_PHASE_IA);
    }

    for (uint32_t i = 1U; i < INA240_A_MEDIAN_SAMPLES; i++) {
        uint32_t key = samples[i];
        int j = (int)i - 1;

        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }

    if ((INA240_A_MEDIAN_SAMPLES % 2U) == 0U) {
        const uint32_t mid = INA240_A_MEDIAN_SAMPLES / 2U;

        return (samples[mid - 1U] + samples[mid]) / 2U;
    }

    return samples[INA240_A_MEDIAN_SAMPLES / 2U];
}
#endif

static uint32_t read_phase_mv(ina240_phase_t phase)
{
#if INA240_A_MEDIAN_SAMPLES >= 3U
    if (phase == INA240_PHASE_A) {
        return read_mv_median_phase_a();
    }
#endif

    return hal_adc_read_mv(s_adc_channels[phase]);
}

bool ina240_init(void)
{
    s_initialized = true;
    return true;
}

bool ina240_is_offset_calibrated(void)
{
    return s_offset_calibrated;
}

/**
 * @brief Calibra offset de cada fase com corrente zero (motor parado, PWM off).
 * Média ADC substitui offset nominal; com INA240_USE_MANUAL_OFFSET aplica correção delta.
 */
bool ina240_calibrate_offset(uint16_t sample_count)
{
    uint32_t raw_sums[INA240_PHASE_COUNT] = {0U, 0U, 0U};
    uint32_t mv_sums[INA240_PHASE_COUNT] = {0U, 0U, 0U};

    if (!s_initialized || sample_count == 0U) {
        return false;
    }

    for (uint16_t sample = 0; sample < sample_count; sample++) {
        for (int phase = 0; phase < INA240_PHASE_COUNT; phase++) {
            const int raw = hal_adc_read_raw(s_adc_channels[phase]);

            if (raw < 0) {
                return false;
            }

            raw_sums[phase] += (uint32_t)raw;
            mv_sums[phase] += hal_adc_raw_to_mv(raw);
        }
    }

    for (int phase = 0; phase < INA240_PHASE_COUNT; phase++) {
        const uint32_t raw_avg = raw_sums[phase] / (uint32_t)sample_count;
        const uint32_t mv_avg = mv_sums[phase] / (uint32_t)sample_count;

        s_boot_raw_avg[phase] = raw_avg;
        s_boot_mv_linear[phase] = hal_adc_raw_to_mv_linear((int)raw_avg);
        s_boot_mv_cal[phase] = mv_avg;
        s_adc_zero_mv[phase] = (float)mv_avg;

#if INA240_USE_MANUAL_OFFSET
        s_offset_mv[phase] = s_manual_offset_mv[phase];
        s_bench_corr_mv[phase] = s_offset_mv[phase] - s_adc_zero_mv[phase];
#else
        s_offset_mv[phase] = s_adc_zero_mv[phase];
        s_bench_corr_mv[phase] = 0.0f;
#endif
    }

    s_offset_calibrated = true;
    reset_filtered_to_adc_zero();
    return true;
}

/** Recalibra adc_zero/bench_corr em runtime; offset serial (manual) inalterado. */
bool ina240_recalibrate_runtime(uint16_t sample_count)
{
    return ina240_calibrate_offset(sample_count);
}

/** Recalibra adc_zero/bench_corr com softAP ativo; offset serial (manual) inalterado. */
bool ina240_recalibrate_after_wifi(uint16_t sample_count)
{
    return ina240_recalibrate_runtime(sample_count);
}

float ina240_get_offset_mv(ina240_phase_t phase)
{
    if (phase >= INA240_PHASE_COUNT) {
        return INA240_NOMINAL_OFFSET_MV;
    }

    return s_offset_mv[phase];
}

void ina240_log_boot_diagnostics(void)
{
    printf("[Boot] INA240 diagnostico (ADC cal=%s (%s)",
           hal_adc_is_calibrated() ? "ON" : "OFF",
           hal_adc_cal_scheme_name());
#if INA240_USE_MANUAL_OFFSET
    printf(", manual=ON");
#endif
    printf(")\n");

    for (int phase = 0; phase < INA240_PHASE_COUNT; phase++) {
        const float offset = s_offset_mv[phase];
        const float delta_nom = offset - INA240_NOMINAL_OFFSET_MV;

        printf("  Fase %s: raw_avg=%u  mV_linear=%u  mV_cal=%u  adc_zero=%.0f  bench_corr=%+.0f  offset=%.0f  d_vs_1650=%+.0f mV\n",
               s_phase_names[phase],
               (unsigned)s_boot_raw_avg[phase],
               (unsigned)s_boot_mv_linear[phase],
               (unsigned)s_boot_mv_cal[phase],
               s_adc_zero_mv[phase],
               s_bench_corr_mv[phase],
               offset,
               delta_nom);
    }
}

/** Lê ADC, aplica EMA + bench_corr, subtrai offset e divide pelo ganho×shunt. */
float ina240_read_amps(ina240_phase_t phase)
{
    uint32_t mv;
    float v_eff;

    if (!s_initialized || phase >= INA240_PHASE_COUNT) {
        return 0.0f;
    }

    mv = read_phase_mv(phase);

    if (s_offset_calibrated) {
        const float alpha = s_ema_alpha[phase];

        s_filtered_mv[phase] = (alpha * (float)mv) +
                               ((1.0f - alpha) * s_filtered_mv[phase]);
        v_eff = s_filtered_mv[phase] + s_bench_corr_mv[phase];
        return (v_eff - s_offset_mv[phase]) / INA240_MV_TO_AMPS_SCALE;
    }

    return ((float)mv - s_offset_mv[phase]) / INA240_MV_TO_AMPS_SCALE;
}
