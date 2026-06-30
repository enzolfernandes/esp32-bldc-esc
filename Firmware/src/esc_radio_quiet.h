#ifndef ESC_RADIO_QUIET_H
#define ESC_RADIO_QUIET_H

#ifdef __cplusplus
extern "C" {
#endif

/** Desliga BT/BLE quando BOARD_ENABLE_PS4_BT=0 (no-op se PS4 ativo). */
void esc_radio_quiet_init(void);

#ifdef __cplusplus
}
#endif

#endif
