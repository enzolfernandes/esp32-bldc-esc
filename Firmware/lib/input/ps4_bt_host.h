/*
 * ps4_bt_host.h — Lifecycle Bluepad32 e máquina de estados do link BT.
 */

#ifndef PS4_BT_HOST_H
#define PS4_BT_HOST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PS4_LINK_OFF = 0,
    PS4_LINK_PAIRING,
    PS4_LINK_READY,
    PS4_LINK_ACTIVE
} ps4_link_state_t;

bool ps4_bt_host_init(void);
void ps4_bt_host_poll(void);
ps4_link_state_t ps4_bt_host_get_link_state(void);
const char *ps4_bt_host_link_state_name(ps4_link_state_t state);
bool ps4_bt_host_is_ready(void);
void *ps4_bt_host_get_gamepad(void);
uint32_t ps4_bt_host_connect_ms(void);
void ps4_bt_host_mark_active(void);

#ifdef __cplusplus
}
#include <Bluepad32.h>
ControllerPtr ps4_bt_host_first_gamepad(void);
#endif

#endif /* PS4_BT_HOST_H */
