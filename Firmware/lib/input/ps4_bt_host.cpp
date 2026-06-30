/*
 * ps4_bt_host.cpp — Bluepad32 setup/poll, callbacks e estados OFF/PAIRING/READY/ACTIVE.
 */

#include "ps4_bt_host.h"

#include "board_config.h"
#include "ps4_calibration.h"
#include "ps4_feedback.h"
#include "ps4_platform_hook.h"

#include <Arduino.h>
#include <Bluepad32.h>

extern "C" {
#include "bt/uni_bt.h"
#include "uni_virtual_device.h"
}

static ControllerPtr s_controllers[BP32_MAX_GAMEPADS];
static ps4_link_state_t s_link_state = PS4_LINK_OFF;
static uint32_t s_connect_ms = 0U;
static uint32_t s_disconnect_count = 0U;
static uint32_t s_pairing_since_ms = 0U;
static bool s_auto_recovery_done = false;
static bool s_callback_connected = false;

static void set_link_state(ps4_link_state_t next)
{
    if (s_link_state == next) {
        return;
    }

    s_link_state = next;
}

static void reenable_bt_connections(void)
{
    BP32.enableNewBluetoothConnections(true);
}

static void try_auto_pairing_recovery(uint32_t now_ms)
{
#if PS4_AUTO_RECOVERY_PAIRING_MS > 0U
    if (s_callback_connected || s_auto_recovery_done) {
        return;
    }

    if (s_pairing_since_ms == 0U) {
        s_pairing_since_ms = now_ms;
        return;
    }

    if ((now_ms - s_pairing_since_ms) < PS4_AUTO_RECOVERY_PAIRING_MS) {
        return;
    }

    s_auto_recovery_done = true;
    BP32.forgetBluetoothKeys();
    reenable_bt_connections();
    s_pairing_since_ms = now_ms;

    Serial.println("[PS4] auto-recovery: SDP/pareamento preso — chaves apagadas; Share+PS no controle");
#endif
}

static void disable_virtual_device(void)
{
    /* Garante virt=0 após BP32.setup (lib pode restaurar default). bluepad32#194 */
    uni_virtual_device_set_enabled(false);
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

static void refresh_pairing_state(void)
{
    if (s_callback_connected) {
        if (s_link_state < PS4_LINK_READY) {
            set_link_state(PS4_LINK_READY);
        }
        return;
    }

    if (first_active_gamepad() != nullptr) {
        set_link_state(PS4_LINK_PAIRING);
        return;
    }

    if (s_link_state != PS4_LINK_OFF) {
        set_link_state(PS4_LINK_PAIRING);
    }
}

static void on_connected_controller(ControllerPtr ctl)
{
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (s_controllers[i] == nullptr) {
            s_controllers[i] = ctl;
            s_callback_connected = true;
            s_connect_ms = static_cast<uint32_t>(millis());
            s_pairing_since_ms = 0U;
            set_link_state(PS4_LINK_READY);
            Serial.printf("[PS4] conectado  virt=%d  gap=%d  heap=%u\n",
                          uni_virtual_device_is_enabled() ? 1 : 0,
                          uni_bt_get_gap_security_level(),
                          static_cast<unsigned>(ESP.getFreeHeap()));
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

    s_callback_connected = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (s_controllers[i] != nullptr && s_controllers[i]->isConnected()) {
            s_callback_connected = true;
            break;
        }
    }

    if (!s_callback_connected) {
        ps4_calibration_reset();
        ps4_feedback_reset();
        s_disconnect_count++;
        const uint32_t now_ms = static_cast<uint32_t>(millis());
        const uint32_t uptime_ms =
            (s_connect_ms != 0U && now_ms >= s_connect_ms) ? (now_ms - s_connect_ms) : 0U;
        Serial.printf("[PS4] desconectado (#%lu, sessao=%lu ms) gap=%d heap=%u — recal R2\n",
                      static_cast<unsigned long>(s_disconnect_count),
                      static_cast<unsigned long>(uptime_ms),
                      uni_bt_get_gap_security_level(),
                      static_cast<unsigned>(ESP.getFreeHeap()));
        s_connect_ms = 0U;
        s_pairing_since_ms = static_cast<uint32_t>(millis());
        set_link_state(PS4_LINK_PAIRING);
        reenable_bt_connections();
    }
}

extern "C" bool ps4_bt_host_init(void)
{
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        s_controllers[i] = nullptr;
    }

    s_callback_connected = false;
    s_connect_ms = 0U;
    s_disconnect_count = 0U;
    s_pairing_since_ms = 0U;
    s_auto_recovery_done = false;
    ps4_calibration_reset();

    BP32.setup(&on_connected_controller, &on_disconnected_controller);

    ps4_platform_hook_install();

    reenable_bt_connections();

#if PS4_FORGET_BT_KEYS_ON_BOOT
    BP32.forgetBluetoothKeys();
    Serial.println("[PS4] chaves BT apagadas — use Share+PS (LED piscando) para parear");
#endif

    /* GAP só após BP32.setup() — chamar antes corrompe estado (gap_rt lixo no boot). */
    uni_bt_set_gap_security_level(PS4_GAP_SECURITY_LEVEL);

#if PS4_ENABLE_VIRTUAL_DEVICE
    BP32.enableVirtualDevice(true);
#else
    BP32.enableVirtualDevice(false);
    disable_virtual_device();
#endif

    /* BLE off: só BR/EDR para DS4; reduz contenção de rádio. */
    BP32.enableBLEService(false);
    uni_bt_enable_service_safe(false);

    set_link_state(PS4_LINK_PAIRING);
    s_pairing_since_ms = static_cast<uint32_t>(millis());

    Serial.printf("[PS4] init: BP32 %s  forget=%d  virt=%d  gap_cfg=%d  gap_rt=%d  ble_off=1  lightbar_skip=%d  profile=%d  auto_rec_ms=%u\n",
                  BP32.firmwareVersion(),
#if PS4_FORGET_BT_KEYS_ON_BOOT
                  1,
#else
                  0,
#endif
                  uni_virtual_device_is_enabled() ? 1 : 0,
                  PS4_GAP_SECURITY_LEVEL,
                  uni_bt_get_gap_security_level(),
#if PS4_SKIP_LIGHTBAR_ON_CLONE
                  1,
#else
                  0,
#endif
                  PS4_ACTIVE_PROFILE,
                  static_cast<unsigned>(PS4_AUTO_RECOVERY_PAIRING_MS));

    return true;
}

extern "C" void ps4_bt_host_poll(void)
{
    BP32.update();
    refresh_pairing_state();
    try_auto_pairing_recovery(static_cast<uint32_t>(millis()));
}

extern "C" ps4_link_state_t ps4_bt_host_get_link_state(void)
{
    return s_link_state;
}

extern "C" const char *ps4_bt_host_link_state_name(ps4_link_state_t state)
{
    switch (state) {
    case PS4_LINK_OFF:
        return "OFF";
    case PS4_LINK_PAIRING:
        return "PAIRING";
    case PS4_LINK_READY:
        return "READY";
    case PS4_LINK_ACTIVE:
        return "ACTIVE";
    default:
        return "?";
    }
}

extern "C" bool ps4_bt_host_is_ready(void)
{
    return s_callback_connected && s_link_state >= PS4_LINK_READY;
}

extern "C" void *ps4_bt_host_get_gamepad(void)
{
    ControllerPtr ctl = first_active_gamepad();

    return static_cast<void *>(ctl);
}

extern "C" uint32_t ps4_bt_host_connect_ms(void)
{
    return s_connect_ms;
}

extern "C" void ps4_bt_host_mark_active(void)
{
    if (s_link_state >= PS4_LINK_READY && s_link_state < PS4_LINK_ACTIVE) {
        set_link_state(PS4_LINK_ACTIVE);
    }
}

ControllerPtr ps4_bt_host_first_gamepad(void)
{
    return first_active_gamepad();
}
