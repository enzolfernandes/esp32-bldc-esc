#include "lm339_protection.h"

#include "hal_gpio.h"
#include "hal_pwm.h"

#include <stddef.h>

static lm339_fault_cb_t s_fault_cb = NULL;
static void *s_fault_arg = NULL;
static volatile bool s_fault_latched = false;
static bool s_initialized = false;

static void oc_trip_handler(void *arg)
{
    (void)arg;

    s_fault_latched = true;
    hal_pwm_set_armed(false);
    hal_pwm_disable_all();

    if (s_fault_cb != NULL) {
        s_fault_cb(s_fault_arg);
    }
}

bool lm339_protection_init(void)
{
    s_initialized = true;
    s_fault_latched = false;
    return true;
}

bool lm339_protection_arm(lm339_fault_cb_t cb, void *arg)
{
    if (!s_initialized) {
        return false;
    }

    s_fault_cb = cb;
    s_fault_arg = arg;

    return hal_gpio_attach_oc_trip_isr(oc_trip_handler, NULL);
}

void lm339_protection_disarm(void)
{
    hal_gpio_detach_oc_trip_isr();
    s_fault_cb = NULL;
    s_fault_arg = NULL;
}

bool lm339_protection_fault_active(void)
{
    return s_fault_latched || hal_gpio_oc_trip_asserted();
}

void lm339_protection_clear_fault(void)
{
    if (!hal_gpio_oc_trip_asserted()) {
        s_fault_latched = false;
    }
}
