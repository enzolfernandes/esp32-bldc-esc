// Mapa otimizado DevKitC v4 (esp32_devkitC_v4_pinlayout.png). Ref: Hardware/PCB_Project/ESP32_PINMAP.md

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// JTAG livre (sem nets ESC): GPIO12 MTDI, 13 MTCK, 14 TMS, 15 MTDO — header ESP-Prog
// Flash SPI interna reservada: GPIO6, 7, 8, 9, 10, 11

// --- Pinos de controle de gate (MCPWM) ---
#define PIN_PWM_AH    21
#define PIN_PWM_AL    22
#define PIN_PWM_BH    27
#define PIN_PWM_BL    23   // evita GPIO14 (TMS/JTAG)
#define PIN_PWM_CH    18
#define PIN_PWM_CL    19

// --- Shutdown IR2110 (SD, ativo baixo: HIGH = operação, LOW = shutdown) ---
#define PIN_SD_A      32
#define PIN_SD_B      33
#define PIN_SD_C      4

// --- Pinos analógicos (ADC1 — compatível com Wi-Fi ativo) ---
#define PIN_ADC_IA    34   // ADC1_CH6 (input-only)
#define PIN_ADC_IB    35   // ADC1_CH7 (input-only)
#define PIN_ADC_IC    36   // ADC1_CH0 / SENSOR_VP (input-only)
#define PIN_ADC_VBAT  39   // ADC1_CH3 / SENSOR_VN (input-only)

// --- OCP LM339 (wired-OR → OC Trip; Vdac Ref = DAC1) ---
#define PIN_VDAC_REF  25   // DAC1, referência comparadores OCP (+)
#define PIN_OC_TRIP   26   // Entrada digital, ativo baixo + pull-up

// --- ZCD BEMF (LM339) — U3 RX2/TX2/D5; coexistem com JTAG em 12–15 ---
// 0 = sem comparadores BEMF (malha aberta). 1 = handover OPEN→ZCD.
#define BOARD_ENABLE_BEMF_ZCD  0
#define PIN_ZCD_A     16   // U3 RX2 (pino símb. 6)
#define PIN_ZCD_B     17   // U3 TX2 (pino símb. 7)
#define PIN_ZCD_C     5    // U3 D5  (pino símb. 8); pull-up 10k no comparador

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

/** Malha aberta: frequência elétrica inicial / máxima (Hz) e incremento por passo. */
#define MOTOR_OPEN_LOOP_COMM_HZ_START        5.0f
#define MOTOR_OPEN_LOOP_COMM_HZ_MAX          120.0f
#define MOTOR_OPEN_LOOP_COMM_HZ_RAMP_PER_STEP 1.5f
#define MOTOR_OPEN_LOOP_COMM_HZ_MAX_LIMIT      300.0f
#define MOTOR_OPEN_LOOP_COMM_RAMP_MIN          0.1f
#define MOTOR_OPEN_LOOP_COMM_RAMP_MAX          20.0f

/** Alinhamento estático do rotor antes da rampa (tese: ~500 ms). */
#define MOTOR_ALIGN_DURATION_MS    500U
#define MOTOR_ALIGN_DUTY_PERCENT   12.0f
#define MOTOR_ALIGN_DUTY_MIN       3.0f
#define MOTOR_ALIGN_DUTY_MAX       25.0f
#define MOTOR_ALIGN_DURATION_MS_MIN  100U
#define MOTOR_ALIGN_DURATION_MS_MAX  2000U
#define MOTOR_ALIGN_STEP_CW        0U   /* A+ B- */
#define MOTOR_ALIGN_STEP_CCW       3U   /* A- B+ */

/** Rampa do comando de corrente (A/s); evita degrau brusco após ALIGN. */
#define MOTOR_TARGET_SLEW_AMPS_PER_S  2.0f
#define MOTOR_TARGET_SLEW_MIN         0.5f
#define MOTOR_TARGET_SLEW_MAX         20.0f

/** Ganhos PI de corrente; defaults em board_config.h (sem tuning serial). */
#define MOTOR_PI_KP_DEFAULT  8.0f
#define MOTOR_PI_KI_DEFAULT  120.0f
#define MOTOR_PI_KP_MIN      0.0f
#define MOTOR_PI_KP_MAX      50.0f
#define MOTOR_PI_KI_MIN      0.0f
#define MOTOR_PI_KI_MAX      500.0f
#define MOTOR_PI_INTEG_MIN   -40.0f
#define MOTOR_PI_INTEG_MAX   40.0f

/** Trip de sobrecorrente em software (A); complementa LM339 na bancada. */
#define MOTOR_SOFTWARE_OC_AMPS     8.0f

/** Limiar OCP hardware (A) via Vdac (DAC1 → LM339 +); default = trip SW. */
#define LM339_HW_OC_AMPS           MOTOR_SOFTWARE_OC_AMPS

/** Stall: corrente elevada sustentada em RUN (malha aberta dessincronizada). */
#define MOTOR_STALL_CURRENT_AMPS   6.0f
#define MOTOR_STALL_TIMEOUT_MS     300U

/** Stall: sem avanço de passo por N vezes o período esperado (RUN, I*>0). */
#define MOTOR_STALL_STEP_TIMEOUT_MULT  4U

// --- Entrada PS4 (Bluepad32) ---
/** R2 abaixo deste valor desarma; acima permite armar e mapeia corrente. */
#define PS4_R2_ARM_THRESHOLD  10U
/** Zona morta analógica do gatilho R2 (0–255). */
#define PS4_R2_DEADZONE       5U
/** Período de polling do controle no loop principal (ms). */
#define PS4_INPUT_POLL_MS     20U

// --- UVLO barramento DC (LiPo 4S–6S, auto-detect no boot) ---
/** Faixa de packs suportados (células em série). */
#define BATTERY_CELL_COUNT_S_MIN       4U
#define BATTERY_CELL_COUNT_S_MAX       6U
/** Tensão máxima por célula (plena carga) — limite inferior de S na detecção. */
#define BATTERY_CELL_VMAX_FULL         4.2f
/** UVLO: tensão mínima por célula (disparo) e de recuperação (histerese). */
#define BATTERY_CELL_UVLO_CUTOFF_V     3.3f
#define BATTERY_CELL_UVLO_RECOVER_V    3.5f
/** Tempo contínuo abaixo do cutoff antes de ativar UVLO (ms). */
#define BATTERY_UVLO_DEBOUNCE_MS       100U

// --- Malha de velocidade (modo dual compile-time) ---
/** 1 = R2 controla RPM (SPEED); 0 = R2 controla corrente (CURRENT). */
#define MOTOR_CONTROL_USE_SPEED_MODE   1

/** Pares de polos (motor 4 polos → 2). */
#define MOTOR_POLE_PAIRS               2U
#define MOTOR_SPEED_MAX_RPM            3600.0f
#define MOTOR_SPEED_MIN_RPM            300.0f
#define MOTOR_SPEED_SLEW_RPM_PER_S     1500.0f
#define MOTOR_SPEED_PI_KP_DEFAULT      0.02f
#define MOTOR_SPEED_PI_KI_DEFAULT      0.5f
#define MOTOR_SPEED_PI_INTEG_MIN       -5.0f
#define MOTOR_SPEED_PI_INTEG_MAX       5.0f
/** Rampa OPEN até este RPM antes de PI de velocidade. */
#define MOTOR_SPEED_OPEN_LOOP_HANDOVER_RPM  600.0f
#define MOTOR_SPEED_HANDOVER_MS            200U
/** Corrente fixa durante RUN_OPEN (modo SPEED). */
#define MOTOR_SPEED_OPEN_LOOP_I_AMPS       0.5f
/** Dessincronismo RPM vs f_el_cmd em RUN_SPEED. */
#define MOTOR_SPEED_DESYNC_RPM             200.0f
#define MOTOR_SPEED_DESYNC_TIMEOUT_MS      300U

#endif // BOARD_CONFIG_H
