#include <Arduino.h>

#include "battery_monitor.h"
#include "fsm_system.h"
#include "ina240_current_sensors.h"
#include "lm339_protection.h"

static void print_telemetry(void)
{
    Serial.printf("[%s] I: A=%+.2f  B=%+.2f  C=%+.2f A  VBAT=%.1f V\n",
                  fsm_system_state_name(fsm_system_get_state()),
                  ina240_read_amps(INA240_PHASE_A),
                  ina240_read_amps(INA240_PHASE_B),
                  ina240_read_amps(INA240_PHASE_C),
                  battery_monitor_read_volts());
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
    default:
        break;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println("\n--- ESC BLDC: FSM ---");
    Serial.println("Comandos: a=arm  d=disarm  c=clear fault  s=status");

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
    print_telemetry();
    delay(1000);
}
