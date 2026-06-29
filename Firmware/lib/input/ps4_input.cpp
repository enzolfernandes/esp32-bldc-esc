/*
 * ps4_input.cpp — Entrada via DualShock 4 (Bluepad32 / Bluetooth Classic).
 *
 * Camada: entrada. Polling a cada PS4_INPUT_POLL_MS no loop() de main.cpp.
 * Expõe estado normalizado (R2, direção, Options) sem referenciar a FSM.
 */

#include "ps4_input.h"

#include "board_config.h"
#include "motor_control.h"

#include <Bluepad32.h>

static ControllerPtr s_controllers[BP32_MAX_GAMEPADS];
static bool s_connected = false;
static bool s_prev_options = false;
static ps4_led_status_t s_last_led_status = PS4_LED_OFF;

/** Mapeia estado da FSM para cor RGB da lightbar do controle. */
static void apply_led_color(ControllerPtr ctl, ps4_led_status_t status)
{
    switch (status) {
    case PS4_LED_INIT:
        ctl->setColorLED(255, 165, 0);
        break;
    case PS4_LED_IDLE:
        ctl->setColorLED(0, 120, 255);
        break;
    case PS4_LED_RUNNING:
        ctl->setColorLED(0, 255, 0);
        break;
    case PS4_LED_FAULT:
        ctl->setColorLED(255, 0, 0);
        break;
    case PS4_LED_OFF:
    default:
        break;
    }
}

static void on_connected_controller(ControllerPtr ctl)
{
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (s_controllers[i] == nullptr) {
            s_controllers[i] = ctl;
            s_connected = true;
            apply_led_color(ctl, PS4_LED_IDLE);
            s_last_led_status = PS4_LED_IDLE;
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

    if (!s_connected) {
        s_last_led_status = PS4_LED_OFF;
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

/** Bluepad32 throttle() é 0–1023; normaliza para 0–255 como no protocolo DS4. */
static uint8_t scale_throttle_to_r2(int32_t throttle)
{
    if (throttle < 0) {
        throttle = 0;
    }
    if (throttle > 1023) {
        throttle = 1023;
    }

    return static_cast<uint8_t>((static_cast<uint32_t>(throttle) * 255U) / 1023U);
}

/** Modo CURRENT: R2 acima do limiar mapeia linearmente 0–MOTOR_CONTROL_MAX_TARGET_AMPS. */
static float map_r2_to_amps(uint8_t r2_raw)
{
    if (r2_raw <= PS4_R2_ARM_THRESHOLD) {
        return 0.0f;
    }

    const float effective = static_cast<float>(r2_raw - PS4_R2_ARM_THRESHOLD);
    const float range = static_cast<float>(255U - PS4_R2_ARM_THRESHOLD);

    return (effective / range) * MOTOR_CONTROL_MAX_TARGET_AMPS;
}

/** Modo SPEED: R2 mapeia linearmente 0–MOTOR_SPEED_MAX_RPM. */
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
    s_last_led_status = PS4_LED_OFF;

    BP32.setup(&on_connected_controller, &on_disconnected_controller);
#if PS4_ENABLE_VIRTUAL_DEVICE
    BP32.enableVirtualDevice(true);
#else
    BP32.enableVirtualDevice(false);
#endif
    BP32.enableBLEService(false);

    return true;
}

extern "C" bool ps4_input_is_connected(void)
{
    return s_connected && (first_active_gamepad() != nullptr);
}

extern "C" void ps4_input_set_led_status(ps4_led_status_t status)
{
    if (status == s_last_led_status) {
        return;
    }

    ControllerPtr ctl = first_active_gamepad();
    if (ctl == nullptr) {
        if (status == PS4_LED_OFF) {
            s_last_led_status = PS4_LED_OFF;
        }
        return;
    }

    if (status == PS4_LED_OFF) {
        s_last_led_status = PS4_LED_OFF;
        return;
    }

    apply_led_color(ctl, status);
    s_last_led_status = status;
}

/**
 * @brief Atualiza estado do controle (chamado a cada PS4_INPUT_POLL_MS).
 * Lê R2 via throttle() — não brake() (L2).
 */
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

    uint8_t r2_raw = scale_throttle_to_r2(ctl->throttle());

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
