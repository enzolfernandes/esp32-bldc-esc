#ifndef ESC_BOOT_SENSORS_H
#define ESC_BOOT_SENSORS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calibração prioritária INA240: GPIO safety → ADC (esp_adc_cal) → offset 128 amostras.
 * Chamada em initVariant() antes de setup/Wi-Fi/BP32.setup — sem Serial.
 */
bool esc_boot_early_calibrate(void);

#ifdef __cplusplus
}
#endif

#endif // ESC_BOOT_SENSORS_H
