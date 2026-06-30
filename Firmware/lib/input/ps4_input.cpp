/*
 * ps4_input.cpp — Facade: orquestra host BT, calibração R2 e feedback lightbar.
 *
 * API pública em ps4_input.h (inalterada para main.cpp / FSM).
 */

#include "ps4_input.h"

#include "board_config.h"
#include "ps4_calibration.h"
#include "ps4_feedback.h"

#if BOARD_ENABLE_PS4_BT
#include "ps4_bt_host.h"
#include <Bluepad32.h>
#endif

static bool s_prev_options = false;
static bool s_prev_share = false;

static void fill_disconnected_state(ps4_input_state_t *out)
{
    out->connected = false;
    out->options_pressed = false;
    out->share_pressed = false;
    out->circle_pressed = false;
    out->r2_raw = 0U;
    out->r2_rest = 0U;
    out->r2_effective = 0U;
    out->throttle_active = false;
    out->target_amps = 0.0f;
    out->target_rpm = 0.0f;
    out->direction = 1;
    s_prev_options = false;
    s_prev_share = false;
}

extern "C" bool ps4_input_init(void)
{
    s_prev_options = false;
    s_prev_share = false;
    ps4_feedback_reset();

#if BOARD_ENABLE_PS4_BT
    return ps4_bt_host_init();
#else
    return true;
#endif
}

extern "C" bool ps4_input_is_connected(void)
{
#if BOARD_ENABLE_PS4_BT
    return ps4_bt_host_is_ready() && (ps4_bt_host_first_gamepad() != nullptr);
#else
    return false;
#endif
}

extern "C" bool ps4_input_r2_calibrated(void)
{
    return ps4_calibration_is_ready();
}

extern "C" void ps4_input_set_led_status(ps4_led_status_t status)
{
    ps4_feedback_set_status(status);
}

extern "C" bool ps4_input_update(ps4_input_state_t *out)
{
    if (out == nullptr) {
        return false;
    }

#if !BOARD_ENABLE_PS4_BT
    fill_disconnected_state(out);
    return false;
#else
    ps4_bt_host_poll();

    ControllerPtr ctl = ps4_bt_host_first_gamepad();

    if (ctl == nullptr || !ps4_bt_host_is_ready()) {
        fill_disconnected_state(out);
        return true;
    }

    const uint32_t now_ms = static_cast<uint32_t>(millis());
    const uint8_t r2_raw = ps4_calibration_scale_throttle(ctl->throttle());

    ps4_calibration_on_link_active();
    ps4_calibration_update(r2_raw, now_ms);

    const uint8_t r2_effective =
        ps4_calibration_is_ready() ? ps4_calibration_effective_from_raw(r2_raw) : 0U;
    const bool zero_rest =
        ps4_calibration_is_ready() && (ps4_calibration_get_rest() == 0U);
    const uint8_t r2_travel = ps4_calibration_travel_for_map(r2_raw, r2_effective);
    const bool throttle_active =
        ps4_calibration_throttle_active(r2_raw, r2_effective);

    const bool options_now = ctl->miscStart();
    const bool options_edge = options_now && !s_prev_options;
    const bool share_now = ctl->miscBack();
    const bool share_edge = share_now && !s_prev_share;

    s_prev_options = options_now;
    s_prev_share = share_now;

    ps4_bt_host_mark_active();

    out->connected = true;
    out->options_pressed = options_edge;
    out->share_pressed = share_edge;
    out->circle_pressed = ctl->b();
    out->r2_raw = r2_raw;
    out->r2_rest = ps4_calibration_is_ready() ? ps4_calibration_get_rest() : 255U;
    out->r2_effective = r2_travel;
    out->throttle_active = throttle_active;
    out->target_amps = ps4_calibration_map_travel_to_amps(r2_travel, zero_rest);
    out->target_rpm = ps4_calibration_map_travel_to_rpm(r2_travel, zero_rest);
    out->direction = out->circle_pressed ? -1 : 1;

    return true;
#endif /* BOARD_ENABLE_PS4_BT */
}
