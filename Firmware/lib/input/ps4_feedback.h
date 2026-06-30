/*
 * ps4_feedback.h — Lightbar DS4 deferida (output report após link estável).
 */

#ifndef PS4_FEEDBACK_H
#define PS4_FEEDBACK_H

#include "ps4_input.h"

#ifdef __cplusplus
extern "C" {
#endif

void ps4_feedback_reset(void);
void ps4_feedback_set_status(ps4_led_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* PS4_FEEDBACK_H */
