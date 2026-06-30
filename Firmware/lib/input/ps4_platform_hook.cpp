/*
 * ps4_platform_hook.cpp — Ignora touchpad/mouse DS4 em on_device_ready (idx 0 vs -1).
 *
 * DS4 original expõe gamepad + mouse virtual; com virt=0 o segundo ready usa idx=0
 * e a lib Arduino imprime "got: 0, want: -1" → Platform declined controller.
 */

#include "ps4_platform_hook.h"

#include "board_config.h"

extern "C" {
#include "arduino_platform.h"
#include "controller/uni_controller.h"
#include "platform/uni_platform.h"
#include "uni_error.h"
#include "uni_hid_device.h"
}

static uni_error_t (*s_orig_on_device_ready)(uni_hid_device_t *d) = nullptr;

static uni_error_t ps4_wrapped_on_device_ready(uni_hid_device_t *d)
{
    if (d == nullptr) {
        return UNI_ERROR_INVALID_DEVICE;
    }

    const uni_controller_class_t klass = d->controller.klass;
    const bool is_child = (d->parent != nullptr);

    if (is_child || klass != UNI_CONTROLLER_CLASS_GAMEPAD) {
        return UNI_ERROR_IGNORE_DEVICE;
    }

    return s_orig_on_device_ready(d);
}

extern "C" void ps4_platform_hook_install(void)
{
    struct uni_platform *plat = get_arduino_platform();

    if (plat == nullptr || plat->on_device_ready == nullptr) {
        return;
    }

    if (plat->on_device_ready == ps4_wrapped_on_device_ready) {
        return;
    }

    s_orig_on_device_ready = plat->on_device_ready;
    plat->on_device_ready = ps4_wrapped_on_device_ready;
}
