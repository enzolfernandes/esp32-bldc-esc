/*
 * ps4_input.h — Estado normalizado do DualShock 4 (Bluepad32).
 */

#ifndef PS4_INPUT_H
#define PS4_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Snapshot de uma leitura do controle; preenchido por ps4_input_update(). */
typedef struct {
    bool connected;
    bool options_pressed;  /**< Borda de subida do botão Options (clear fault) */
    bool circle_pressed;     /**< Circle (○): CCW quando pressionado */
    uint8_t r2_raw;          /**< Gatilho direito 0–255 após deadzone */
    float target_amps;       /**< Mapeamento linear para modo CURRENT */
    float target_rpm;        /**< Mapeamento linear para modo SPEED */
    int8_t direction;        /**< +1 CW, -1 CCW */
} ps4_input_state_t;

typedef enum {
    PS4_LED_OFF = 0,
    PS4_LED_INIT,      // amarelo — inicialização
    PS4_LED_IDLE,      // azul — pronto, aguardando R2
    PS4_LED_RUNNING,   // verde — motor armado/ativo
    PS4_LED_FAULT      // vermelho — falha
} ps4_led_status_t;

bool ps4_input_init(void);
bool ps4_input_update(ps4_input_state_t *out);
bool ps4_input_is_connected(void);
void ps4_input_set_led_status(ps4_led_status_t status);

#ifdef __cplusplus
}
#endif

#endif // PS4_INPUT_H
