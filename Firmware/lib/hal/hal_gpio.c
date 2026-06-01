#include "hal_gpio.h"

#include "board_config.h"

#include "driver/gpio.h"
#include "esp_err.h"

static hal_gpio_isr_cb_t s_oc_trip_cb = NULL;
static void *s_oc_trip_arg = NULL;
static bool s_isr_attached = false;

static void IRAM_ATTR oc_trip_isr_handler(void *arg)
{
    (void)arg;

    if (s_oc_trip_cb != NULL) {
        s_oc_trip_cb(s_oc_trip_arg);
    }
}

bool hal_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_OC_TRIP),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&io_conf) == ESP_OK;
}

bool hal_gpio_attach_oc_trip_isr(hal_gpio_isr_cb_t cb, void *arg)
{
    esp_err_t err;

    if (cb == NULL) {
        return false;
    }

    err = gpio_install_isr_service(0);
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
