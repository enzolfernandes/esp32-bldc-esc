#include "battery_monitor.h"

#include "hal_adc.h"

// Divisor: 39 kOhm (topo) / 4,7 kOhm (base) -> V_adc = V_bat * 4,7 / 43,7
#define BATTERY_DIVIDER_RATIO  (4.7f / (39.0f + 4.7f))

static bool s_initialized = false;

bool battery_monitor_init(void)
{
    s_initialized = true;
    return true;
}

float battery_monitor_read_volts(void)
{
    uint32_t mv;

    if (!s_initialized) {
        return 0.0f;
    }

    mv = hal_adc_read_mv(HAL_ADC_VBAT);
    return ((float)mv / 1000.0f) / BATTERY_DIVIDER_RATIO;
}
