/*
 * board_config.h — Configuração central do hardware e parâmetros do ESC.
 *
 * Camada: configuração em tempo de compilação (incluída por HAL, drivers e controle).
 * Única fonte de verdade para pinos GPIO, limites operacionais e ganhos PI.
 * Alterar um pino ou limiar aqui propaga-se a todo o firmware sem mudar lógica de módulos.
 *
 * Ref.: Hardware/PCB_Project/ESP32_PINMAP.md, esp32_devkitC_v4_pinlayout.png
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* Pinos reservados pelo silício — não usar para o ESC */
// JTAG (ESP-Prog): GPIO12 MTDI, 13 MTCK, 14 TMS, 15 MTDO
// Flash SPI interna: GPIO6–11

// ==========================================
// MAPEAMENTO LÓGICO DEFINITIVO (PORTAS 'D')
// ==========================================

// --- FASE A ---
#define PIN_AH            19
#define PIN_AL            22
#define PIN_SHUTDOWN_A    32

// --- FASE B ---
#define PIN_BH            27
#define PIN_BL            23
#define PIN_SHUTDOWN_B    33

// --- FASE C ---
#define PIN_CH            15
#define PIN_CL            18
#define PIN_SHUTDOWN_C    4

/* Aliases legados (hal_pwm / docs) */
#define PIN_PWM_AH        PIN_AH
#define PIN_PWM_AL        PIN_AL
#define PIN_PWM_BH        PIN_BH
#define PIN_PWM_BL        PIN_BL
#define PIN_PWM_CH        PIN_CH
#define PIN_PWM_CL        PIN_CL
#define PIN_SD_A          PIN_SHUTDOWN_A
#define PIN_SD_B          PIN_SHUTDOWN_B
#define PIN_SD_C          PIN_SHUTDOWN_C

/** IR2110 SD (datasheet): HIGH = HO/LO High-Z; LOW = segue HIN/LIN. */
#define IR2110_SD_SHUTDOWN_LEVEL  1
#define IR2110_SD_ENABLE_LEVEL    0

// --- SENSORES DE CORRENTE E TENSÃO ---
#define PIN_ISENSE_A      34
#define PIN_ISENSE_B      35
#define PIN_ISENSE_C      36  /* VP */
#define PIN_VCC_SENSE     39  /* VN */
#define PIN_ADC_IA        PIN_ISENSE_A
#define PIN_ADC_IB        PIN_ISENSE_B
#define PIN_ADC_IC        PIN_ISENSE_C
#define PIN_ADC_VBAT      PIN_VCC_SENSE

/** Calibração de offset INA240 no boot (128 amostras por fase). */
#define INA240_CALIBRATION_SAMPLES  128U
/** Filtro EMA em runtime (1 ADC/leitura): A mais lento (GPIO 34 ruidoso); B/C inalterados. */
#define INA240_MV_EMA_ALPHA_A       0.05f
#define INA240_MV_EMA_ALPHA_BC      0.25f
#define INA240_NOMINAL_OFFSET_MV    1650.0f

/** Vref interno do ADC ESP32 (mV) para esp_adc_cal quando eFuse ausente — NÃO é 3300 mV. */
#define ADC_DEFAULT_VREF_MV         1100U

/** Override de offset INA240 medido na bancada (Setup 1); 0 = offset = média ADC. */
#define INA240_USE_MANUAL_OFFSET    1
#define INA240_MANUAL_OFFSET_A_MV   1670.0f
#define INA240_MANUAL_OFFSET_B_MV   1480.0f
#define INA240_MANUAL_OFFSET_C_MV   1510.0f

/** Mediana de N leituras ADC só fase A (GPIO 34); 0 = desligado. */
#define INA240_A_MEDIAN_SAMPLES         16U

/** 2ª calibração INA240 após softAP (atualiza adc_zero/bench_corr com RF ativo). */
#define INA240_RECAL_AFTER_WIFI         1
#define INA240_RECAL_AFTER_WIFI_DELAY_MS 300U

/** 3ª calibração INA240 após ps4_input_init (regime BT scan ativo). */
#define INA240_RECAL_AFTER_PS4          0
#define INA240_RECAL_AFTER_PS4_DELAY_MS 500U

// --- PROTEÇÃO OCP (LM339) ---
#define PIN_VDAC_REF      25
#define PIN_OC_TRIP       26

/* --- ZCD BEMF (opcional): comparadores para comutação sensorless por cruzamento por zero --- */
// 0 = malha aberta (padrão). 1 = permite handover OPEN → ZCD fechado.
#define BOARD_ENABLE_BEMF_ZCD  0
#define PIN_ZCD_A     16   // U3 RX2 (pino símb. 6)
#define PIN_ZCD_B     17   // U3 TX2 (pino símb. 7)
#define PIN_ZCD_C     5    // U3 D5  (pino símb. 8); pull-up 10k no comparador

/** Atraso elétrico após ZCD até comutar (graus). Spec/tese: 30°. */
#define BEMF_COMM_DELAY_DEG_ELEC  30.0f

/** Eventos ZCD válidos consecutivos para handover malha aberta → fechada. */
#define BEMF_ZCD_HANDOVER_COUNT   6U

/* --- PWM e malha: frequência de comutação, dead-time e teto de duty (bootstrap IR2110) --- */
#define MAX_DUTY_CYCLE_PERCENT 95.0f
#define PWM_FREQUENCY_HZ       20000
#define DEAD_TIME_NS           500

#define CONTROL_LOOP_HZ        10000.0f
#define CONTROL_DT_S           (1.0f / CONTROL_LOOP_HZ)

/** Malha aberta: frequência elétrica inicial / máxima (Hz) e incremento por passo.
 *  Motor A2212/10T 1400kV (14 polos, p=7): teto de 300 Hz → ≈ 2571 RPM mecânicos.
 *  Limite garante FCEM detectável para handover ZCD sem perda de sincronismo (stall). */
#define MOTOR_OPEN_LOOP_COMM_HZ_START        5.0f
#define MOTOR_OPEN_LOOP_COMM_HZ_MAX          300.0f
#define MOTOR_OPEN_LOOP_COMM_HZ_RAMP_PER_STEP 1.5f
#define MOTOR_OPEN_LOOP_COMM_HZ_MAX_LIMIT      300.0f
#define MOTOR_OPEN_LOOP_COMM_RAMP_MIN          0.1f
#define MOTOR_OPEN_LOOP_COMM_RAMP_MAX          20.0f

/** Alinhamento estático do rotor antes da rampa (tese: ~500 ms). */
#define MOTOR_ALIGN_DURATION_MS    100U
#define MOTOR_ALIGN_DUTY_PERCENT   3.0f
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
#define MOTOR_SOFTWARE_OC_AMPS     15.0f

/** OCP hardware: DAC1 raw → ~2,15 V → ~25 A (shunt 1 mΩ, INA240 20 V/V). */
#define LM339_OCP_DAC_RAW          166U
#define LM339_OCP_AMPS_LIMIT       25.0f
/** Limiar OCP documentado (A); referência para telemetria — HW usa LM339_OCP_DAC_RAW. */
#define LM339_HW_OC_AMPS           LM339_OCP_AMPS_LIMIT

/** Stall: corrente elevada sustentada em RUN (malha aberta dessincronizada). */
#define MOTOR_STALL_CURRENT_AMPS   6.0f
#define MOTOR_STALL_TIMEOUT_MS     300U

/** Stall: sem avanço de passo por N vezes o período esperado (RUN, I*>0). */
#define MOTOR_STALL_STEP_TIMEOUT_MULT  4U

/* --- Entrada PS4: R2 com auto-calibração de repouso (offset por controle) --- */
#include "board_profiles.h"
/** Janela após BT connect para aprender R2 em repouso (não pressione o gatilho). */
#define PS4_R2_CALIB_MS          2000U
/** Margem acima do repouso calibrado antes de considerar gatilho pressionado. */
#define PS4_R2_REST_MARGIN       10U
/** Travel efetivo mínimo (0–255 pós-offset) para arm / torque (histerese implícita). */
#define PS4_R2_ARM_EFFECTIVE     12U
/** R2 repouso=0 (Bluepad32): raw mínimo para arm/desarm (idle medido 0–4). */
#define PS4_R2_ZERO_REST_ARM_RAW 35U
/** 1 = troca fases B↔C (só inverte sentido; equivalente a trocar 2 fios). */
#define MOTOR_SWAP_PHASES_BC       0
/** 1 = RUN_OPEN em f_el fixo (bancada); R2 só arma/desarma, não acelera. */
#define MOTOR_BENCH_FIX_RUN_OPEN_F_EL  1
#define MOTOR_BENCH_RUN_OPEN_F_EL_HZ   8.0f
/** Repouso em R2=0: tempo contínuo em zero para calibrar (Bluepad32 idle). */
#define PS4_R2_ZERO_REST_MS      PS4_R2_CALIB_MS
/** Repouso analógico elevado: spread máx. na janela e teto idle (não pressionar). */
#define PS4_R2_STABLE_SPREAD     12U
#define PS4_R2_STABLE_MAX_IDLE   55U
#define PS4_R2_ANALOG_REST_MIN   8U
/** Período de polling do controle no loop principal (ms). */
#define PS4_INPUT_POLL_MS     20U
#if PS4_ACTIVE_PROFILE == PS4_PROFILE_BENCH_STABLE
/** Perfil bancada: sem lightbar até pareamento estável. */
#define PS4_SKIP_LIGHTBAR_ON_CLONE  1
#else
/** Perfil DS4 Sony original: lightbar por FSM (setColorLED). */
#define PS4_SKIP_LIGHTBAR_ON_CLONE  0
#endif
/** 1 = touchpad virtual DS4 (evita log "Failed to create virtual device"). */
/** 0 = só controle físico na bancada; 1 = gamepad virtual Bluepad32 (pode conflitar com DS4 real). */
#define PS4_ENABLE_VIRTUAL_DEVICE 0
/** Nível GAP BTstack (0=off). sdkconfig.defaults não altera lib pré-compilada — aplicar em runtime. */
#define PS4_GAP_SECURITY_LEVEL  0
/** 1 = apaga chaves BT no boot (recovery SDP — usar UMA VEZ com PS4 esquecido também).
 *  NÃO deixe 1 permanente: ESP apaga chaves mas PS4 tenta reconectar → sdp_query_timeout. */
#define PS4_FORGET_BT_KEYS_ON_BOOT  0
/** Sem conectar em N ms: apaga chaves 1× por boot e pede Share+PS (SDP/pareamento preso). */
#define PS4_AUTO_RECOVERY_PAIRING_MS  30000U

/** 1 = Bluepad32 / DualShock 4 ativo; 0 = BT desligado (bancada inversor). */
#define BOARD_ENABLE_PS4_BT  0
/** 1 = HMI serial via monitor USB (substitui PS4 na bancada). */
#define BOARD_ENABLE_SERIAL_HMI  1

#if BOARD_ENABLE_PS4_BT && BOARD_ENABLE_SERIAL_HMI
#error "Ative apenas PS4 ou Serial HMI, nao ambos"
#endif

/** Task FreeRTOS do HMI serial (Core 0 — Core 1 reservado ao motor). */
#define SERIAL_HMI_TASK_CORE   0
#define SERIAL_HMI_TASK_STACK  2048
#define SERIAL_HMI_TASK_PRIO   1
#define SERIAL_HMI_POLL_MS     20U
/** Passo de '+'/'-' em modo SPEED (RPM) ou CURRENT (A). */
#define SERIAL_HMI_RPM_STEP    50.0f
#define SERIAL_HMI_AMPS_STEP   0.2f

/* --- UVLO: subtensão do pack LiPo; detecção automática de 4S–6S no boot --- */
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

/* --- Modo SPEED vs CURRENT: seleção em tempo de compilação (R2 → RPM ou corrente) --- */
/** 1 = R2 controla RPM (SPEED); 0 = R2 controla corrente (CURRENT). */
#define MOTOR_CONTROL_USE_SPEED_MODE   1

/** Pares de polos — motor A2212/10T 1400kV (14 polos magnéticos → p = 7).
 *  Governa f_e = p · n/60: conversão entre frequência elétrica de comutação e RPM mecânico. */
#define MOTOR_POLE_PAIRS               7U
#define MOTOR_SPEED_MAX_RPM            2571.0f   /* 300 Hz × 60 / 7 (A2212/10T) */
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
/** Teto de f_el na partida RUN_OPEN (malha aberta forçada) até handover RUN_SPEED. */
#define MOTOR_OPEN_LOOP_RUN_OPEN_RAMP_MAX_HZ  18.0f
/** Velocidade de aproximação de f_el ao comando R2 em RUN_OPEN (Hz/s). */
#define MOTOR_OPEN_LOOP_RUN_OPEN_F_EL_SLEW_HZ_PER_S  2.0f
/** Dessincronismo RPM vs f_el_cmd em RUN_SPEED. */
#define MOTOR_SPEED_DESYNC_RPM             200.0f
#define MOTOR_SPEED_DESYNC_TIMEOUT_MS      300U

/* --- Wi-Fi Telemetry Dashboard (Access Point mode) ---
 * O ESP32 cria uma rede Wi-Fi própria sem necessidade de roteador.
 * Conectar ao SSID abaixo e abrir http://192.168.4.1 no browser.
 * Coexistência BT Classic (DualShock 4) + Wi-Fi gerenciada pelo ESP-IDF.
 * ADC1 (pinos 34/35/36/39) não é afetado — apenas ADC2 tem conflito com rádio.
 *
 * Para ensaios de bancada (ex.: ID 16 dead-time): defina 0 para desligar AP/HTTP
 * e reduzir contenção de rádio com o PS4. Telemetria serial (115200) permanece.
 */
#define BOARD_ENABLE_WIFI_TELEMETRY  0
#define WIFI_AP_SSID        "ESC-Dashboard"
#define WIFI_AP_PASSWORD    "esc12345"
#define WIFI_AP_CHANNEL     6
#define WIFI_TELEMETRY_PORT 80

/** Telemetria diferida em RUNNING: grava amostras compactas em RAM e expõe lote em IDLE.
 *  Só efetivo com BOARD_ENABLE_WIFI_TELEMETRY=1. DEFER=0 restaura push JSON completo em RUNNING. */
#define WIFI_TELEMETRY_DEFER_IN_RUNNING  1
#define WIFI_TELEM_RUN_BUF_SAMPLES       80   /* 80 × 100 ms ≈ 8 s de corrida */
#define WIFI_TELEM_RUN_SAMPLE_MS         100  /* alinhado ao intervalo RUNNING no loop */

#endif // BOARD_CONFIG_H
