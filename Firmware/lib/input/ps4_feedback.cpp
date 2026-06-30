/*
 * ps4_feedback.cpp — Lightbar deferida: output report só após link READY + delay.
 */

#include "ps4_feedback.h"

#include "board_config.h"
#include "ps4_bt_host.h"

#include <Arduino.h>
#include <Bluepad32.h>

#ifndef PS4_FEEDBACK_OUTPUT_DELAY_MS
#define PS4_FEEDBACK_OUTPUT_DELAY_MS 500U
#endif

static ps4_led_status_t s_last_led_status = PS4_LED_OFF;

static void apply_led_color(ControllerPtr ctl, ps4_led_status_t status)
{
#if PS4_SKIP_LIGHTBAR_ON_CLONE
    (void)ctl;
    (void)status;
#else
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
#endif
}


extern "C" void ps4_feedback_reset(void)
{
    s_last_led_status = PS4_LED_OFF;
}

extern "C" void ps4_feedback_set_status(ps4_led_status_t status)
{
    if (!ps4_bt_host_is_ready()) {
        return;
    }

    const uint32_t now_ms = static_cast<uint32_t>(millis());
    const uint32_t connect_ms = ps4_bt_host_connect_ms();

    if (connect_ms != 0U && (now_ms - connect_ms) < PS4_FEEDBACK_OUTPUT_DELAY_MS) {
        return;
    }

    if (status == s_last_led_status) {
        return;
    }

    ControllerPtr ctl = ps4_bt_host_first_gamepad();
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
