/*
 * main.cpp — Ponto de entrada Arduino: setup(), loop() e orquestração do ESC.
 *
 * Camada: aplicação. Cadências no loop():
 *   - Contínuo: battery_monitor_tick, fsm_system_tick
 *   - 20 ms: polling PS4 e tradução para arm/disarm/setpoints
 *   - 500 ms: telemetria serial (somente leitura, 115200 baud)
 *
 * A malha de controle (1 kHz) executa em esp_timer dentro de motor_control — não no loop().
 */

#include <Arduino.h>

#include "battery_monitor.h"
#include "board_config.h"
#include "fsm_system.h"
#include "ina240_current_sensors.h"
#include "motor_control.h"
#include "ps4_input.h"

static uint32_t s_last_telemetry_ms = 0;
static bool s_require_r2_release = false;

static bool is_run_phase_for_telemetry(motor_start_phase_t phase)
{
    return phase == MOTOR_START_RUN || phase == MOTOR_START_RUN_OPEN ||
           phase == MOTOR_START_RUN_SPEED;
}

/** Imprime diagnóstico na Serial: estado FSM, correntes, VBAT, RPM/duty em RUNNING. */
static void print_telemetry(const ps4_input_state_t *ps4)
{
    Serial.printf("[%s] BT=%s  R2=%u  bolinha=%u",
                  fsm_system_state_name(fsm_system_get_state()),
                  (ps4 != nullptr && ps4->connected) ? "OK" : "OFF",
                  (ps4 != nullptr) ? static_cast<unsigned>(ps4->r2_raw) : 0U,
                  (ps4 != nullptr && ps4->circle_pressed) ? 1U : 0U);

    Serial.printf("  I: A=%+.2f  B=%+.2f  C=%+.2f A  VBAT=%.1f V  pack=%uS  uvlo=%s",
                  ina240_read_amps(INA240_PHASE_A),
                  ina240_read_amps(INA240_PHASE_B),
                  ina240_read_amps(INA240_PHASE_C),
                  battery_monitor_get_volts_filtered(),
                  static_cast<unsigned>(battery_monitor_get_cell_count_s()),
                  battery_monitor_uvlo_active() ? "ATIVO" : "OK");

    if (fsm_system_get_state() == ESC_STATE_FAULT &&
        motor_control_get_last_fault_reason() != MOTOR_FAULT_NONE) {
        Serial.printf("  falha=%s",
                      motor_control_fault_reason_name(
                          motor_control_get_last_fault_reason()));
    }

    if (fsm_system_get_state() == ESC_STATE_RUNNING) {
        Serial.printf("  mode=%s  dir=%s  start=%s",
                      motor_control_control_mode_name(
                          motor_control_get_control_mode()),
                      motor_control_direction_name(motor_control_get_direction()),
                      motor_control_start_phase_name(motor_control_get_start_phase()));

#if MOTOR_CONTROL_DEFAULT_MODE == MOTOR_CONTROL_MODE_SPEED
        Serial.printf("  RPM=%.0f/%.0f(%.0f)",
                      motor_control_get_measured_rpm(),
                      motor_control_get_target_rpm(),
                      motor_control_get_target_command_rpm());
#endif

        Serial.printf("  I=%.2f/%.2f(%.2f)A  duty=%.1f%%  step=%u  comm=%s",
                      motor_control_get_measured_amps(),
                      motor_control_get_target_amps(),
                      motor_control_get_target_command_amps(),
                      motor_control_get_duty_percent(),
                      static_cast<unsigned>(motor_control_get_commutation_step()),
                      motor_control_comm_mode_name(motor_control_get_comm_mode()));

        if (is_run_phase_for_telemetry(motor_control_get_start_phase()) &&
            motor_control_get_comm_mode() == MOTOR_COMM_OPEN_LOOP) {
            Serial.printf("  f_el=%.1f Hz", motor_control_get_open_loop_comm_hz());
        }

        Serial.printf("  Kp=%.2f Ki=%.1f",
                      motor_control_get_pi_kp(),
                      motor_control_get_pi_ki());
    }

    if (s_require_r2_release) {
        Serial.print("  aguardando_R2=0");
    }

    Serial.println();
}

static ps4_led_status_t led_status_from_fsm(bool connected, esc_state_t state)
{
    if (!connected) {
        return PS4_LED_OFF;
    }

    switch (state) {
    case ESC_STATE_INIT:
        return PS4_LED_INIT;
    case ESC_STATE_IDLE:
        return PS4_LED_IDLE;
    case ESC_STATE_RUNNING:
        return PS4_LED_RUNNING;
    case ESC_STATE_FAULT:
        return PS4_LED_FAULT;
    default:
        return PS4_LED_IDLE;
    }
}

/**
 * @brief Traduz estado do PS4 em requisições para FSM e motor_control.
 * Máquina de decisão do operador — ver etapas numeradas abaixo.
 */
static void apply_ps4_to_esc(const ps4_input_state_t *st)
{
    if (st == nullptr) {
        return;
    }

    // Etapa 1: perda de Bluetooth — desarme imediato se em RUNNING
    if (!st->connected) {
        if (fsm_system_get_state() == ESC_STATE_RUNNING) {
            fsm_system_request_disarm();
        }
        return;
    }

    // Etapa 2: Options (borda) em FAULT — clear fault; exige soltar R2 antes de re-armar
    if (st->options_pressed && fsm_system_get_state() == ESC_STATE_FAULT) {
        if (fsm_system_clear_fault()) {
            s_require_r2_release = true;
        }
        return;
    }

    // Etapa 3: em FAULT sem clear — ignora demais entradas
    if (fsm_system_get_state() == ESC_STATE_FAULT) {
        return;
    }

    // Etapa 4: após clear fault, aguarda R2 em zero antes de aceitar novo arm
    if (s_require_r2_release) {
        if (st->r2_raw > 0U) {
            return;
        }

        s_require_r2_release = false;
    }

    // Etapa 5: R2 solto (≤ limiar) — desarm, zera setpoint, permite trocar sentido (Circle)
    if (st->r2_raw <= PS4_R2_ARM_THRESHOLD) {
        if (fsm_system_get_state() == ESC_STATE_RUNNING) {
            fsm_system_request_disarm();
        }

        motor_control_set_target_amps(0.0f);
        motor_control_set_target_rpm(0.0f);
        motor_control_set_direction(st->direction);
        return;
    }

    // Etapa 6: R2 pressionado em IDLE — tenta arm (recusado se UVLO ou OCP ativo)
    if (fsm_system_get_state() == ESC_STATE_IDLE) {
        if (!fsm_system_request_arm()) {
            return;
        }
    }

    // Etapa 7: só aplica setpoint se efetivamente em RUNNING
    if (fsm_system_get_state() != ESC_STATE_RUNNING) {
        return;
    }

    motor_control_set_direction(st->direction);

#if MOTOR_CONTROL_DEFAULT_MODE == MOTOR_CONTROL_MODE_SPEED
    motor_control_set_target_rpm(st->target_rpm);
#else
    motor_control_set_target_amps(st->target_amps);
#endif
}

/** Boot: Serial, PS4, init da FSM (HAL, drivers, timer 1 kHz). */
void setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println("\n--- ESC BLDC: controle PS4 (Bluepad32) ---");
    Serial.println("Serial: telemetria somente leitura.");
    Serial.println("Pairing PS4: Share + PS ate LED piscar; ESP32 escaneia automaticamente.");
#if BOARD_ENABLE_BEMF_ZCD
    Serial.println("ZCD BEMF: habilitado (handover OPEN->ZCD se hardware OK)");
#else
    Serial.println("ZCD BEMF: desabilitado — comutacao malha aberta com rampa");
#endif
    Serial.printf("Controle: mode=%s  R2=%s | Bolinha=CCW | Options=clear fault\n",
                  motor_control_control_mode_name(motor_control_get_control_mode()),
#if MOTOR_CONTROL_DEFAULT_MODE == MOTOR_CONTROL_MODE_SPEED
                  "RPM");
#else
                  "corrente");
#endif

    if (!ps4_input_init()) {
        Serial.println("ps4_input_init: FALHA");
    }

    if (!fsm_system_init()) {
        Serial.println("fsm_system_init: FAULT");
    } else {
        Serial.printf("fsm_system_init: %s\n",
                      fsm_system_state_name(fsm_system_get_state()));
        Serial.printf("Pack LiPo: %uS (auto)  UVLO cutoff=%.1f V  recover=%.1f V\n",
                      static_cast<unsigned>(battery_monitor_get_cell_count_s()),
                      battery_monitor_get_uvlo_cutoff_v(),
                      battery_monitor_get_uvlo_recover_v());
#if MOTOR_CONTROL_DEFAULT_MODE == MOTOR_CONTROL_MODE_SPEED
        Serial.printf("Velocidade: max %.0f RPM  handover OPEN %.0f RPM\n",
                      MOTOR_SPEED_MAX_RPM,
                      MOTOR_SPEED_OPEN_LOOP_HANDOVER_RPM);
#endif
        Serial.printf("INA240 offset: A=%.0f  B=%.0f  C=%.0f mV\n",
                      ina240_get_offset_mv(INA240_PHASE_A),
                      ina240_get_offset_mv(INA240_PHASE_B),
                      ina240_get_offset_mv(INA240_PHASE_C));
    }
}

/**
 * @brief Loop principal Arduino (~20 ms efetivo para PS4; jitter tolerável).
 * Supervisão e entrada aqui; malha 1 kHz no esp_timer (motor_control_tick).
 */
void loop()
{
    battery_monitor_tick(millis());
    fsm_system_tick();

    static uint32_t last_poll_ms = 0;
    static ps4_input_state_t ps4 = {};

    const uint32_t now_ms = millis();

    if ((now_ms - last_poll_ms) >= PS4_INPUT_POLL_MS) {
        last_poll_ms = now_ms;

        if (ps4_input_update(&ps4)) {
            apply_ps4_to_esc(&ps4);

            ps4_input_set_led_status(
                led_status_from_fsm(ps4.connected, fsm_system_get_state()));
        }
    }

    if ((now_ms - s_last_telemetry_ms) >= 500U) {
        s_last_telemetry_ms = now_ms;
        print_telemetry(&ps4);
    }
}
