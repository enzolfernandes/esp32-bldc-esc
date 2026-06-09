#ifndef HAL_DAC_H
#define HAL_DAC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool hal_dac_init(void);
bool hal_dac_set_voltage(float volts);
float hal_dac_get_voltage(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_DAC_H
