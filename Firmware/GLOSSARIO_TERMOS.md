# Glossário de Termos, Firmware ESC BLDC (ESP32)

---

## Diretriz de Sincronização Contínua

> **REGRA DE MANUTENÇÃO:** Toda vez que o código C++ for alterado ou o texto em LaTeX sofrer revisões de escopo (ex.: mudança de motores, adição de sensores, alteração de limiares de proteção, inclusão de novos módulos de firmware), a IA atuante DEVE obrigatoriamente atualizar a Documentação de Programação e a Memória do TCC (`MEMORIA_TCC.md`) em paralelo para evitar defasagem técnica entre o estado real do código e os documentos de referência do trabalho. Novas siglas ou termos introduzidos nesse processo devem ser adicionados a este glossário.

---

Este glossário complementa a [Documentação de Programação](DOCUMENTACAO_PROGRAMACAO.md). Mantenha este arquivo aberto em um painel lateral enquanto lê o documento principal para consultar rapidamente siglas, abreviações e termos técnicos.

As entradas estão em ordem alfabética. Cada termo inclui o nome por extenso, uma definição acessível e, quando relevante, o contexto no firmware deste projeto.

> **Navegação:** clique nos links do índice com o arquivo aberto no **visualizador Markdown** (`Ctrl+Shift+V` no Cursor/VS Code). No modo de edição, use `Ctrl+clique` nos links.

---

## Índice alfabético

[A](#a) · [B](#b) · [C](#c) · [D](#d) · [E](#e) · [F](#f) · [G](#g) · [H](#h) · [I](#i) · [J](#j) · [K](#k) · [L](#l) · [M](#m) · [N](#n) · [O](#o) · [P](#p) · [R](#r) · [S](#s) · [T](#t) · [U](#u) · [V](#v) · [W](#w) · [Z](#z)

[6-step](#6-step) · [A2212](#a2212) · [ADC](#adc) · [ADC1](#adc1) · [ADC2](#adc2) · [AH / AL](#ah--al) · [ALIGN](#align) · [anti-windup](#anti-windup) · [API](#api) · [arm](#arm) · [baud](#baud) · [BEMF](#bemf) · [BH / BL](#bh--bl) · [BLDC](#bldc) · [Bluepad32](#bluepad32) · [bootstrap](#bootstrap) · [BT](#bt) · [cascata](#cascata) · [CH / CL](#ch--cl) · [clear fault](#clear-fault) · [coast-down](#coast-down) · [CURRENT](#current) · [CW / CCW](#cw--ccw) · [DAC](#dac) · [DAC1](#dac1) · [dB](#db) · [dead-time](#dead-time) · [debounce](#debounce) · [disarm](#disarm) · [DMA](#dma) · [duty cycle](#duty-cycle) · [ESC](#esc) · [ESP-IDF](#esp-idf) · [ESP32](#esp32) · [ESP-Prog](#esp-prog) · [ESP_TIMER_TASK](#esp_timer_task) · [ESR](#esr) · [ESL](#esl) · [EXTI](#exti) · [fail-safe](#fail-safe) · [FAULT](#fault) · [feedforward](#feedforward) · [FOC](#foc) · [frequência elétrica](#frequência-elétrica) · [FreeRTOS](#freertos) · [FSM](#fsm) · [fsm_system](#fsm_system) · [GPIO](#gpio) · [HAL](#hal) · [handover](#handover) · [HID](#hid) · [high-side / low-side](#high-side--low-side) · [Hz / kHz](#hz--khz) · [IDLE](#idle) · [INA240](#ina240) · [INIT](#init) · [IR2110](#ir2110) · [IRAM_ATTR](#iram_attr) · [Isense](#isense) · [ISR](#isr) · [jitter](#jitter) · [JTAG](#jtag) · [Kconfig](#kconfig) · [L2](#l2) · [LDF](#ldf) · [LEDC](#ledc) · [LiPo](#lipo) · [LM339](#lm339) · [malha aberta / fechada](#malha-aberta--fechada) · [MCPWM](#mcpwm) · [MCU](#mcu) · [MOTOR_OPEN_LOOP_COMM_HZ_MAX](#motor_open_loop_comm_hz_max) · [MOTOR_POLE_PAIRS](#motor_pole_pairs) · [motor_control](#motor_control) · [ms / ns](#ms--ns) · [mutex](#mutex) · [OC](#oc) · [OC Trip](#oc-trip) · [OCP](#ocp) · [OFF / SOURCE / SINK](#off--source--sink) · [OPEN_LOOP](#open_loop) · [outrunner](#outrunner) · [overshoot](#overshoot) · [pares de polos](#pares-de-polos) · [PCB](#pcb) · [PI](#pi) · [pid_regulator](#pid_regulator) · [PlatformIO](#platformio) · [polling](#polling) · [power-cycle](#power-cycle) · [PS4](#ps4) · [PWM](#pwm) · [R2](#r2) · [RC](#rc) · [RGB](#rgb) · [RPM](#rpm) · [RUN / RUN_OPEN / RUN_SPEED](#run--run_open--run_speed) · [RUNNING](#running) · [SD](#sd) · [SDK](#sdk) · [sensorless](#sensorless) · [setpoint](#setpoint) · [slew rate](#slew-rate) · [SPEED](#speed) · [SPI](#spi) · [stall](#stall) · [TCC](#tcc) · [trapezoidal](#trapezoidal) · [UART](#uart) · [UVLO](#uvlo) · [VBAT](#vbat) · [Vdac](#vdac) · [volatile](#volatile) · [Wi-Fi](#wi-fi) · [wired-OR](#wired-or) · [ZCD](#zcd) · [ZCD_CLOSED](#zcd_closed)

---

<a id="a"></a>

## A

<a id="a2212"></a>

### A2212

Motor **Brushless Outrunner A2212/10T 1400kV** — motor de teste adotado nos ensaios em bancada deste projeto.

| Parâmetro | Valor |
|-----------|-------|
| Tipo | Outrunner (rotor externo) |
| Constante de velocidade K_V | 1400 RPM/V |
| Número de polos magnéticos | 14 |
| Pares de polos (p) | **7** |
| `MOTOR_POLE_PAIRS` | `7U` |
| `MOTOR_OPEN_LOOP_COMM_HZ_MAX` | `300.0f` Hz |
| RPM máx. em malha aberta | ≈ 2571 RPM (300 Hz × 60 / 7) |

A geometria outrunner com 14 polos impõe frequência elétrica de comutação 7× maior que a frequência mecânica de rotação, conforme a relação \(f_e = p \cdot n / 60\). Ver também: [outrunner](#outrunner), [pares de polos](#pares-de-polos), [frequência elétrica](#frequência-elétrica).

<a id="adc"></a>

### ADC

**A**nalog-to-**D**igital **C**onverter (Conversor Analógico-Digital).

Dispositivo que transforma uma tensão analógica contínua (ex.: saída de um sensor) em um valor numérico que o microcontrolador pode processar.

**No firmware:** abstraído pelo módulo `hal_adc`, que lê tensões em milivolts.

<a id="adc1"></a>

### ADC1

Primeiro conversor analógico-digital do ESP32.

**No firmware:** usado para leituras de corrente de fase (GPIO 34, 35, 36) e tensão do barramento VBAT (GPIO 39). É o conversor seguro quando o Bluetooth está ativo.

<a id="adc2"></a>

### ADC2

Segundo conversor analógico-digital do ESP32.

Compartilha recursos com o subsistema Wi-Fi/Bluetooth e torna-se indisponível ou não confiável quando o rádio está ativo. Por isso **não** é usado neste firmware.

<a id="ah--al"></a>

### AH / AL

**A** High / **A** Low, pernas superior e inferior da fase A do inversor.

**No firmware:** GPIO 21 (AH) e GPIO 22 (AL), controlados pelo MCPWM.

<a id="align"></a>

### ALIGN

Fase de **alinhamento** na sequência de partida do motor.

Aplica um vetor magnético fixo no estator por 500 ms para posicionar o rotor em um ângulo conhecido antes de iniciar a comutação. Ver também: [sensorless](#sensorless), [6-step](#6-step).

<a id="anti-windup"></a>

### anti-windup

Técnica que impede o termo integral de um controlador PI de acumular valor enquanto a saída está saturada (ex.: duty cycle no teto de 95 %).

Sem anti-windup, ao liberar a saturação o sistema pode apresentar [overshoot](#overshoot) e resposta lenta.

<a id="api"></a>

### API

**A**pplication **P**rogramming **I**nterface (Interface de Programação de Aplicações).

Conjunto de funções públicas que um módulo expõe para ser usado por outros módulos.

<a id="arm"></a>

### arm

**Armar** o ESC, autorizar a geração de PWM e o acionamento do motor.

Transição da FSM de `IDLE` para `RUNNING`. O operador arma pressionando o gatilho R2 acima do limiar. Oposto de [disarm](#disarm).

<a id="baud"></a>

### baud

Unidade de taxa de transmissão serial (símbolos por segundo).

**No firmware:** telemetria na porta serial a 115200 baud, somente leitura.

---

<a id="b"></a>

## B

<a id="bemf"></a>

### BEMF

**B**ack **E**lectromotive **F**orce (Força Eletromotriz Contra).

Tensão induzida nas bobinas do motor quando o rotor gira. Pode ser medida na fase que não está sendo energizada para detectar a posição do rotor sem sensores dedicados.

**No firmware:** comparadores LM339 nos pinos ZCD A/B/C detectam o cruzamento por zero da BEMF quando `BOARD_ENABLE_BEMF_ZCD` está habilitado.

<a id="bh--bl"></a>

### BH / BL

**B** High / **B** Low, pernas superior e inferior da fase B do inversor.

**No firmware:** GPIO 27 (BH) e GPIO 23 (BL).

<a id="bldc"></a>

### BLDC

**B**rush**l**ess **D**C (Motor de Corrente Contínua sem Escovas).

Motor trifásico cujo rotor contém ímãs permanentes e o estator contém bobinas comutadas eletronicamente. Não possui escovas mecânicas.

<a id="bluepad32"></a>

### Bluepad32

Biblioteca que integra o stack Bluetooth Classic ao core Arduino-ESP32, permitindo parear e ler controles como o DualShock 4 (PS4).

**No firmware:** usada em `ps4_input.cpp` para comando remoto do ESC.

<a id="bootstrap"></a>

### bootstrap

Circuito de capacitor que alimenta temporariamente a perna superior (high-side) do driver de gate IR2110.

O firmware limita o duty cycle a 95 % para garantir tempo de recarga do capacitor bootstrap entre pulsos PWM.

<a id="bt"></a>

### BT

**B**lue**t**ooth, protocolo de comunicação sem fio de curto alcance.

**No firmware:** o controle PS4 comunica-se com o ESP32 exclusivamente via Bluetooth Classic.

---

<a id="c"></a>

## C

<a id="cascata"></a>

### cascata

Estrutura de controle em que a saída de um controlador serve de referência para outro, em série.

**No firmware:** no modo SPEED, o PI de velocidade gera a referência de corrente para o PI de corrente: `RPM_cmd → PI_velocidade → I_cmd → PI_corrente → duty %`.

<a id="ch--cl"></a>

### CH / CL

**C** High / **C** Low, pernas superior e inferior da fase C do inversor.

**No firmware:** GPIO 18 (CH) e GPIO 19 (CL).

<a id="clear-fault"></a>

### clear fault

**Limpar falha**, ação do operador para sair do estado `FAULT` e retornar a `IDLE`.

No firmware, acionada pelo botão Options do PS4, após o hardware de sobrecorrente estar liberado e sem UVLO ativo.

<a id="coast-down"></a>

### coast-down

**Desaceleração por inércia**, período em que o motor desacelera livremente após o PWM ser cortado (disarm), sem frenagem ativa nem energia aplicada ao estator.

Durante o coast-down, a FCEM decai proporcionalmente à velocidade residual do rotor. O firmware não possui estimativa de velocidade neste estado (o `s_measured_rpm` é zerado em `motor_control_on_disarm()`), o que impossibilita condicionar o re-arme ao RPM real do eixo. Esta ausência de medição no coast-down é a razão pela qual a inversão automática de sentido com motor em movimento apresenta risco: o novo ciclo de ALIGN é iniciado sem conhecimento da velocidade residual. Ver também: [arm](#arm), [disarm](#disarm), [ALIGN](#align).

<a id="current"></a>

### CURRENT

Modo de controle em que o gatilho R2 define diretamente a **corrente alvo** (0–5 A). Também referido como **modo corrente** ou **modo torque** (corrente ≈ torque). Não possui PI de velocidade; o RPM resultante depende da carga e da rampa de comutação em malha aberta.

Selecionado em tempo de compilação com `MOTOR_CONTROL_USE_SPEED_MODE 0`. Oposto do modo [SPEED](#speed). Detalhes comparativos na [Seção 5.5](DOCUMENTACAO_PROGRAMACAO.md#55-modos-de-controle-current-e-speed) da documentação de programação.

<a id="cw--ccw"></a>

### CW / CCW

**C**lock**w**ise / **C**ounter-**C**lock**w**ise (Horário / Anti-horário).

Sentido de rotação do motor. No firmware, alternado pelo botão Circle (○) do PS4 com R2 solto.

---

<a id="d"></a>

## D

<a id="dac"></a>

### DAC

**D**igital-to-**A**nalog **C**onverter (Conversor Digital-Analógico).

Dispositivo que gera uma tensão analógica a partir de um valor digital.

<a id="dac1"></a>

### DAC1

Primeiro conversor digital-analógico do ESP32.

**No firmware:** GPIO 25 gera a tensão Vdac de referência para os comparadores LM339 de sobrecorrente (OCP).

<a id="db"></a>

### dB

**D**eci**b**el, unidade logarítmica de atenuação ou ganho.

**No firmware:** o ADC1 está configurado com atenuação de 12 dB, permitindo leitura até ~3,3 V.

<a id="dead-time"></a>

### dead-time

Intervalo de tempo programado entre o desligamento de uma perna do inversor e o ligamento da perna complementar.

Evita curto-circuito momentâneo no barramento. Neste projeto: 500 ns, configurado no MCPWM.

<a id="debounce"></a>

### debounce

Técnica que exige uma condição estável por um tempo mínimo antes de confirmar um evento.

**No firmware:** o UVLO exige 100 ms contínuos abaixo do limiar de cutoff antes de ativar a proteção.

<a id="disarm"></a>

### disarm

**Desarmar** o ESC, interromper PWM e referência de comando.

O motor deixa de ser energizado. Oposto de [arm](#arm).

<a id="dma"></a>

### DMA

**D**irect **M**emory **A**ccess (Acesso Direto à Memória).

Mecanismo que transfere dados entre periférico e memória sem envolver o processador a cada amostra.

**No firmware:** o ADC1 é lido de forma síncrona (`adc1_get_raw()`), sem DMA, suficiente para a taxa de 1 kHz.

<a id="duty-cycle"></a>

### duty cycle

Razão entre o tempo em que o sinal PWM permanece ligado e o período total, expressa em porcentagem.

Controla a tensão média aplicada ao motor. Teto do projeto: 95 %.

---

<a id="e"></a>

## E

<a id="esc"></a>

### ESC

**E**lectronic **S**peed **C**ontroller (Controlador Eletrônico de Velocidade).

Circuito que comanda a velocidade ou o torque de um motor elétrico, tipicamente variando a tensão ou a corrente aplicada por meio de um inversor trifásico.

<a id="esp-idf"></a>

### ESP-IDF

**Esp**ressif **I**oT **D**evelopment **F**ramework, SDK oficial da Espressif para programar o ESP32.

Fornece drivers de baixo nível (MCPWM, ADC, GPIO, DAC, esp_timer) usados diretamente pelos módulos críticos de potência.

<a id="esp32"></a>

### ESP32

Microcontrolador de 32 bits com Wi-Fi e Bluetooth integrados, fabricado pela Espressif.

**No firmware:** ESP32-WROOM-32 em placa `esp32doit-devkit-v1`.

<a id="esp-prog"></a>

### ESP-Prog

Programador/debugger USB usado para gravar firmware e depurar via JTAG.

Os pinos GPIO 12–15 estão reservados para essa interface.

<a id="esp_timer_task"></a>

### ESP_TIMER_TASK

Modo de despacho do `esp_timer` em que o callback é executado em uma task FreeRTOS, não em uma ISR de hardware.

Permite operações mais complexas (ADC, PI, comutação) no callback da malha de controle.

<a id="esr"></a>

### ESR

**E**quivalent **S**eries **R**esistance (Resistência Série Equivalente).

Resistência parasita presente em todo capacitor real, modelada como um resistor em série com a capacitância ideal. Limita a eficácia do capacitor como filtro de alta frequência, pois a queda de tensão $V_{ESR} = ESR \times I_{ripple}$ se soma à ondulação capacitiva ideal.

**No projeto:** o banco do \textit{Link DC} ($940\,\mu\text{F}$) utiliza capacitores \textit{Low-ESR} para minimizar o aquecimento e a ondulação de tensão no barramento DC durante a comutação de alta corrente. Ver também: [ESL](#esl).

<a id="esl"></a>

### ESL

**E**quivalent **S**eries **I**nductance (Indutância Série Equivalente).

Indutância parasita presente em todo capacitor real, devida às geometrias dos terminais e das trilhas internas. Em altas frequências, a ESL faz o capacitor comportar-se como indutor, limitando sua capacidade de absorver transientes rápidos de corrente ($di/dt$).

**No projeto:** os capacitores cerâmicos de $1\,\mu\text{F}$ do barramento compensam a ESL dos capacitores eletrolíticos de $220\,\mu\text{F}$, fornecendo caminho de baixíssima impedância para harmônicos de alta frequência da comutação. Ver também: [ESR](#esr).

<a id="exti"></a>

### EXTI

**Ext**ernal **I**nterrupt (Interrupção Externa).

Mecanismo que dispara uma rotina (ISR) quando um pino GPIO muda de estado.

**No firmware:** usado no pino OC Trip (GPIO 26) e nos comparadores BEMF ZCD.

---

<a id="f"></a>

## F

<a id="fail-safe"></a>

### fail-safe

Princípio de projeto em que qualquer condição anômala conduz a um estado seguro (desarme do PWM, shutdown dos drivers).

O ESC adota fail-safe em todas as camadas de proteção.

<a id="feedforward"></a>

### feedforward

Contribuição de comando calculada diretamente a partir da referência, sem passar pelo controlador PI.

**No firmware:** em RUN_SPEED, a frequência elétrica de comutação segue a referência de RPM como feedforward.

<a id="frequência-elétrica"></a>

### frequência elétrica

Também representada como \(f_{el}\) ou \(f_e\). Frequência em Hz à qual o campo magnético giratório do estator completa um ciclo elétrico completo de 360°.

A relação entre frequência elétrica e velocidade mecânica é governada pelo número de **pares de polos** \(p\) do motor:

\[
f_e = \frac{p \times n}{60} \quad \Leftrightarrow \quad n = \frac{f_e \times 60}{p}
\]

onde \(n\) é a velocidade em RPM. Para o motor A2212/10T (\(p = 7\)): 300 Hz elétricos ≡ ≈ 2571 RPM mecânicos.

**No firmware:** `s_open_loop_comm_hz` armazena a frequência elétrica atual da rampa em malha aberta; `MOTOR_OPEN_LOOP_COMM_HZ_MAX = 300.0f` é o limite para o motor A2212/10T. Ver também: [pares de polos](#pares-de-polos), [A2212](#a2212).

<a id="fault"></a>

### FAULT

Estado de **falha** da FSM do ESC (`ESC_STATE_FAULT`).

PWM desarmado; operação bloqueada até [clear fault](#clear-fault) explícito. Indicado na telemetria como `[FAULT]` e na lightbar do PS4 em vermelho.

<a id="foc"></a>

### FOC

**F**ield-**O**riented **C**ontrol (Controle de Campo Orientado).

Técnica avançada de controle vetorial que alinha o campo magnético do estator com o rotor para torque suave e eficiente.

**No firmware:** não implementado; escopo futuro. Diferente da comutação trapezoidal 6-step.

<a id="freertos"></a>

### FreeRTOS

Sistema operacional de tempo real de código aberto que gerencia tasks, filas e temporizadores.

Subjaz ao framework Arduino no ESP32. A malha de controle executa em uma task despachada pelo `esp_timer`.

<a id="fsm"></a>

### FSM

**F**inite **S**tate **M**achine (Máquina de Estados Finitos).

Modelo de software em que o sistema ocupa um entre vários estados discretos e transita entre eles por regras explícitas.

**No firmware:** `fsm_system` gerencia INIT, IDLE, RUNNING e FAULT; `motor_control` possui sub-FSM de partida.

<a id="fsm_system"></a>

### fsm_system

Módulo da camada de aplicação que implementa a FSM de alto nível do ESC.

Orquestra inicialização, arm/disarm e resposta a falhas. Não executa comutação nem cálculo de PI.

---

<a id="g"></a>

## G

<a id="gpio"></a>

### GPIO

**G**eneral **P**urpose **I**nput/**O**utput (Entrada/Saída de Uso Geral).

Pinos do microcontrolador configuráveis como entrada digital, saída digital ou função de periférico.

**No firmware:** mapeamento centralizado em `board_config.h`; abstraídos por `hal_gpio`.

---

<a id="h"></a>

## H

<a id="hal"></a>

### HAL

**H**ardware **A**bstraction **L**ayer (Camada de Abstração de Hardware).

Camada de software que encapsula o acesso direto aos periféricos do microcontrolador, expondo uma API simples em C puro.

**No firmware:** módulos `hal_pwm`, `hal_adc`, `hal_gpio`, `hal_dac`.

<a id="handover"></a>

### handover

Transição controlada de um modo de operação para outro.

**No firmware:** passagem da comutação em malha aberta para malha fechada por ZCD/BEMF quando velocidade e duty são suficientes.

<a id="hid"></a>

### HID

**H**uman **I**nterface **D**evice (Dispositivo de Interface Humana).

Perfil Bluetooth/USB para periféricos de entrada como gamepads.

**No firmware:** o DualShock 4 opera como HID via Bluepad32.

<a id="high-side--low-side"></a>

### high-side / low-side

Perna **superior** (high-side) conecta a fase ao barramento positivo; perna **inferior** (low-side) conecta a fase ao negativo.

Cada fase do inversor possui um par complementar (ex.: AH/AL). Ver também: [IR2110](#ir2110).

<a id="hz--khz"></a>

### Hz / kHz

**H**ert**z** / **k**ilo**h**ert**z**, unidades de frequência (ciclos por segundo).

**No firmware:** PWM de comutação a 20 kHz; malha de controle a 1 kHz; rampa de frequência elétrica de 5 a 120 Hz.

<a id="idle"></a>

### IDLE

Estado **ocioso** da FSM (`ESC_STATE_IDLE`).

ESC pronto para operação; PWM desarmado; aguarda comando de [arm](#arm) via R2. Telemetria: `[IDLE]`; lightbar azul.

---

<a id="i"></a>

## I

<a id="ina240"></a>

### INA240

Amplificador de corrente da Texas Instruments (CI INA240).

Mede a queda de tensão no shunt de corrente de fase e entrega sinal amplificado ao ADC.

**No firmware:** driver `ina240_current_sensors` converte mV do ADC em ampères.

<a id="init"></a>

### INIT

Estado transitório de **inicialização** no boot do ESC.

Executa `fsm_system_init()`, configuração de periféricos e drivers. Transita para IDLE se bem-sucedido, ou FAULT em caso de erro.

<a id="ir2110"></a>

### IR2110

Driver de gate de meia-ponte da Infineon/IR.

Isola e amplifica os sinais do ESP32 para acionar os MOSFETs de potência. Possui pino SD (shutdown) ativo em nível baixo.

<a id="iram_attr"></a>

### IRAM_ATTR

Atributo do compilador que coloca uma função na RAM interna de instrução (IRAM).

Usado em ISRs para evitar falhas quando a flash está ocupada por operações de cache.

<a id="isense"></a>

### Isense

Sinal de **corrente de fase** (current sense) lido pelos amplificadores INA240.

**No firmware:** GPIO 34, 35 e 36 (fases A, B e C).

<a id="isr"></a>

### ISR

**I**nterrupt **S**ervice **R**outine (Rotina de Serviço de Interrupção).

Função executada automaticamente pelo hardware quando ocorre um evento de interrupção.

**No firmware:** ISR do OC Trip desarma PWM imediatamente; processamento elaborado é diferido para a FSM.

---

<a id="j"></a>

## J

<a id="jitter"></a>

### jitter

Variação irregular no tempo entre execuções sucessivas de uma rotina.

O `loop()` Arduino acumula jitter; por isso a malha de controle usa `esp_timer` com período fixo de 1 ms.

<a id="jtag"></a>

### JTAG

**J**oint **T**est **A**ction **G**roup, interface padrão de depuração e programação de circuitos integrados.

**No firmware:** GPIO 12–15 reservados para JTAG/ESP-Prog.

---

<a id="k"></a>

## K

<a id="kconfig"></a>

### Kconfig

Sistema de configuração do ESP-IDF baseado em menus e arquivos `.defaults`.

**No firmware:** `sdkconfig.defaults` define opções como desabilitar o console USB do Bluepad32.

---

<a id="l"></a>

## L

<a id="l2"></a>

### L2

Gatilho **esquerdo** do DualShock 4 (botão de freio na API Bluepad32: `brake()`).

**No firmware:** não participa do controle do ESC; apenas o R2 (`throttle()`) é usado.

<a id="ldf"></a>

### LDF

**L**ibrary **D**ependency **F**inder, ferramenta do PlatformIO que detecta e compila bibliotecas em `lib/` automaticamente.

<a id="ledc"></a>

### LEDC

**LED** **C**ontroller, periférico PWM do ESP32 voltado a dimming de LEDs.

**No firmware:** não utilizado; o MCPWM foi escolhido por suporte nativo a dead-time e modos complementares.

<a id="lipo"></a>

### LiPo

**Li**thium **Po**lymer, tipo de bateria recarregável de íons de lítio em invólucro polimérico.

**No firmware:** packs de 4S a 6S detectados automaticamente no boot pelo `battery_monitor`.

<a id="lm339"></a>

### LM339

Comparador analógico quadruplo de baixo custo.

**No firmware:** compara sinais de corrente (via INA240) com a referência Vdac; saída wired-OR no pino OC Trip.

---

<a id="m"></a>

## M

<a id="malha-aberta--fechada"></a>

### malha aberta / fechada

**Malha aberta:** o controlador não usa medição de saída para corrigir o comando (ex.: comutação por temporizador).

**Malha fechada:** a medição realimenta o controlador (ex.: PI de corrente, comutação por ZCD).

<a id="mcpwm"></a>

### MCPWM

**M**otor **C**ontrol **P**WM, periférico PWM do ESP32 para controle de motores.

Gera até seis saídas sincronizadas com dead-time programável. Usado em `hal_pwm.c`.

<a id="mcu"></a>

### MCU

**M**icro**c**ontroller **U**nit (Unidade de Microcontrolador).

O circuito integrado que executa o firmware. Neste projeto: ESP32-WROOM-32.

<a id="motor_control"></a>

### motor_control

Módulo central da camada de controle.

Integra temporizador 1 kHz, malhas PI de corrente/velocidade, comutação 6-step, partida (ALIGN → RUN) e detecção de stall.

<a id="motor_open_loop_comm_hz_max"></a>

### MOTOR_OPEN_LOOP_COMM_HZ_MAX

Macro de `board_config.h` que define a **frequência elétrica máxima** da rampa de comutação em malha aberta, em Hz (`float`).

Governa o limite superior da aceleração forçada antes de FCEM suficiente para handover ao ZCD. Um valor excessivamente alto causa perda de sincronismo magnético (stall) porque o campo giratório do estator acelera mais rápido do que o rotor consegue acompanhar dado seu momento de inércia e o torque eletromagnético disponível. Um valor excessivamente baixo pode não atingir a velocidade mínima de leitura confiável dos comparadores.

**Motor de teste A2212/10T 1400kV:** `300.0f` Hz → teto mecânico de ≈ 2571 RPM (\(300 \times 60 / 7\)).

Ver também: [frequência elétrica](#frequência-elétrica), [MOTOR_POLE_PAIRS](#motor_pole_pairs), [malha aberta / fechada](#malha-aberta--fechada), [stall](#stall).

<a id="motor_pole_pairs"></a>

### MOTOR_POLE_PAIRS

Macro de `board_config.h` que define o número de **pares de polos magnéticos** do motor (`uint`).

É o parâmetro físico mais crítico para a relação eletromecânica do motor: um valor errado resulta em estimativa de RPM completamente incorreta e em dessincronismo permanente entre o campo do estator e os ímãs do rotor. Determina a conversão entre frequência elétrica de comutação e velocidade mecânica:

\[
n \text{ [RPM]} = \frac{f_{el} \text{ [Hz]} \times 60}{p}
\]

**Motor de teste A2212/10T 1400kV (14 polos magnéticos):** `7U`.

Ver também: [pares de polos](#pares-de-polos), [frequência elétrica](#frequência-elétrica), [A2212](#a2212).

<a id="ms--ns"></a>

### ms / ns

**M**ili**s**egundo / **n**ano**s**egundo, unidades de tempo.

**No firmware:** malha de controle a 1 ms; dead-time de 500 ns; debounce UVLO de 100 ms.

<a id="mutex"></a>

### mutex

Mecanismo de sincronização que garante acesso exclusivo a um recurso compartilhado entre tasks.

**No firmware:** proibido bloquear mutexes dentro de ISRs.

---

<a id="n"></a>

## N

*(Nenhuma entrada nesta letra.)*

---

<a id="o"></a>

## O

<a id="oc"></a>

### OC

**O**ver**c**urrent (Sobrecorrente).

Condição em que a corrente do motor excede um limiar seguro.

<a id="oc-trip"></a>

### OC Trip

Sinal de **disparo de sobrecorrente**, entrada digital ativa em nível baixo quando um comparador LM339 detecta corrente acima do limiar.

**No firmware:** GPIO 26, com interrupção EXTI na borda de descida.

<a id="ocp"></a>

### OCP

**O**ver**c**urrent **P**rotection (Proteção contra Sobrecorrente).

Mecanismo que desarma o inversor quando a corrente excede o limite. Implementado em hardware (LM339 + ISR) e em software (`motor_control_tick`).

<a id="off--source--sink"></a>

### OFF / SOURCE / SINK

Modos de condução de cada fase do inversor:

- **OFF:** ambas as pernas desligadas; fase flutuante.
- **SOURCE:** PWM na perna high-side; low-side complementar.
- **SINK:** low-side condutora contínua; high-side desligada.

Mapeiam a tabela de comutação 6-step.

<a id="open_loop"></a>

### OPEN_LOOP

Comutação em **malha aberta**, os passos 6-step avançam por temporizador, sem feedback de posição do rotor.

Modo padrão com `BOARD_ENABLE_BEMF_ZCD 0`.

<a id="outrunner"></a>

### outrunner

Configuração construtiva de motor BLDC em que o **rotor (ímãs permanentes) envolve externamente o estator (bobinas fixas)**.

Característica: diâmetro externo maior, relação K_V mais baixa, número de polos tipicamente maior que motores inrunner de mesma classe de potência. Exemplo: motor A2212/10T 1400kV possui 14 polos na carcaça giratória externa. Contrasta com o [inrunner](#bldc) (rotor interno), como o Turnigy XK3674-2200KV de 4 polos.

**Implicação de firmware:** o maior número de pares de polos (\(p = 7\) vs \(p = 2\)) eleva a frequência elétrica de comutação necessária para uma mesma velocidade mecânica, exigindo ajuste dos parâmetros `MOTOR_POLE_PAIRS` e `MOTOR_OPEN_LOOP_COMM_HZ_MAX`.

<a id="overshoot"></a>

### overshoot

Ultrapassagem da referência pelo valor controlado após uma mudança de comando.

O anti-windup do PI reduz overshoot quando a saída estava saturada.

---

<a id="p"></a>

## P

<a id="pcb"></a>

### PCB

**P**rinted **C**ircuit **B**oard (Placa de Circuito Impresso).

Placa física que interliga o ESP32, drivers IR2110, sensores e conectores. Documentada em `Hardware/PCB_Project/`.

<a id="pares-de-polos"></a>

### pares de polos

Número de **pares de polos magnéticos** do rotor de um motor BLDC, denotado \(p\). Cada par é composto por um polo Norte e um polo Sul adjacentes no rotor. O número total de polos é \(2p\).

Este parâmetro é fundamental para a conversão eletromecânica: governa quantas vezes o campo magnético do estator precisa completar um ciclo elétrico para que o rotor avance um giro mecânico completo:

\[
f_e = p \cdot \frac{n}{60}
\]

onde \(f_e\) é a frequência elétrica de comutação (Hz) e \(n\) a velocidade mecânica (RPM).

| Motor | Polos totais | Pares de polos \(p\) | `MOTOR_POLE_PAIRS` |
|-------|-------------|----------------------|--------------------|
| Turnigy XK3674-2200KV (inrunner) | 4 | 2 | `2U` |
| **A2212/10T 1400kV (outrunner)** | **14** | **7** | **`7U`** |

**No firmware:** macro `MOTOR_POLE_PAIRS` em `board_config.h`; usada na conversão RPM ↔ f_el e na estimativa de RPM a partir do intervalo entre passos de comutação. Ver também: [frequência elétrica](#frequência-elétrica), [MOTOR_POLE_PAIRS](#motor_pole_pairs), [outrunner](#outrunner).

<a id="pi"></a>

### PI

Controlador **P**roporcional-**I**ntegral, algoritmo de malha fechada que combina resposta proporcional ao erro atual e acumulação integral do erro ao longo do tempo.

**No firmware:** implementado em `pid_regulator`; instâncias `s_current_pi` e `s_speed_pi`.

<a id="pid_regulator"></a>

### pid_regulator

Módulo que implementa o controlador PI com anti-windup.

Agnóstico de hardware, processa apenas grandezas `float` (referência, medição, limites).

<a id="platformio"></a>

### PlatformIO

Ambiente de desenvolvimento e build para microcontroladores, usado para compilar e gravar o firmware.

Configuração em `platformio.ini`; compilação com `pio run`.

<a id="polling"></a>

### polling

Técnica em que o software verifica periodicamente o estado de um dispositivo, em vez de esperar uma interrupção.

**No firmware:** o PS4 é lido por polling a cada 20 ms no `loop()`.

<a id="power-cycle"></a>

### power-cycle

**Reinicialização por energia**, desligar e religar a alimentação do equipamento.

Necessário para redetectar o número de células LiPo após troca de pack (4S ↔ 6S).

<a id="ps4"></a>

### PS4

**P**lay**S**tation **4**, console Sony; aqui, o controle DualShock 4 usado como interface de comando do ESC via Bluetooth.

<a id="pwm"></a>

### PWM

**P**ulse **W**idth **M**odulation (Modulação por Largura de Pulso).

Técnica que varia a razão de tempo ligado/desligado de um sinal digital para controlar potência média.

---

<a id="r"></a>

## R

<a id="r2"></a>

### R2

Gatilho **direito** do DualShock 4.

**No firmware:** lido via `throttle()` na API Bluepad32; arma o ESC e define referência de corrente ou RPM.

<a id="rc"></a>

### RC

Circuito **R**esistor-**C**apacitor, filtro passivo que atenua ruído de alta frequência.

Usado nos sensores BEMF para filtrar harmônicas do PWM antes dos comparadores.

<a id="rgb"></a>

### RGB

**R**ed, **G**reen, **B**lue, modelo de cor por três canais.

**No firmware:** a lightbar do PS4 indica o estado da FSM (âmbar=INIT, azul=IDLE, verde=RUNNING, vermelho=FAULT).

<a id="rpm"></a>

### RPM

**R**evolutions **P**er **M**inute (Rotações por Minuto).

Unidade de velocidade mecânica do motor. Faixa de comando no modo SPEED: **0–2571 RPM** com o motor A2212/10T 1400kV (`MOTOR_POLE_PAIRS = 7U`, `MOTOR_OPEN_LOOP_COMM_HZ_MAX = 300.0f` Hz). O valor máximo é recalculado automaticamente pelo firmware com base nesses parâmetros. Ver também: [SPEED](#speed), [MOTOR_OPEN_LOOP_COMM_HZ_MAX](#motor_open_loop_comm_hz_max), [A2212](#a2212).

<a id="run--run_open--run_speed"></a>

### RUN / RUN_OPEN / RUN_SPEED

Fases da sequência de partida após ALIGN:

- **RUN:** PI de corrente ativo com rampa de frequência elétrica (modo CURRENT).
- **RUN_OPEN:** rampa em malha aberta com corrente fixa de 0,5 A (modo SPEED).
- **RUN_SPEED:** PI de velocidade ativo com feedforward de frequência elétrica (modo SPEED).

<a id="running"></a>

### RUNNING

Estado de **operação** da FSM (`ESC_STATE_RUNNING`).

PWM armado; `motor_control` ativo. Telemetria: `[RUNNING]`; lightbar verde.

---

<a id="s"></a>

## S

<a id="sd"></a>

### SD

**S**hut**d**own, sinal de desligamento dos drivers IR2110, ativo em nível baixo.

**No firmware:** GPIO 32, 33 e 4; em falha, colocados em LOW antes de desarmar o PWM.

<a id="sdk"></a>

### SDK

**S**oftware **D**evelopment **K**it, conjunto de ferramentas, bibliotecas e documentação para desenvolver software para uma plataforma.

Ex.: ESP-IDF é o SDK da Espressif.

<a id="sensorless"></a>

### sensorless

Controle de motor **sem sensores de posição** dedicados (Hall, encoder).

A posição do rotor é inferida pela BEMF ou por sequência de partida em malha aberta.

<a id="setpoint"></a>

### setpoint

**Ponto de ajuste**, valor de referência que o controlador PI tenta alcançar (corrente em A ou RPM).

Definido pelo gatilho R2 ou zerado ao desarmar.

<a id="slew-rate"></a>

### slew rate

**Taxa de variação**, limite imposto à velocidade de mudança de uma referência ou sinal ao longo do tempo, expresso em unidade por segundo (A/s ou RPM/s).

No firmware, o slew rate limiter é aplicado à referência de corrente e à referência de velocidade antes de alimentar o controlador PI, prevenindo variações abruptas que causariam sobrecorrente transitória ou disparo da proteção OCP. A cada ciclo da malha de controle ($\Delta t = 1\,\text{ms}$), o incremento máximo permitido é $\Delta I_{max} = \text{MOTOR\_TARGET\_SLEW\_AMPS\_PER\_S} \times \Delta t$. Um degrau de comando do R2 converte-se, portanto, em uma rampa de referência de duração $\Delta I_{cmd} / \text{slew}$ segundos.

**No firmware:** `MOTOR_TARGET_SLEW_AMPS_PER_S = 2 A/s` (corrente); `MOTOR_SPEED_SLEW_RPM_PER_S = 1500 RPM/s` (velocidade). Ver também: [PI](#pi), [anti-windup](#anti-windup), [setpoint](#setpoint).

<a id="speed"></a>

### SPEED

Modo de controle em que o gatilho R2 define a **velocidade alvo** em RPM. Também referido como **modo velocidade**. Usa malha cascata: PI velocidade → PI corrente; a corrente é adaptada automaticamente à carga (até 5 A).

Faixa de comando: **0–2571 RPM** (motor A2212/10T 1400kV, `MOTOR_POLE_PAIRS = 7U`, `MOTOR_OPEN_LOOP_COMM_HZ_MAX = 300.0f` Hz). Para o motor nominal Turnigy XK3674-2200KV (`MOTOR_POLE_PAIRS = 2U`), a faixa correspondente seria 0–3600 RPM (120 Hz × 60/2). O valor efetivo depende dos parâmetros de `board_config.h`.

Padrão do projeto (`MOTOR_CONTROL_USE_SPEED_MODE 1`). Detalhes comparativos na [Seção 5.5](DOCUMENTACAO_PROGRAMACAO.md#55-modos-de-controle-current-e-speed) da documentação de programação.

<a id="spi"></a>

### SPI

**S**erial **P**eripheral **I**nterface, barramento serial síncrono para comunicação com periféricos.

**No firmware:** GPIO 6–11 reservados para flash SPI interna do ESP32.

<a id="stall"></a>

### stall

**Dessincronismo**, o rotor perde sincronismo com a sequência de comutação 6-step.

Detectado por corrente elevada sustentada, ausência de avanço de passo ou RPM muito baixo com comando alto.

---

<a id="t"></a>

## T

<a id="tcc"></a>

### TCC

**T**rabalho de **C**onclusão de **C**urso, monografia de graduação.

Este firmware e sua documentação subsidiam o TCC em Engenharia Elétrica.

<a id="trapezoidal"></a>

### trapezoidal

Comutação em forma de **onda trapezoidal**, sequência 6-step que energiza duas fases por vez com terceira flutuante.

Método implementado no `motor_control` v1; distinto do FOC senoidal.

---

<a id="u"></a>

## U

<a id="uart"></a>

### UART

**U**niversal **A**synchronous **R**eceiver-**T**ransmitter, interface serial assíncrona.

**No firmware:** `Serial` a 115200 baud emite telemetria de diagnóstico; não aceita comandos interativos.

<a id="uvlo"></a>

### UVLO

**U**nder-**V**oltage **L**ock-**O**ut (Bloqueio por Subtensão).

Proteção que impede operação quando a tensão do pack LiPo cai abaixo do limiar seguro (3,3 V/célula com histerese de recuperação a 3,5 V).

---

<a id="v"></a>

## V

<a id="vbat"></a>

### VBAT

Tensão da **bateria** (battery voltage), tensão do barramento DC medida pelo divisor resistivo no GPIO 39.

<a id="vdac"></a>

### Vdac

Tensão de referência **analógica** gerada pelo DAC1 (GPIO 25) para os comparadores LM339 de OCP.

Calculada conforme o limiar de corrente desejado (ex.: 8 A → ~1,81 V).

<a id="volatile"></a>

### volatile

Qualificador C/C++ que indica ao compilador que uma variável pode mudar a qualquer momento fora do fluxo normal do programa.

Essencial em flags escritas por ISRs e lidas no loop principal (ex.: `s_fault_pending`).

---

<a id="w"></a>

## W

<a id="wi-fi"></a>

### Wi-Fi

Protocolo de rede sem fio de longo alcance integrado ao ESP32.

**No firmware:** não utilizado; o rádio Bluetooth permanece ativo para o PS4, o que restringe o uso do ADC2.

<a id="wired-or"></a>

### wired-OR

Ligação em que múltiplas saídas compartilham um mesmo fio; qualquer uma em nível baixo puxa o sinal para baixo.

**No firmware:** saídas dos comparadores LM339 conectam-se em wired-OR no pino OC Trip.

---

<a id="z"></a>

## Z

<a id="zcd"></a>

### ZCD

**Z**ero **C**rossing **D**etection (Detecção de Cruzamento por Zero).

Técnica que identifica o instante em que a BEMF de uma fase flutuante cruza zero, indicando posição do rotor.

**No firmware:** módulo `bemf_zcd`; opcional via `BOARD_ENABLE_BEMF_ZCD`.

<a id="zcd_closed"></a>

### ZCD_CLOSED

Comutação em **malha fechada por ZCD**, os passos 6-step avançam após detecção de cruzamento por zero da BEMF, com atraso de 30° elétricos.

Modo `MOTOR_COMM_ZCD_CLOSED`; requer hardware de comparadores dedicado.

---

<a id="6-step"></a>

## 6-step

Método de comutação **trapezoidal** em seis passos (0–5) que energiza sequencialmente as três fases do motor BLDC.

Implementado por tabela estática em `motor_control.c`, mapeando passo e sentido (CW/CCW) aos modos OFF/SOURCE/SINK.

---

*Glossário vinculado à [Documentação de Programação](DOCUMENTACAO_PROGRAMACAO.md), junho de 2026.*
