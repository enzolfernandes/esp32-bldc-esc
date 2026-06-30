/*
 * ps4_calibration.cpp — Repouso R2 e mapeamento para RPM/corrente.
 */

#include "ps4_calibration.h"

#include "board_config.h"
#include "motor_control.h"

#include <Arduino.h>

static uint8_t s_r2_rest = 255U;
static bool s_r2_calibrated = false;
static uint32_t s_zero_rest_begin_ms = 0U;
static uint32_t s_stable_cal_start_ms = 0U;
static uint8_t s_stable_cal_min = 255U;
static uint8_t s_stable_cal_max = 0U;
static bool s_had_connection = false;

static void finish_r2_calibration(uint8_t rest)
{
    s_r2_rest = rest;
    s_r2_calibrated = true;
    s_zero_rest_begin_ms = 0U;
    s_stable_cal_start_ms = 0U;
    Serial.printf("[PS4] R2 cal OK: repouso=%u\n", static_cast<unsigned>(rest));
}

static void update_stable_analog_cal(uint8_t r2_raw, uint32_t now_ms)
{
    if (s_stable_cal_start_ms == 0U) {
        s_stable_cal_start_ms = now_ms;
        s_stable_cal_min = r2_raw;
        s_stable_cal_max = r2_raw;
        return;
    }

    if (r2_raw + PS4_R2_STABLE_SPREAD < s_stable_cal_min ||
        r2_raw > s_stable_cal_max + PS4_R2_STABLE_SPREAD) {
        s_stable_cal_start_ms = now_ms;
        s_stable_cal_min = r2_raw;
        s_stable_cal_max = r2_raw;
        return;
    }

    if (r2_raw < s_stable_cal_min) {
        s_stable_cal_min = r2_raw;
    }
    if (r2_raw > s_stable_cal_max) {
        s_stable_cal_max = r2_raw;
    }

    if ((now_ms - s_stable_cal_start_ms) < PS4_R2_CALIB_MS) {
        return;
    }

    const uint8_t spread = s_stable_cal_max - s_stable_cal_min;

    if (spread <= PS4_R2_STABLE_SPREAD &&
        s_stable_cal_max <= PS4_R2_STABLE_MAX_IDLE) {
        finish_r2_calibration(s_stable_cal_min);
        return;
    }

    s_stable_cal_start_ms = 0U;
}

void ps4_calibration_reset(void)
{
    s_r2_calibrated = false;
    s_r2_rest = 255U;
    s_zero_rest_begin_ms = 0U;
    s_stable_cal_start_ms = 0U;
    s_stable_cal_min = 255U;
    s_stable_cal_max = 0U;
    s_had_connection = false;
}

void ps4_calibration_on_link_active(void)
{
    s_had_connection = true;
}

bool ps4_calibration_is_ready(void)
{
    return s_r2_calibrated;
}

uint8_t ps4_calibration_get_rest(void)
{
    return s_r2_rest;
}

uint8_t ps4_calibration_scale_throttle(int32_t throttle)
{
    if (throttle < 0) {
        throttle = 0;
    }
    if (throttle > 1023) {
        throttle = 1023;
    }

    return (uint8_t)(((uint32_t)throttle * 255U) / 1023U);
}

void ps4_calibration_update(uint8_t r2_raw, uint32_t now_ms)
{
    if (!s_had_connection) {
        s_had_connection = true;
    }

    if (s_r2_calibrated) {
        if (r2_raw <= (uint8_t)(s_r2_rest + PS4_R2_REST_MARGIN + 4U)) {
            if (s_r2_rest == 0U && r2_raw == 0U) {
                return;
            }

            s_r2_rest = (uint8_t)(((uint16_t)s_r2_rest * 31U + (uint16_t)r2_raw) / 32U);
        }

        return;
    }

    if (r2_raw == 0U) {
        s_stable_cal_start_ms = 0U;

        if (s_zero_rest_begin_ms == 0U) {
            s_zero_rest_begin_ms = now_ms;
        } else if ((now_ms - s_zero_rest_begin_ms) >= PS4_R2_ZERO_REST_MS) {
            finish_r2_calibration(0U);
        }

        return;
    }

    s_zero_rest_begin_ms = 0U;

    if (r2_raw >= PS4_R2_ANALOG_REST_MIN) {
        update_stable_analog_cal(r2_raw, now_ms);
    }
}

uint8_t ps4_calibration_effective_from_raw(uint8_t r2_raw)
{
    const uint16_t margin = (uint16_t)s_r2_rest + (uint16_t)PS4_R2_REST_MARGIN;

    if ((uint16_t)r2_raw <= margin) {
        return 0U;
    }

    const uint16_t eff = (uint16_t)r2_raw - margin;

    return (eff > 255U) ? 255U : (uint8_t)eff;
}

bool ps4_calibration_throttle_active(uint8_t r2_raw, uint8_t r2_effective)
{
    if (!s_r2_calibrated) {
        return false;
    }

    if (s_r2_rest == 0U) {
        return r2_raw >= PS4_R2_ZERO_REST_ARM_RAW;
    }

    return r2_effective >= PS4_R2_ARM_EFFECTIVE;
}

uint8_t ps4_calibration_travel_for_map(uint8_t r2_raw, uint8_t r2_effective)
{
    if (s_r2_calibrated && s_r2_rest == 0U) {
        if (r2_raw <= PS4_R2_ZERO_REST_ARM_RAW) {
            return 0U;
        }

        const uint16_t travel = (uint16_t)r2_raw - PS4_R2_ZERO_REST_ARM_RAW;

        return (travel > 255U) ? 255U : (uint8_t)travel;
    }

    return r2_effective;
}

float ps4_calibration_map_travel_to_amps(uint8_t travel, bool zero_rest)
{
    const uint8_t min_travel = zero_rest ? 0U : PS4_R2_ARM_EFFECTIVE;

    if (travel < min_travel) {
        return 0.0f;
    }

    const float span = (float)(travel - min_travel);
    const float range = (float)(255U - min_travel);

    return (span / range) * MOTOR_CONTROL_MAX_TARGET_AMPS;
}

float ps4_calibration_map_travel_to_rpm(uint8_t travel, bool zero_rest)
{
    const uint8_t min_travel = zero_rest ? 0U : PS4_R2_ARM_EFFECTIVE;

    if (travel < min_travel) {
        return 0.0f;
    }

    const float span = (float)(travel - min_travel);
    const float range = (float)(255U - min_travel);

    return (span / range) * MOTOR_SPEED_MAX_RPM;
}
