#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "hal_pwm.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*hal_gpio_isr_cb_t)(void *arg);

bool hal_gpio_init(void);
/** Reconfigura SD após BT/Arduino. Fail-safe: todos SD=HIGH (shutdown). */
bool hal_gpio_reclaim_shutdown_outputs(void);

/** true = SD HIGH (HO/LO High-Z); false = SD LOW (driver ativo). */
void hal_phase_shutdown_set(hal_pwm_phase_t phase, bool shutdown);
/** Aplica shutdown (HIGH) ou enable (LOW) nas três fases. */
void hal_phase_shutdown_all(bool shutdown);
/** ISR OCP: força SD A/B/C → HIGH via latch GPIO (IRAM). */
void hal_phase_shutdown_emergency(void);

bool hal_gpio_attach_oc_trip_isr(hal_gpio_isr_cb_t cb, void *arg);
void hal_gpio_detach_oc_trip_isr(void);
bool hal_gpio_oc_trip_asserted(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_GPIO_H
