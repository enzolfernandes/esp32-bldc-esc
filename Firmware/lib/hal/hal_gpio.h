#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*hal_gpio_isr_cb_t)(void *arg);

bool hal_gpio_init(void);
bool hal_gpio_attach_oc_trip_isr(hal_gpio_isr_cb_t cb, void *arg);
void hal_gpio_detach_oc_trip_isr(void);
bool hal_gpio_oc_trip_asserted(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_GPIO_H
