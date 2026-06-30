/*
 * fsm_system.c — Máquina de estados finita de alto nível do ESC.
 *
 * Camada: aplicação. Estados: INIT → IDLE → RUNNING ↔ FAULT.
 * Orquestra init de HAL/drivers, arm/disarm e resposta a falhas (OC, UVLO, stall).
 * Não executa comutação nem PI — delega a motor_control.
 */

#include "fsm_system.h"

#include "battery_monitor.h"
#include "bemf_zcd.h"
#include "board_config.h"
#include "hal_adc.h"
#include "hal_motor.h"
#include "ina240_current_sensors.h"
#include "lm339_protection.h"
#include "motor_control.h"

#include "esp_attr.h"
#include <stddef.h>

static esc_state_t s_state = ESC_STATE_INIT;
static volatile bool s_fault_pending = false;

/** Callback mínimo da ISR de OCP — apenas sinaliza flag para fsm_system_tick. */
static void IRAM_ATTR on_overcurrent_isr(void *arg)
{
    (void)arg;
    s_fault_pending = true;
}

/**
 * @brief Sequência unificada de entrada em falha (fail-safe).
 * Ordem: SD=HIGH → desarm motor_control → disarm HAL → estado FAULT.
 */
static void enter_fault_state(void)
{
    hal_motor_disarm();
    motor_control_on_disarm();
    s_state = ESC_STATE_FAULT;
}

/**
 * @brief Sequência de boot: HAL → proteção → sensores → opcional BEMF.
 * Qualquer falha retorna false e fsm_system_init chama enter_fault_state.
 */
static bool run_init_sequence(void)
{
    if (!hal_motor_init()) {
        return false;
    }

    if (!hal_adc_init()) {
        return false;
    }

    if (!lm339_protection_init()) {
        return false;
    }

    ina240_init();

    if (!ina240_is_offset_calibrated()) {
        if (!ina240_calibrate_offset(INA240_CALIBRATION_SAMPLES)) {
            return false;
        }
    }

    battery_monitor_init();

#if BOARD_ENABLE_BEMF_ZCD
    (void)bemf_zcd_init();
#endif

    return true;
}

/** Boot completo: init periféricos, arma OCP, cria timer 1 kHz do motor_control. */
bool fsm_system_init(void)
{
    s_fault_pending = false;
    s_state = ESC_STATE_INIT;

    if (!run_init_sequence()) {
        enter_fault_state();
        return false;
    }

    if (!lm339_protection_arm(on_overcurrent_isr, NULL)) {
        enter_fault_state();
        return false;
    }

    if (!motor_control_init()) {
        enter_fault_state();
        return false;
    }

    s_state = ESC_STATE_IDLE;
    return true;
}

/**
 * @brief Supervisão periódica no loop() Arduino (~contínuo).
 * Processa flags de ISR, falhas de software, UVLO e latch LM339.
 */
void fsm_system_tick(void)
{
    if (s_fault_pending) {
        s_fault_pending = false;
        if (s_state != ESC_STATE_FAULT) {
            enter_fault_state();
        }
        return;
    }

    if (s_state == ESC_STATE_FAULT) {
        return;
    }

    if (s_state == ESC_STATE_RUNNING && motor_control_consume_software_fault()) {
        enter_fault_state();
        return;
    }

    if (battery_monitor_uvlo_active()) {
        if (s_state == ESC_STATE_RUNNING) {
            motor_control_trip_uvlo_fault();
            enter_fault_state();
        }

        return;
    }

    if (lm339_protection_fault_active()) {
        if (s_state != ESC_STATE_FAULT) {
            enter_fault_state();
        }
    }
}

esc_state_t fsm_system_get_state(void)
{
    return s_state;
}

const char *fsm_system_state_name(esc_state_t state)
{
    switch (state) {
    case ESC_STATE_INIT:
        return "INIT";
    case ESC_STATE_IDLE:
        return "IDLE";
    case ESC_STATE_RUNNING:
        return "RUNNING";
    case ESC_STATE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

/** IDLE → RUNNING: exige sem UVLO e sem falha LM339 ativa. */
bool fsm_system_request_arm(void)
{
    if (s_state != ESC_STATE_IDLE) {
        return false;
    }

    if (lm339_protection_fault_active()) {
        return false;
    }

    if (battery_monitor_uvlo_active()) {
        return false;
    }

    if (!hal_motor_arm()) {
        hal_motor_disarm();
        return false;
    }

    motor_control_on_arm();
    s_state = ESC_STATE_RUNNING;

    return true;
}

/** RUNNING → IDLE: ordem inversa do arm — para motor antes de desligar drivers. */
bool fsm_system_request_disarm(void)
{
    if (s_state != ESC_STATE_RUNNING) {
        return false;
    }

    motor_control_on_disarm();
    hal_motor_disarm();
    s_state = ESC_STATE_IDLE;
    return true;
}

/** FAULT → IDLE: requer hardware OC liberado e UVLO inativo. */
bool fsm_system_clear_fault(void)
{
    if (s_state != ESC_STATE_FAULT) {
        return false;
    }

    lm339_protection_clear_fault();

    if (lm339_protection_fault_active()) {
        return false;
    }

    if (battery_monitor_uvlo_active()) {
        return false;
    }

    s_fault_pending = false;
    motor_control_on_disarm();
    hal_motor_disarm();
    s_state = ESC_STATE_IDLE;
    return true;
}
