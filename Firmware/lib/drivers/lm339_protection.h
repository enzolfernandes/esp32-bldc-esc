#ifndef LM339_PROTECTION_H
#define LM339_PROTECTION_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*lm339_fault_cb_t)(void *arg);

bool lm339_protection_init(void);
bool lm339_protection_set_oc_threshold_amps(float amps);
bool lm339_protection_arm(lm339_fault_cb_t cb, void *arg);
void lm339_protection_disarm(void);
bool lm339_protection_fault_active(void);
void lm339_protection_clear_fault(void);

#ifdef __cplusplus
}
#endif

#endif // LM339_PROTECTION_H
