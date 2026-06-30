/*
 * serial_hmi.h — HMI serial (monitor USB) substituindo PS4 na bancada.
 *
 * Produz ps4_input_state_t para reutilizar apply_ps4_to_esc() em main.cpp.
 */

#ifndef SERIAL_HMI_H
#define SERIAL_HMI_H

#include "ps4_input.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool serial_hmi_init(void);
bool serial_hmi_update(ps4_input_state_t *out);
void serial_hmi_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_HMI_H */
