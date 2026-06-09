#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool battery_monitor_init(void);
float battery_monitor_read_volts(void);

void battery_monitor_tick(uint32_t now_ms);
bool battery_monitor_uvlo_active(void);
float battery_monitor_get_volts_filtered(void);

uint8_t battery_monitor_get_cell_count_s(void);
float battery_monitor_get_uvlo_cutoff_v(void);
float battery_monitor_get_uvlo_recover_v(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_MONITOR_H
