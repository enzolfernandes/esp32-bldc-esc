#include "ps4_input.h"

#include "board_config.h"
#include "motor_control.h"

#include <Bluepad32.h>

static ControllerPtr s_controllers[BP32_MAX_GAMEPADS];
static bool s_connected = false;
static bool s_prev_options = false;

static void on_connected_controller(ControllerPtr ctl)
{
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (s_controllers[i] == nullptr) {
            s_controllers[i] = ctl;
            s_connected = true;
            return;
        }
    }
}

static void on_disconnected_controller(ControllerPtr ctl)
{
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (s_controllers[i] == ctl) {
            s_controllers[i] = nullptr;
            break;
        }
    }

    s_connected = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (s_controllers[i] != nullptr && s_controllers[i]->isConnected()) {
            s_connected = true;
            break;
        }
    }
}

static ControllerPtr first_active_gamepad(void)
{
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        ControllerPtr ctl = s_controllers[i];

        if (ctl != nullptr && ctl->isConnected() && ctl->isGamepad()) {
            return ctl;
        }
    }

    return nullptr;
}

static uint8_t scale_brake_to_r2(int32_t brake)
{
    if (brake < 0) {
        brake = 0;
    }
    if (brake > 1023) {
        brake = 1023;
    }

    return static_cast<uint8_t>((static_cast<uint32_t>(brake) * 255U) / 1023U);
}

static float map_r2_to_amps(uint8_t r2_raw)
{
    if (r2_raw <= PS4_R2_ARM_THRESHOLD) {
        return 0.0f;
    }

    const float effective = static_cast<float>(r2_raw - PS4_R2_ARM_THRESHOLD);
    const float range = static_cast<float>(255U - PS4_R2_ARM_THRESHOLD);

    return (effective / range) * MOTOR_CONTROL_MAX_TARGET_AMPS;
}

static float map_r2_to_rpm(uint8_t r2_raw)
{
    if (r2_raw <= PS4_R2_ARM_THRESHOLD) {
        return 0.0f;
    }

    const float effective = static_cast<float>(r2_raw - PS4_R2_ARM_THRESHOLD);
    const float range = static_cast<float>(255U - PS4_R2_ARM_THRESHOLD);

    return (effective / range) * MOTOR_SPEED_MAX_RPM;
}

static void fill_disconnected_state(ps4_input_state_t *out)
{
    out->connected = false;
    out->options_pressed = false;
    out->circle_pressed = false;
    out->r2_raw = 0U;
    out->target_amps = 0.0f;
    out->target_rpm = 0.0f;
    out->direction = 1;
    s_prev_options = false;
}

extern "C" bool ps4_input_init(void)
{
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        s_controllers[i] = nullptr;
    }

    s_connected = false;
    s_prev_options = false;

    BP32.setup(&on_connected_controller, &on_disconnected_controller);
    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);

    return true;
}

extern "C" bool ps4_input_is_connected(void)
{
    return s_connected && (first_active_gamepad() != nullptr);
}

extern "C" bool ps4_input_update(ps4_input_state_t *out)
{
    if (out == nullptr) {
        return false;
    }

    BP32.update();

    ControllerPtr ctl = first_active_gamepad();
    if (ctl == nullptr) {
        fill_disconnected_state(out);
        return true;
    }

    uint8_t r2_raw = scale_brake_to_r2(ctl->brake());

    if (r2_raw <= PS4_R2_DEADZONE) {
        r2_raw = 0U;
    }

    const bool options_now = ctl->miscStart();
    const bool options_edge = options_now && !s_prev_options;

    s_prev_options = options_now;

    out->connected = true;
    out->options_pressed = options_edge;
    out->circle_pressed = ctl->b();
    out->r2_raw = r2_raw;
    out->target_amps = map_r2_to_amps(r2_raw);
    out->target_rpm = map_r2_to_rpm(r2_raw);
    out->direction = out->circle_pressed ? -1 : 1;

    return true;
}
