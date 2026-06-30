/*
 * hal_gpio.c — GPIO: shutdown por fase (IR2110) e interrupção OC Trip (LM339).
 *
 * SD (GPIO 4/32/33): HIGH = shutdown (High-Z); LOW = driver segue HIN/LIN.
 * Pinos ADC2 usam RTC GPIO quando disponível.
 */

#include "hal_gpio.h"

#include "board_config.h"

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "soc/gpio_struct.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc.h"

#include <stddef.h>

static hal_gpio_isr_cb_t s_oc_trip_cb = NULL;
static void *s_oc_trip_arg = NULL;
static bool s_isr_attached = false;

static int phase_to_shutdown_pin(hal_pwm_phase_t phase)
{
    switch (phase) {
    case HAL_PWM_PHASE_A:
        return PIN_SHUTDOWN_A;
    case HAL_PWM_PHASE_B:
        return PIN_SHUTDOWN_B;
    case HAL_PWM_PHASE_C:
        return PIN_SHUTDOWN_C;
    default:
        return -1;
    }
}


/** Índice RTCIO (ESP32): GPIO4→10, GPIO32→9, GPIO33→8. */
static int sd_pin_to_rtc_num(int pin)
{
    switch (pin) {
    case 4:
        return 10;
    case 32:
        return 9;
    case 33:
        return 8;
    default:
        return -1;
    }
}

static void IRAM_ATTR sd_set_level_raw(int pin, int level)
{
    if (pin >= 32) {
        const uint32_t bit = 1U << (pin - 32);

        if (level != 0) {
            GPIO.out1_w1ts.val = bit;
        } else {
            GPIO.out1_w1tc.val = bit;
        }
        return;
    }

    const uint32_t bit = 1U << pin;

    if (level != 0) {
        GPIO.out_w1ts = bit;
    } else {
        GPIO.out_w1tc = bit;
    }
}

/** Saída SD via registradores RTCIO — válido em ISR (OCP). */
static void IRAM_ATTR sd_set_level_rtc_iram(int pin, int level)
{
    const int rtc_num = sd_pin_to_rtc_num(pin);

    if (rtc_num < 0) {
        sd_set_level_raw(pin, level);
        return;
    }

    if (level != 0) {
        REG_WRITE(RTC_GPIO_OUT_W1TS_REG, (1U << rtc_num));
    } else {
        REG_WRITE(RTC_GPIO_OUT_W1TC_REG, (1U << rtc_num));
    }
}

static bool sd_pin_is_rtc(int pin)
{
    return rtc_gpio_is_valid_gpio((gpio_num_t)pin);
}

static bool configure_shutdown_pin(int pin)
{
    const gpio_num_t gpio = (gpio_num_t)pin;

    gpio_reset_pin(gpio);

    if (!sd_pin_is_rtc(pin)) {
        return false;
    }

    esp_err_t err = rtc_gpio_init(gpio);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return false;
    }

    rtc_gpio_set_direction(gpio, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_pullup_dis(gpio);
    rtc_gpio_pulldown_dis(gpio);
    rtc_gpio_hold_dis(gpio);

    return true;
}

static void sd_set_level(int pin, int level)
{
    if (sd_pin_is_rtc(pin)) {
        rtc_gpio_set_level((gpio_num_t)pin, level);
    } else {
        sd_set_level_raw(pin, level);
    }
}

static void apply_sd_level(bool shutdown)
{
    const int level =
        shutdown ? IR2110_SD_SHUTDOWN_LEVEL : IR2110_SD_ENABLE_LEVEL;

    sd_set_level(PIN_SHUTDOWN_A, level);
    sd_set_level(PIN_SHUTDOWN_B, level);
    sd_set_level(PIN_SHUTDOWN_C, level);
}

static bool configure_all_shutdown_outputs(void)
{
    if (!configure_shutdown_pin(PIN_SHUTDOWN_A)) {
        return false;
    }
    if (!configure_shutdown_pin(PIN_SHUTDOWN_B)) {
        return false;
    }
    if (!configure_shutdown_pin(PIN_SHUTDOWN_C)) {
        return false;
    }

    return true;
}

static void IRAM_ATTR oc_trip_isr_handler(void *arg)
{
    (void)arg;

    if (s_oc_trip_cb != NULL) {
        s_oc_trip_cb(s_oc_trip_arg);
    }
}

bool hal_gpio_init(void)
{
    if (!configure_all_shutdown_outputs()) {
        return false;
    }

    apply_sd_level(true);

    gpio_config_t input_conf = {
        .pin_bit_mask = (1ULL << PIN_OC_TRIP),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&input_conf) == ESP_OK;
}

bool hal_gpio_reclaim_shutdown_outputs(void)
{
    if (!configure_all_shutdown_outputs()) {
        return false;
    }

    apply_sd_level(true);
    return true;
}

void hal_phase_shutdown_set(hal_pwm_phase_t phase, bool shutdown)
{
    const int pin = phase_to_shutdown_pin(phase);

    if (pin < 0) {
        return;
    }

    const int level =
        shutdown ? IR2110_SD_SHUTDOWN_LEVEL : IR2110_SD_ENABLE_LEVEL;

    sd_set_level(pin, level);
}

void hal_phase_shutdown_all(bool shutdown)
{
    const int level =
        shutdown ? IR2110_SD_SHUTDOWN_LEVEL : IR2110_SD_ENABLE_LEVEL;

    sd_set_level(PIN_SHUTDOWN_A, level);
    sd_set_level(PIN_SHUTDOWN_B, level);
    sd_set_level(PIN_SHUTDOWN_C, level);
}

void IRAM_ATTR hal_phase_shutdown_emergency(void)
{
    sd_set_level_rtc_iram(PIN_SHUTDOWN_A, IR2110_SD_SHUTDOWN_LEVEL);
    sd_set_level_rtc_iram(PIN_SHUTDOWN_B, IR2110_SD_SHUTDOWN_LEVEL);
    sd_set_level_rtc_iram(PIN_SHUTDOWN_C, IR2110_SD_SHUTDOWN_LEVEL);
}

bool hal_gpio_attach_oc_trip_isr(hal_gpio_isr_cb_t cb, void *arg)
{
    esp_err_t err;

    if (cb == NULL) {
        return false;
    }

    err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return false;
    }

    if (s_isr_attached) {
        gpio_isr_handler_remove(PIN_OC_TRIP);
        s_isr_attached = false;
    }

    s_oc_trip_cb = cb;
    s_oc_trip_arg = arg;

    err = gpio_set_intr_type(PIN_OC_TRIP, GPIO_INTR_NEGEDGE);
    if (err != ESP_OK) {
        return false;
    }

    err = gpio_isr_handler_add(PIN_OC_TRIP, oc_trip_isr_handler, NULL);
    if (err != ESP_OK) {
        return false;
    }

    s_isr_attached = true;

    return true;
}

void hal_gpio_detach_oc_trip_isr(void)
{
    if (s_isr_attached) {
        gpio_isr_handler_remove(PIN_OC_TRIP);
        s_isr_attached = false;
    }

    gpio_set_intr_type(PIN_OC_TRIP, GPIO_INTR_DISABLE);
    s_oc_trip_cb = NULL;
    s_oc_trip_arg = NULL;
}

bool hal_gpio_oc_trip_asserted(void)
{
    return gpio_get_level(PIN_OC_TRIP) == 0;
}
