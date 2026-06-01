#ifndef INA240_CURRENT_SENSORS_H
#define INA240_CURRENT_SENSORS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INA240_PHASE_A = 0,
    INA240_PHASE_B,
    INA240_PHASE_C,
    INA240_PHASE_COUNT
} ina240_phase_t;

bool ina240_init(void);
bool ina240_calibrate_offset(uint16_t sample_count);
float ina240_read_amps(ina240_phase_t phase);
float ina240_get_offset_mv(ina240_phase_t phase);

#ifdef __cplusplus
}
#endif

#endif // INA240_CURRENT_SENSORS_H
