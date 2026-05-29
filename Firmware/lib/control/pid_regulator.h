#ifndef PID_REGULATOR_H
#define PID_REGULATOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float kp;
    float ki;
    float dt;             // [s]
    float integral_term;  // integrator state
    float out_max;        // output saturation max
    float out_min;        // output saturation min
    float integ_max;      // anti-windup clamp max
    float integ_min;      // anti-windup clamp min
} pi_controller_t;

float pi_compute(pi_controller_t *pi, float setpoint, float measurement);

#ifdef __cplusplus
}
#endif

#endif // PID_REGULATOR_H
