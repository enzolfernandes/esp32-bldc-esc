/*
 * serial_hmi.cpp — Task FreeRTOS (Core 0) lê comandos Serial e preenche ps4_input_state_t.
 *
 * Comandos: A/a arm-disarm toggle, +/- setpoint, espaço e-stop.
 * FSM/motor só via apply_ps4_to_esc() no loop() (Core 1).
 */

#include "serial_hmi.h"

#include "board_config.h"
#include "fsm_system.h"
#include "motor_control.h"
#include "ps4_calibration.h"

#include <Arduino.h>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
}

typedef struct {
    bool wants_armed;
    bool share_pulse;
    uint8_t throttle_level;
    float target_rpm;
    float target_amps;
} serial_hmi_internal_t;

static serial_hmi_internal_t s_internal = {};
static ps4_input_state_t s_snapshot = {};
static SemaphoreHandle_t s_mutex = nullptr;
static TaskHandle_t s_task = nullptr;

static uint8_t rpm_to_throttle_level(float rpm)
{
    if (rpm <= 0.0f) {
        return 0U;
    }

    const float range = MOTOR_SPEED_MAX_RPM - MOTOR_SPEED_MIN_RPM;

    if (range <= 0.0f) {
        return 255U;
    }

    float norm = (rpm - MOTOR_SPEED_MIN_RPM) / range;

    if (norm < 0.0f) {
        norm = 0.0f;
    }
    if (norm > 1.0f) {
        norm = 1.0f;
    }

    const uint16_t travel = (uint16_t)(norm * 255.0f);

    return (uint8_t)(travel + PS4_R2_ZERO_REST_ARM_RAW);
}

static uint8_t amps_to_throttle_level(float amps)
{
    if (amps <= 0.0f) {
        return 0U;
    }

    float norm = amps / MOTOR_CONTROL_MAX_TARGET_AMPS;

    if (norm > 1.0f) {
        norm = 1.0f;
    }

    const uint16_t travel = (uint16_t)(norm * 255.0f);

    return (uint8_t)(travel + PS4_R2_ZERO_REST_ARM_RAW);
}

static uint8_t serial_travel_from_level(uint8_t r2_raw)
{
    if (r2_raw <= PS4_R2_ZERO_REST_ARM_RAW) {
        return 0U;
    }

    const uint16_t travel = (uint16_t)r2_raw - (uint16_t)PS4_R2_ZERO_REST_ARM_RAW;

    return (travel > 255U) ? 255U : (uint8_t)travel;
}

static void serial_sync_targets_from_throttle(serial_hmi_internal_t *st)
{
    const uint8_t travel = serial_travel_from_level(st->throttle_level);

    st->target_rpm = ps4_calibration_map_travel_to_rpm(travel, true);
    st->target_amps = ps4_calibration_map_travel_to_amps(travel, true);
}

static void sync_throttle_from_targets(serial_hmi_internal_t *st)
{
#if MOTOR_CONTROL_DEFAULT_MODE == MOTOR_CONTROL_MODE_SPEED
    st->throttle_level = rpm_to_throttle_level(st->target_rpm);
#else
    st->throttle_level = amps_to_throttle_level(st->target_amps);
#endif
}

static void fill_snapshot_locked(const serial_hmi_internal_t *st, ps4_input_state_t *out)
{
    const uint8_t r2_raw = st->throttle_level;
    const uint8_t travel = serial_travel_from_level(r2_raw);
    const bool throttle_active =
        st->wants_armed &&
        (r2_raw >= PS4_R2_ZERO_REST_ARM_RAW ||
         st->target_rpm > 0.0f || st->target_amps > 0.0f);

    out->connected = true;
    out->options_pressed = false;
    out->share_pressed = st->share_pulse;
    out->circle_pressed = false;
    out->r2_raw = r2_raw;
    out->r2_rest = 0U;
    out->r2_effective = travel;
    out->throttle_active = throttle_active;
    out->target_rpm = st->target_rpm;
    out->target_amps = st->target_amps;
    out->direction = 1;
}

static void handle_char(char c, serial_hmi_internal_t *st)
{
    switch (c) {
    case 'A':
    case 'a':
        st->wants_armed = !st->wants_armed;

        if (st->wants_armed) {
            if (st->throttle_level < PS4_R2_ZERO_REST_ARM_RAW + 1U) {
                st->throttle_level = PS4_R2_ZERO_REST_ARM_RAW + 1U;
            }

#if MOTOR_CONTROL_DEFAULT_MODE == MOTOR_CONTROL_MODE_SPEED
            if (st->target_rpm <= 0.0f) {
                st->target_rpm = SERIAL_HMI_RPM_STEP;
            }
#else
            if (st->target_amps <= 0.0f) {
                st->target_amps = SERIAL_HMI_AMPS_STEP;
            }
#endif
            serial_sync_targets_from_throttle(st);
        } else {
            st->throttle_level = 0U;
            st->target_rpm = 0.0f;
            st->target_amps = 0.0f;
        }

        break;

    case '+':
#if MOTOR_CONTROL_DEFAULT_MODE == MOTOR_CONTROL_MODE_SPEED
        st->target_rpm += SERIAL_HMI_RPM_STEP;

        if (st->target_rpm > MOTOR_SPEED_MAX_RPM) {
            st->target_rpm = MOTOR_SPEED_MAX_RPM;
        }
#else
        st->target_amps += SERIAL_HMI_AMPS_STEP;

        if (st->target_amps > MOTOR_CONTROL_MAX_TARGET_AMPS) {
            st->target_amps = MOTOR_CONTROL_MAX_TARGET_AMPS;
        }
#endif
        st->wants_armed = true;
        sync_throttle_from_targets(st);
        break;

    case '-':
#if MOTOR_CONTROL_DEFAULT_MODE == MOTOR_CONTROL_MODE_SPEED
        if (st->target_rpm > SERIAL_HMI_RPM_STEP) {
            st->target_rpm -= SERIAL_HMI_RPM_STEP;
        } else {
            st->target_rpm = 0.0f;
        }
#else
        if (st->target_amps > SERIAL_HMI_AMPS_STEP) {
            st->target_amps -= SERIAL_HMI_AMPS_STEP;
        } else {
            st->target_amps = 0.0f;
        }
#endif

        if (st->target_rpm <= 0.0f && st->target_amps <= 0.0f) {
            st->wants_armed = false;
            st->throttle_level = 0U;
        } else {
            sync_throttle_from_targets(st);
        }

        break;

    case ' ':
        st->share_pulse = true;
        st->wants_armed = false;
        st->throttle_level = 0U;
        st->target_rpm = 0.0f;
        st->target_amps = 0.0f;
        break;

    case 'c':
    case 'C':
        if (fsm_system_get_state() == ESC_STATE_FAULT) {
            if (fsm_system_clear_fault()) {
                st->wants_armed = false;
                st->throttle_level = 0U;
                st->target_rpm = 0.0f;
                st->target_amps = 0.0f;
            }
        }
        break;

    default:
        break;
    }
}

static void vTaskSerialHMI(void *arg)
{
    (void)arg;

    for (;;) {
        while (Serial.available() > 0) {
            const char c = static_cast<char>(Serial.read());

            if (s_mutex == nullptr) {
                continue;
            }

            if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
                continue;
            }

            handle_char(c, &s_internal);
            fill_snapshot_locked(&s_internal, &s_snapshot);
            s_internal.share_pulse = false;

            xSemaphoreGive(s_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(SERIAL_HMI_POLL_MS));
    }
}

extern "C" bool serial_hmi_init(void)
{
    s_internal = {};
    s_snapshot = {};
    s_internal.wants_armed = false;
    s_internal.throttle_level = 0U;

    if (s_mutex == nullptr) {
        s_mutex = xSemaphoreCreateMutex();

        if (s_mutex == nullptr) {
            return false;
        }
    }

    if (s_task == nullptr) {
        const BaseType_t ok = xTaskCreatePinnedToCore(
            vTaskSerialHMI,
            "serial_hmi",
            SERIAL_HMI_TASK_STACK,
            nullptr,
            SERIAL_HMI_TASK_PRIO,
            &s_task,
            SERIAL_HMI_TASK_CORE);

        if (ok != pdPASS) {
            s_task = nullptr;
            return false;
        }
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        fill_snapshot_locked(&s_internal, &s_snapshot);
        xSemaphoreGive(s_mutex);
    }

    return true;
}

extern "C" bool serial_hmi_update(ps4_input_state_t *out)
{
    if (out == nullptr || s_mutex == nullptr) {
        return false;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false;
    }

    *out = s_snapshot;
    s_snapshot.share_pressed = false;
    xSemaphoreGive(s_mutex);

    return true;
}

extern "C" void serial_hmi_shutdown(void)
{
    if (s_task != nullptr) {
        vTaskDelete(s_task);
        s_task = nullptr;
    }

    if (s_mutex != nullptr) {
        vSemaphoreDelete(s_mutex);
        s_mutex = nullptr;
    }
}
