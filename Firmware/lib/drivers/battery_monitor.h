#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool battery_monitor_init(void);
float battery_monitor_read_volts(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_MONITOR_H
