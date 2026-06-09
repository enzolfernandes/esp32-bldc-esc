#include "battery_monitor.h"

#include "board_config.h"
#include "hal_adc.h"

#include <math.h>

// Divisor: 39 kOhm (topo) / 4,7 kOhm (base) -> V_adc = V_bat * 4,7 / 43,7
#define BATTERY_DIVIDER_RATIO  (4.7f / (39.0f + 4.7f))

static bool s_initialized = false;
static bool s_uvlo_active = false;
static float s_volts_filtered = 0.0f;
static uint32_t s_below_since_ms = 0U;
static bool s_below_pending = false;
static uint8_t s_cell_count_s = BATTERY_CELL_COUNT_S_MIN;
static float s_uvlo_cutoff_v = 0.0f;
static float s_uvlo_recover_v = 0.0f;

static float read_volts_raw(void)
{
    const uint32_t mv = hal_adc_read_mv(HAL_ADC_VBAT);

    return ((float)mv / 1000.0f) / BATTERY_DIVIDER_RATIO;
}

static uint8_t clamp_cell_count(uint8_t cells)
{
    if (cells < BATTERY_CELL_COUNT_S_MIN) {
        return BATTERY_CELL_COUNT_S_MIN;
    }

    if (cells > BATTERY_CELL_COUNT_S_MAX) {
        return BATTERY_CELL_COUNT_S_MAX;
    }

    return cells;
}

static uint8_t detect_cell_count(float volts)
{
    uint8_t s_min;
    uint8_t s_max;
    uint8_t cells;

    if (volts <= 0.0f) {
        return BATTERY_CELL_COUNT_S_MIN;
    }

    s_min = (uint8_t)ceilf(volts / BATTERY_CELL_VMAX_FULL);

    if (s_min < BATTERY_CELL_COUNT_S_MIN) {
        s_min = BATTERY_CELL_COUNT_S_MIN;
    }

    if (s_min > BATTERY_CELL_COUNT_S_MAX) {
        s_min = BATTERY_CELL_COUNT_S_MAX;
    }

    s_max = (uint8_t)floorf(volts / BATTERY_CELL_UVLO_CUTOFF_V);

    if (s_max < BATTERY_CELL_COUNT_S_MIN) {
        s_max = BATTERY_CELL_COUNT_S_MIN;
    }

    if (s_max > BATTERY_CELL_COUNT_S_MAX) {
        s_max = BATTERY_CELL_COUNT_S_MAX;
    }

    if (s_min > s_max) {
        return clamp_cell_count(s_min);
    }

    cells = s_max;

    return clamp_cell_count(cells);
}

static void apply_cell_count(uint8_t cells)
{
    s_cell_count_s = clamp_cell_count(cells);
    s_uvlo_cutoff_v = (float)s_cell_count_s * BATTERY_CELL_UVLO_CUTOFF_V;
    s_uvlo_recover_v = (float)s_cell_count_s * BATTERY_CELL_UVLO_RECOVER_V;
}

bool battery_monitor_init(void)
{
    const float volts = read_volts_raw();

    s_initialized = true;
    s_uvlo_active = false;
    s_volts_filtered = volts;
    s_below_since_ms = 0U;
    s_below_pending = false;

    apply_cell_count(detect_cell_count(volts));

    return true;
}

float battery_monitor_read_volts(void)
{
    if (!s_initialized) {
        return 0.0f;
    }

    return read_volts_raw();
}

void battery_monitor_tick(uint32_t now_ms)
{
    const float volts = battery_monitor_read_volts();

    s_volts_filtered = volts;

    if (!s_initialized) {
        return;
    }

    if (s_uvlo_active) {
        if (volts >= s_uvlo_recover_v) {
            s_uvlo_active = false;
        }

        s_below_pending = false;
        s_below_since_ms = 0U;
        return;
    }

    if (volts < s_uvlo_cutoff_v) {
        if (!s_below_pending) {
            s_below_pending = true;
            s_below_since_ms = now_ms;
            return;
        }

        if ((now_ms - s_below_since_ms) >= BATTERY_UVLO_DEBOUNCE_MS) {
            s_uvlo_active = true;
            s_below_pending = false;
            s_below_since_ms = 0U;
        }

        return;
    }

    s_below_pending = false;
    s_below_since_ms = 0U;
}

bool battery_monitor_uvlo_active(void)
{
    return s_uvlo_active;
}

float battery_monitor_get_volts_filtered(void)
{
    return s_volts_filtered;
}

uint8_t battery_monitor_get_cell_count_s(void)
{
    return s_cell_count_s;
}

float battery_monitor_get_uvlo_cutoff_v(void)
{
    return s_uvlo_cutoff_v;
}

float battery_monitor_get_uvlo_recover_v(void)
{
    return s_uvlo_recover_v;
}
