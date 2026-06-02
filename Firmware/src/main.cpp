#include <Arduino.h>

#include "battery_monitor.h"
#include "fsm_system.h"
#include "ina240_current_sensors.h"
#include "lm339_protection.h"
#include "motor_control.h"

static uint32_t s_last_telemetry_ms = 0;

static void print_telemetry(void)
{
    Serial.printf("[%s] I: A=%+.2f  B=%+.2f  C=%+.2f A  VBAT=%.1f V",
                  fsm_system_state_name(fsm_system_get_state()),
                  ina240_read_amps(INA240_PHASE_A),
                  ina240_read_amps(INA240_PHASE_B),
                  ina240_read_amps(INA240_PHASE_C),
                  battery_monitor_read_volts());

    if (fsm_system_get_state() == ESC_STATE_RUNNING) {
        Serial.printf("  I*=%.2f/%.2f A  duty=%.1f%%  step=%u  comm=%s",
                      motor_control_get_measured_amps(),
                      motor_control_get_target_amps(),
                      motor_control_get_duty_percent(),
                      static_cast<unsigned>(motor_control_get_commutation_step()),
                      motor_control_comm_mode_name(motor_control_get_comm_mode()));
    }

    Serial.println();
}

static void handle_serial_command(void)
{
    if (!Serial.available()) {
        return;
    }

    const char cmd = static_cast<char>(Serial.read());

    switch (cmd) {
    case 'a':
    case 'A':
        Serial.println(fsm_system_request_arm() ? "ARM OK" : "ARM recusado");
        break;
    case 'd':
    case 'D':
        Serial.println(fsm_system_request_disarm() ? "DISARM OK" : "DISARM recusado");
        break;
    case 'c':
    case 'C':
        Serial.println(fsm_system_clear_fault() ? "FAULT limpo -> IDLE" : "CLEAR recusado");
        break;
    case 's':
    case 'S':
        Serial.printf("Estado: %s\n", fsm_system_state_name(fsm_system_get_state()));
        break;
    case 'o':
    case 'O':
        motor_control_force_open_loop();
        Serial.println("Comutacao: OPEN (malha aberta)");
        break;
    case 't':
    case 'T': {
        if (fsm_system_get_state() != ESC_STATE_RUNNING) {
            Serial.println("Defina corrente apenas em RUNNING (comando a)");
            while (Serial.available()) {
                Serial.read();
            }
            break;
        }

        const float amps = Serial.parseFloat();
        motor_control_set_target_amps(amps);
        Serial.printf("I_alvo=%.2f A (max %.1f)\n",
                      motor_control_get_target_amps(),
                      MOTOR_CONTROL_MAX_TARGET_AMPS);
        break;
    }
    default:
        break;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println("\n--- ESC BLDC: FSM + motor_control ---");
    Serial.println("Comandos: a=arm  d=disarm  c=clear fault  s=status");
    Serial.println("         t<amps>=corrente alvo (ex.: t1.5), so em RUNNING");
    Serial.println("         o=forca malha aberta (OPEN)");

    if (!fsm_system_init()) {
        Serial.println("fsm_system_init: FAULT");
    } else {
        Serial.printf("fsm_system_init: %s\n",
                      fsm_system_state_name(fsm_system_get_state()));
        Serial.printf("INA240 offset: A=%.0f  B=%.0f  C=%.0f mV\n",
                      ina240_get_offset_mv(INA240_PHASE_A),
                      ina240_get_offset_mv(INA240_PHASE_B),
                      ina240_get_offset_mv(INA240_PHASE_C));
    }
}

void loop()
{
    fsm_system_tick();
    handle_serial_command();

    const uint32_t now_ms = millis();

    if ((now_ms - s_last_telemetry_ms) >= 500U) {
        s_last_telemetry_ms = now_ms;
        print_telemetry();
    }
}
