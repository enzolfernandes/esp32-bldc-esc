#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_ADC_PHASE_IA = 0,
    HAL_ADC_PHASE_IB,
    HAL_ADC_PHASE_IC,
    HAL_ADC_VBAT,
    HAL_ADC_CHANNEL_COUNT
} hal_adc_channel_t;

bool hal_adc_init(void);
uint32_t hal_adc_read_mv(hal_adc_channel_t channel);

#ifdef __cplusplus
}
#endif

#endif // HAL_ADC_H
