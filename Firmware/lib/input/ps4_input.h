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

bool ps4_input_init(void);
bool ps4_input_update(ps4_input_state_t *out);
bool ps4_input_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // PS4_INPUT_H
