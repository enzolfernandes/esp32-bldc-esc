//TODO: Revisar pinos no PCB, confirmar ZCD no esquemático da PCB, ajustar constantes de operação conforme necessário.

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// JTAG reservado: GPIO12, 13, 14, 15
// Flash SPI interna reservada: GPIO6, 7, 8, 9, 10, 11

// --- Pinos de controle de gate (MCPWM) ---
#define PIN_PWM_AH    25
#define PIN_PWM_AL    26
#define PIN_PWM_BH    27
#define PIN_PWM_BL    18
#define PIN_PWM_CH    19
#define PIN_PWM_CL    21

// --- Pinos analógicos (ADC1 — compatível com Wi-Fi ativo) ---
#define PIN_ADC_IA    32   // ADC1_CH4
#define PIN_ADC_IB    33   // ADC1_CH5
#define PIN_ADC_IC    34   // ADC1_CH6 (input-only)
#define PIN_ADC_VBAT  35   // ADC1_CH7 (input-only)

// --- Pino de segurança de hardware ---
#define PIN_OC_TRIP   4    // LM339 OCP, ativo baixo + pull-up

// --- ZCD BEMF (LM339, saída open-collector) — confirmar no esquemático da PCB ---
#define BOARD_ENABLE_BEMF_ZCD  1
#define PIN_ZCD_A     16
#define PIN_ZCD_B     17
#define PIN_ZCD_C     5

/** Atraso elétrico após ZCD até comutar (graus). Spec/tese: 30°. */
#define BEMF_COMM_DELAY_DEG_ELEC  30.0f

/** Eventos ZCD válidos consecutivos para handover malha aberta → fechada. */
#define BEMF_ZCD_HANDOVER_COUNT   6U

// --- Constantes de operação ---
#define MAX_DUTY_CYCLE_PERCENT 95.0f
#define PWM_FREQUENCY_HZ       20000
#define DEAD_TIME_NS           500

#define CONTROL_LOOP_HZ        10000.0f
#define CONTROL_DT_S           (1.0f / CONTROL_LOOP_HZ)

#endif // BOARD_CONFIG_H
