/*
 * ps4_calibration.h — Auto-calibração R2 e mapeamento travel → setpoint.
 */

#ifndef PS4_CALIBRATION_H
#define PS4_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ps4_calibration_reset(void);
void ps4_calibration_on_link_active(void);
bool ps4_calibration_is_ready(void);
uint8_t ps4_calibration_get_rest(void);
uint8_t ps4_calibration_scale_throttle(int32_t throttle);
void ps4_calibration_update(uint8_t r2_raw, uint32_t now_ms);
uint8_t ps4_calibration_effective_from_raw(uint8_t r2_raw);
bool ps4_calibration_throttle_active(uint8_t r2_raw, uint8_t r2_effective);
uint8_t ps4_calibration_travel_for_map(uint8_t r2_raw, uint8_t r2_effective);
float ps4_calibration_map_travel_to_amps(uint8_t travel, bool zero_rest);
float ps4_calibration_map_travel_to_rpm(uint8_t travel, bool zero_rest);

#ifdef __cplusplus
}
#endif

#endif /* PS4_CALIBRATION_H */
