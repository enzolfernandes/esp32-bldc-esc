#ifndef FSM_SYSTEM_H
#define FSM_SYSTEM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESC_STATE_INIT = 0,
    ESC_STATE_IDLE,
    ESC_STATE_RUNNING,
    ESC_STATE_FAULT
} esc_state_t;

bool fsm_system_init(void);
void fsm_system_tick(void);

esc_state_t fsm_system_get_state(void);
const char *fsm_system_state_name(esc_state_t state);

bool fsm_system_request_arm(void);
bool fsm_system_request_disarm(void);
bool fsm_system_clear_fault(void);

#ifdef __cplusplus
}
#endif

#endif // FSM_SYSTEM_H
