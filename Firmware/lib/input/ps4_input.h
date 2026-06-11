#ifndef PS4_INPUT_H
#define PS4_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool connected;
    bool options_pressed;
    bool circle_pressed;
    uint8_t r2_raw;
    float target_amps;
    float target_rpm;
    int8_t direction;
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
