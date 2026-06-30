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
#include <esp_system.h>   /* esp_get_free_heap_size() — Sub-teste 5.2 */

#include "battery_monitor.h"
#include "board_config.h"
#include "esc_radio_quiet.h"
#include "fsm_system.h"
#include "hal_motor.h"
#include "ina240_current_sensors.h"
#include "motor_control.h"
#include "ps4_input.h"
#if BOARD_ENABLE_PS4_BT
#include "ps4_bt_host.h"
#endif
#if BOARD_ENABLE_SERIAL_HMI
#include "serial_hmi.h"
#endif
#include "wifi_telemetry.h"

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
#if BOARD_ENABLE_SERIAL_HMI
    Serial.printf("[%s] HMI=SER  R2=%u(rest=%u eff=%u)",
                  fsm_system_state_name(fsm_system_get_state()),
                  (ps4 != nullptr) ? static_cast<unsigned>(ps4->r2_raw) : 0U,
                  (ps4 != nullptr) ? static_cast<unsigned>(ps4->r2_rest) : 0U,
                  (ps4 != nullptr) ? static_cast<unsigned>(ps4->r2_effective) : 0U);
#elif BOARD_ENABLE_PS4_BT
    Serial.printf("[%s] BT=%s link=%s  R2=%u(rest=%u eff=%u)",
                  fsm_system_state_name(fsm_system_get_state()),
                  (ps4 != nullptr && ps4->connected) ? "ON" : "OFF",
                  ps4_bt_host_link_state_name(ps4_bt_host_get_link_state()),
                  (ps4 != nullptr) ? static_cast<unsigned>(ps4->r2_raw) : 0U,
                  (ps4 != nullptr) ? static_cast<unsigned>(ps4->r2_rest) : 0U,
                  (ps4 != nullptr) ? static_cast<unsigned>(ps4->r2_effective) : 0U);
#else
    Serial.printf("[%s] HMI=OFF  R2=%u(rest=%u eff=%u)",
                  fsm_system_state_name(fsm_system_get_state()),
                  (ps4 != nullptr) ? static_cast<unsigned>(ps4->r2_raw) : 0U,
                  (ps4 != nullptr) ? static_cast<unsigned>(ps4->r2_rest) : 0U,
                  (ps4 != nullptr) ? static_cast<unsigned>(ps4->r2_effective) : 0U);
#endif
    Serial.printf("  bolinha=%u",
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

        /* Sub-teste 5.1: latência do tick em µs (last / min / max desde o boot) */
        Serial.printf("  tick=%lu/%lu/%lu us",
                      (unsigned long)motor_control_get_tick_latency_us(),
                      (unsigned long)motor_control_get_tick_latency_min_us(),
                      (unsigned long)motor_control_get_tick_latency_max_us());

        if (!hal_motor_is_armed()) {
            Serial.print("  PWM=DESARM");
        }
    }

    /* Sub-teste 5.2: heap livre do FreeRTOS em bytes */
    Serial.printf("  heap=%lu B", (unsigned long)esp_get_free_heap_size());

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
            s_require_r2_release = true;
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

    // Etapa 2b: Share (borda) — desarme imediato em RUNNING (fallback se R2 colado)
    if (st->share_pressed && fsm_system_get_state() == ESC_STATE_RUNNING) {
        fsm_system_request_disarm();
        s_require_r2_release = true;
        motor_control_set_target_amps(0.0f);
        motor_control_set_target_rpm(0.0f);
        return;
    }

    // Etapa 3: em FAULT sem clear — ignora demais entradas
    if (fsm_system_get_state() == ESC_STATE_FAULT) {
        return;
    }

    // Etapa 4: após clear fault ou desarme, aguarda gatilho solto antes de re-armar
    if (s_require_r2_release) {
        if (st->throttle_active) {
            return;
        }

        s_require_r2_release = false;
    }

    // Etapa 5: gatilho solto (pós-cal) — desarm e zera setpoint
    if (!st->throttle_active) {
        if (fsm_system_get_state() == ESC_STATE_RUNNING) {
            fsm_system_request_disarm();
            s_require_r2_release = true;
        }

        motor_control_set_target_amps(0.0f);
        motor_control_set_target_rpm(0.0f);
        motor_control_set_direction(st->direction);
        return;
    }

    // Etapa 6: aguarda calibração R2 e pressão do gatilho para arm em IDLE
    if (fsm_system_get_state() == ESC_STATE_IDLE) {
#if !BOARD_ENABLE_SERIAL_HMI
        if (!ps4_input_r2_calibrated()) {
            return;
        }
#endif

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

/**
 * @brief Serializa o estado atual do ESC como JSON e envia via WebSocket.
 *
 * Campos do JSON (chaves curtas para minimizar bytes over-the-air):
 *   t=millis, ia/ib/ic=correntes fase, im=I medida, it=I alvo, itc=I cmd,
 *   d=duty%, v=VBAT, rpm=RPM medido, rpmt=RPM alvo, rpmtc=RPM cmd,
 *   fel=f elétrica Hz, kp/ki=ganhos PI, lat/lmin/lmax=latência tick µs,
 *   heap=heap livre bytes, state=FSM(0-3), phase=partida(0-4),
 *   step=passo 6-step, cmode=modo comutação(0=OPEN,1=ZCD),
 *   uvlo=bool, ps4c=PS4 conectado, r2=R2%, circle=bool, fault=string.
 */
static void push_wifi_telemetry_full(const ps4_input_state_t *ps4)
{
    if (wifi_telemetry_client_count() == 0) {
        return;
    }

    const esc_state_t state = fsm_system_get_state();
    const bool running = (state == ESC_STATE_RUNNING);
    const bool connected = (ps4 != nullptr && ps4->connected);

#if WIFI_TELEMETRY_DEFER_IN_RUNNING
    const bool batch_ready = wifi_telemetry_running_batch_ready();
#else
    const bool batch_ready = false;
#endif

    char json[420];
    snprintf(json, sizeof(json),
        "{"
        "\"t\":%lu,"
        "\"ia\":%.2f,\"ib\":%.2f,\"ic\":%.2f,"
        "\"im\":%.2f,\"it\":%.2f,\"itc\":%.2f,"
        "\"d\":%.1f,\"v\":%.2f,"
        "\"rpm\":%.0f,\"rpmt\":%.0f,\"rpmtc\":%.0f,"
        "\"fel\":%.1f,"
        "\"kp\":%.2f,\"ki\":%.1f,"
        "\"lat\":%lu,\"lmin\":%lu,\"lmax\":%lu,"
        "\"heap\":%lu,"
        "\"state\":%d,\"phase\":%d,\"step\":%d,\"cmode\":%d,"
        "\"uvlo\":%s,"
        "\"ps4c\":%s,\"r2\":%u,\"circle\":%s,"
        "\"fault\":\"%s\""
#if WIFI_TELEMETRY_DEFER_IN_RUNNING
        ",\"batch_ready\":%s"
#endif
        "}",
        (unsigned long)millis(),
        ina240_read_amps(INA240_PHASE_A),
        ina240_read_amps(INA240_PHASE_B),
        ina240_read_amps(INA240_PHASE_C),
        running ? motor_control_get_measured_amps()      : 0.0f,
        running ? motor_control_get_target_amps()        : 0.0f,
        running ? motor_control_get_target_command_amps(): 0.0f,
        running ? motor_control_get_duty_percent()       : 0.0f,
        battery_monitor_get_volts_filtered(),
        running ? motor_control_get_measured_rpm()       : 0.0f,
        running ? motor_control_get_target_rpm()         : 0.0f,
        running ? motor_control_get_target_command_rpm() : 0.0f,
        running ? motor_control_get_open_loop_comm_hz()  : 0.0f,
        motor_control_get_pi_kp(),
        motor_control_get_pi_ki(),
        (unsigned long)motor_control_get_tick_latency_us(),
        (unsigned long)motor_control_get_tick_latency_min_us(),
        (unsigned long)motor_control_get_tick_latency_max_us(),
        (unsigned long)esp_get_free_heap_size(),
        static_cast<int>(state),
        static_cast<int>(motor_control_get_start_phase()),
        static_cast<int>(motor_control_get_commutation_step()),
        static_cast<int>(motor_control_get_comm_mode()),
        battery_monitor_uvlo_active() ? "true" : "false",
        connected ? "true" : "false",
        connected ? static_cast<unsigned>(ps4->r2_raw * 100U / 255U) : 0U,
        (connected && ps4->circle_pressed) ? "true" : "false",
        (state == ESC_STATE_FAULT)
            ? motor_control_fault_reason_name(motor_control_get_last_fault_reason())
            : ""
#if WIFI_TELEMETRY_DEFER_IN_RUNNING
        , batch_ready ? "true" : "false"
#endif
    );

    wifi_telemetry_push(json);
}

#if BOARD_ENABLE_WIFI_TELEMETRY && !WIFI_TELEMETRY_DEFER_IN_RUNNING
static void push_wifi_telemetry(const ps4_input_state_t *ps4)
{
    push_wifi_telemetry_full(ps4);
}
#endif

/** Boot: Serial, PS4, init da FSM (HAL, drivers, timer 1 kHz). */
void setup()
{
    /* Segurança de potência antes de Wi-Fi/BT: SD em shutdown e pinos PWM em GPIO LOW. */
    (void)hal_motor_init();

    Serial.begin(115200);
    delay(100);

    esc_radio_quiet_init();

    Serial.println("\n--- ESC BLDC: bancada inversor ---");
#if BOARD_ENABLE_SERIAL_HMI
    Serial.println("HMI: Serial USB (monitor 115200).");
    Serial.println("  A/a = arm/disarm   + = sobe setpoint   - = desce setpoint   espaco = e-stop");
    Serial.println("  c/C = clear fault (sai de FAULT para IDLE)");
#elif BOARD_ENABLE_PS4_BT
    Serial.println("--- ESC BLDC: controle PS4 (Bluepad32) ---");
    Serial.println("Pairing PS4: Share + PS ate LED piscar; ESP32 escaneia automaticamente.");
    Serial.printf("R2: aguarde cal (2 s em repouso, R2=0) antes de pressionar.\n");
#endif
    Serial.println("Serial: telemetria somente leitura.");
#if BOARD_ENABLE_BEMF_ZCD
    Serial.println("ZCD BEMF: habilitado (handover OPEN->ZCD se hardware OK)");
#else
    Serial.println("ZCD BEMF: desabilitado — comutacao malha aberta com rampa");
#endif
#if MOTOR_SWAP_PHASES_BC
    Serial.println("Motor: MOTOR_SWAP_PHASES_BC=1 (fases B/C trocadas)");
#endif
#if MOTOR_BENCH_FIX_RUN_OPEN_F_EL
    Serial.printf("Bancada: RUN_OPEN f_el fixo %.1f Hz (~%.0f RPM) — R2 arma/desarma\n",
                  (double)MOTOR_BENCH_RUN_OPEN_F_EL_HZ,
                  (double)(MOTOR_BENCH_RUN_OPEN_F_EL_HZ * 60.0f /
                           (float)MOTOR_POLE_PAIRS));
#endif
    Serial.printf("Controle: mode=%s  setpoint=%s",
                  motor_control_control_mode_name(motor_control_get_control_mode()),
#if MOTOR_CONTROL_DEFAULT_MODE == MOTOR_CONTROL_MODE_SPEED
                  "RPM");
#else
                  "corrente");
#endif
#if BOARD_ENABLE_PS4_BT
    Serial.println(" | Bolinha=CCW | Share=desarm | Options=clear fault");
#else
    Serial.println("");
#endif

    /* Wi-Fi ANTES do Bluetooth: o ESP-IDF exige que o modo Wi-Fi seja configurado
     * antes de o stack BT (Bluepad32) alocar o coexistence scheduler de rádio. */
#if BOARD_ENABLE_WIFI_TELEMETRY
    if (!wifi_telemetry_init()) {
        Serial.println("[WiFi] Dashboard desabilitado.");
    }
#else
    Serial.println("[WiFi] Dashboard desligado (BOARD_ENABLE_WIFI_TELEMETRY=0).");
#endif

#if BOARD_ENABLE_PS4_BT
    if (!ps4_input_init()) {
        Serial.println("[PS4] init: FALHA");
    }
#else
    Serial.println("[PS4] Bluetooth desligado (BOARD_ENABLE_PS4_BT=0).");
#endif

#if BOARD_ENABLE_SERIAL_HMI
    if (!serial_hmi_init()) {
        Serial.println("[Serial HMI] init: FALHA");
    } else {
        Serial.println("[Serial HMI] ativo (Core 0, poll 20 ms).");
    }
#endif

    if (!fsm_system_init()) {
        Serial.println("fsm_system_init: FAULT");
    } else {
        if (!hal_motor_reclaim_outputs()) {
            Serial.println("[HAL] reclaim SD FALHOU");
        }
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
    const esc_state_t esc_state = fsm_system_get_state();

    static esc_state_t s_prev_esc_state = ESC_STATE_INIT;

    if (esc_state != s_prev_esc_state) {
        Serial.printf("[FSM] %s -> %s\n",
                      fsm_system_state_name(s_prev_esc_state),
                      fsm_system_state_name(esc_state));
    }

#if BOARD_ENABLE_WIFI_TELEMETRY && WIFI_TELEMETRY_DEFER_IN_RUNNING
    static uint32_t s_last_light_wifi_ms = 0;

    if (esc_state == ESC_STATE_RUNNING && s_prev_esc_state != ESC_STATE_RUNNING) {
        wifi_telemetry_clear_running_batch();
    }
    if (s_prev_esc_state == ESC_STATE_RUNNING &&
        (esc_state == ESC_STATE_IDLE || esc_state == ESC_STATE_FAULT)) {
        wifi_telemetry_finalize_running_batch();
        if (wifi_telemetry_client_count() > 0) {
            push_wifi_telemetry_full(&ps4);
        }
    }
#endif

    s_prev_esc_state = esc_state;

    if ((now_ms - last_poll_ms) >= PS4_INPUT_POLL_MS) {
        last_poll_ms = now_ms;

#if BOARD_ENABLE_PS4_BT
        if (ps4_input_update(&ps4)) {
            apply_ps4_to_esc(&ps4);

            ps4_input_set_led_status(
                led_status_from_fsm(ps4.connected, fsm_system_get_state()));
        }
#elif BOARD_ENABLE_SERIAL_HMI
        if (serial_hmi_update(&ps4)) {
            const esc_state_t fsm_before = fsm_system_get_state();
            apply_ps4_to_esc(&ps4);
            const esc_state_t fsm_after = fsm_system_get_state();

            if (fsm_before != fsm_after) {
                Serial.printf("[FSM] %s -> %s\n",
                              fsm_system_state_name(fsm_before),
                              fsm_system_state_name(fsm_after));
                s_prev_esc_state = fsm_after;
            }
        }
#endif
    }

    /* Telemetria serial 500 ms (IDLE e RUNNING). Wi-Fi push só com dashboard ativo. */
#if BOARD_ENABLE_WIFI_TELEMETRY
    const uint32_t telem_interval_ms =
        (esc_state == ESC_STATE_RUNNING ||
         wifi_telemetry_client_count() > 0) ? 100U : 500U;
#else
    const uint32_t telem_interval_ms = 500U;
#endif

    if ((now_ms - s_last_telemetry_ms) >= telem_interval_ms) {
        s_last_telemetry_ms = now_ms;
        print_telemetry(&ps4);
#if BOARD_ENABLE_WIFI_TELEMETRY
#if WIFI_TELEMETRY_DEFER_IN_RUNNING
        if (esc_state == ESC_STATE_RUNNING) {
            wifi_telemetry_record_running_sample(
                now_ms,
                motor_control_get_measured_rpm(),
                motor_control_get_measured_amps(),
                motor_control_get_duty_percent(),
                motor_control_get_open_loop_comm_hz());
            if (wifi_telemetry_client_count() > 0 &&
                (now_ms - s_last_light_wifi_ms) >= 1000U) {
                s_last_light_wifi_ms = now_ms;
                const bool ps4_conn = ps4.connected;
                wifi_telemetry_push_light_running_status(
                    now_ms,
                    wifi_telemetry_running_buf_count(),
                    ps4_conn,
                    ps4_conn ? static_cast<uint8_t>(ps4.r2_raw * 100U / 255U) : 0U);
            }
        } else {
            push_wifi_telemetry_full(&ps4);
        }
#else
        push_wifi_telemetry(&ps4);
#endif
#endif
    }

#if BOARD_ENABLE_WIFI_TELEMETRY && WIFI_TELEMETRY_DEFER_IN_RUNNING
    s_prev_esc_state = esc_state;
#endif

    vTaskDelay(1);
}
