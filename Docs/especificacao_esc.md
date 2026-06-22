```markdown
# Arquitetura de Firmware: ESC Trifásico para Motor BLDC (ESP32)

---

## Diretriz de Sincronização Contínua

> **REGRA DE MANUTENÇÃO:** Toda vez que o código C++ for alterado ou o texto em LaTeX sofrer revisões de escopo (ex.: mudança de motores, adição de sensores, alteração de limiares de proteção, inclusão de novos módulos de firmware), a IA atuante DEVE obrigatoriamente atualizar a Documentação de Programação (`Firmware/DOCUMENTACAO_PROGRAMACAO.md`) e a Memória do TCC (`MEMORIA_TCC.md`) em paralelo para evitar defasagem técnica entre o estado real do código e os documentos de referência do trabalho. Divergências entre esta especificação funcional e a implementação efetiva devem ser documentadas na Seção 1 ou Seção 4 da Documentação de Programação.

---

## 1. Visão Geral
Sistema de controle de velocidade eletrônico (ESC) para motor BLDC trifásico, operando sob o microcontrolador ESP32 (framework ESP-IDF com FreeRTOS). A arquitetura é estritamente modular, baseada em eventos, dividida em camadas (Aplicação, Controle, Drivers de Sensores e HAL), garantindo o isolamento da lógica matemática do hardware físico. O mapeamento de pinos segue o DevKitC v4; GPIO **12–15** reservados para JTAG (ESP-Prog); ZCD BEMF em GPIO **16, 17, 5** (U3 RX2/TX2/D5).

## 2. Estrutura de Diretórios e Arquivos (PlatformIO)
A base de código segue o padrão do PlatformIO, isolando as lógicas agnósticas e abstrações de hardware em subdiretórios como bibliotecas isoladas, enquanto o ponto de entrada e configurações globais residem nas pastas de origem e cabeçalho.

### `include/` (Cabeçalhos Globais)
* `board_config.h`: Único arquivo contendo mapeamento de pinos físicos e limites operacionais. Acessível por todas as camadas do projeto.

### `src/` (Source - Aplicação e Estado)
* `main.c`: Ponto de entrada. Inicializa a HAL, instancia as tarefas do FreeRTOS e executa o *loop* principal da Máquina de Estados.
* `fsm_system.h` / `fsm_system.c`: Gerencia os estados lógicos do ESC e coordena as transições de segurança (*Finite State Machine*).

### `lib/control/` (Lógica de Controle Agnóstica)
* `motor_control.h` / `motor_control.c`: Núcleo de cálculo responsável por receber a telemetria e determinar o *Duty Cycle* das 3 fases.
* `pid_regulator.h` / `pid_regulator.c`: Biblioteca matemática discreta do controlador Proporcional-Integral, obrigatoriamente com proteção *Anti-Windup*.

### `lib/hal/` (Abstração de Hardware - Silício ESP32)
* `hal_pwm.h` / `hal_pwm.c`: Interface direta com o periférico MCPWM. Gera os sinais (AH, AL, BH, BL, CH, CL) e aplica o *Dead-Time* por hardware.
* `hal_adc.h` / `hal_adc.c`: Interface com o periférico ADC. Retorna apenas os valores lidos em milivolts brutos.
* `hal_gpio.h` / `hal_gpio.c`: Configuração de pinos digitais e interrupções externas (EXTI).

### `lib/drivers/` (Sensores e Proteção)
* `ina240_current_sensors.h` / `ina240_current_sensors.c`: Driver específico para os INA240A1DR. Extrai o *offset* de $1,65 \: V$ e aplica o ganho ($20 \: V/V$) para conversão em Amperes ($A$).
* `battery_monitor.h` / `battery_monitor.c`: Driver para o divisor resistivo do *Link DC* ($39 \: k\Omega$ / $4,7 \: k\Omega$). Escala a leitura bruta para Volts ($V$).
* `lm339_protection.h` / `lm339_protection.c`: Vincula a interrupção gerada no pino de *OC Trip* à rotina de desarme crítico do motor.

### Raiz do Projeto
* `platformio.ini`: Configuração de ambiente, compilador e *debug*. Define o framework `espidf` e a interface `esp-prog` para o ICE via JTAG.

## 3. Máquina de Estados

A operação é regida pelo `fsm_system`, definida pela seguinte enumeração:

```c
typedef enum {
    ESC_STATE_INIT = 0,   // Boot: Calibrando ADC, lendo offset do INA240 (1.65V) e configurando PWM/EXTI.
    ESC_STATE_IDLE,       // Espera: Aguardando sinal de Arming (aceleração nula). PWM 0%. Telemetria ativa.
    ESC_STATE_RUNNING,    // Ativo: Motor comutando. Lógica PID processando e atualizando Duty Cycle.
    ESC_STATE_FAULT       // Falha: Interrupção LM339 (Sobrecorrente). Hardware desarmado. Requer reset.
} esc_state_t;

```

## 4. Regras de Controle (Agnósticas)

A biblioteca `pid_regulator` processa a malha de controle através da estrutura base:

```c
typedef struct {
    float kp;             // Ganho Proporcional
    float ki;             // Ganho Integral
    float integral_term;  // Acumulador do erro discreto
    float out_max;        // Saturação máxima
    float out_min;        // Saturação mínima
} pi_controller_t;

```

**Regra Crítica:** A implementação da função computacional deve possuir limitadores rígidos para a variável `integral_term` (*Anti-Windup*) e para a variável `output` final, evitando instabilidade de saturação térmica e elétrica na ponte inversora.

## 5. Configurações Globais de Hardware

Mapa otimizado DevKitC v4. Ver [`Firmware/include/board_config.h`](../Firmware/include/board_config.h), [`Hardware/PCB_Project/ESP32_PINMAP.md`](../Hardware/PCB_Project/ESP32_PINMAP.md) e [`Firmware/DOCUMENTACAO_PROGRAMACAO.md`](../Firmware/DOCUMENTACAO_PROGRAMACAO.md) §8.

```c
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// --- Pinos de Controle de Gate (MCPWM) ---
#define PIN_PWM_AH    21
#define PIN_PWM_AL    22
#define PIN_PWM_BH    27
#define PIN_PWM_BL    23
#define PIN_PWM_CH    18
#define PIN_PWM_CL    19

// --- Shutdown IR2110 (SD, ativo baixo) ---
#define PIN_SD_A      32
#define PIN_SD_B      33
#define PIN_SD_C      4

// --- Pinos Analógicos (ADC1) ---
#define PIN_ADC_IA    34
#define PIN_ADC_IB    35
#define PIN_ADC_IC    36
#define PIN_ADC_VBAT  39

// --- OCP LM339 ---
#define PIN_VDAC_REF  25   // DAC1
#define PIN_OC_TRIP   26

// --- ZCD BEMF (U3 RX2/TX2/D5; coexistem com JTAG 12–15) ---
#define BOARD_ENABLE_BEMF_ZCD  0
#define PIN_ZCD_A     16
#define PIN_ZCD_B     17
#define PIN_ZCD_C     5

// --- Constantes de Operação ---
#define MAX_DUTY_CYCLE_PERCENT 95.0f
#define PWM_FREQUENCY_HZ       20000
#define DEAD_TIME_NS           500

#endif // BOARD_CONFIG_H
```

```

```
