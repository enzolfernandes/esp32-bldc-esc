# MEMORIA_TCC.md
## Memória Unificada do Trabalho de Conclusão de Curso

---

## Diretriz de Sincronização Contínua

> **REGRA DE MANUTENÇÃO:** Toda vez que o código C++ for alterado ou o texto em LaTeX sofrer revisões de escopo (ex.: mudança de motores, adição de sensores, alteração de limiares de proteção, inclusão de novos módulos de firmware), a IA atuante DEVE obrigatoriamente atualizar a Documentação de Programação (`Firmware/DOCUMENTACAO_PROGRAMACAO.md`) e esta Memória do TCC em paralelo para evitar defasagem técnica entre o estado real do código e os documentos de referência do trabalho.

Esta regra é válida para toda e qualquer sessão de trabalho futura com assistência de IA neste projeto. A defasagem entre código e documentação invalida a rastreabilidade do projeto e compromete a integridade do Trabalho de Conclusão de Curso.

---

**Título:** Desenvolvimento e Análise de um Controlador Eletrônico de Velocidade para Motores de Corrente Contínua Sem Escovas  
**Autor:** Enzo Luiz Fernandes  
**Instituição:** UNESP – Faculdade de Engenharia de Ilha Solteira – Departamento de Engenharia Elétrica  
**Orientador:** Prof. Dr. Guilherme de Azevedo e Melo  
**Ano:** 2025  

---

## Resumo: Capítulo 1 — Introdução Teórica

### Premissas e Objetivos

Este capítulo estabelece a fundamentação teórica do sistema, partindo do problema central: motores BLDC são fisicamente máquinas síncronas de imãs permanentes (PMSM) alimentadas por corrente alternada sintetizada, mas seu comportamento externo pode ser modelado como um motor DC idealizado — desde que o ESC execute a comutação eletrônica corretamente. O capítulo resolve esse paradoxo e constrói o arcabouço matemático completo (Força de Lorentz, Lei de Faraday, equações de circuito elétrico e mecânico) que fundamenta todas as decisões de projeto subsequentes.

O problema de engenharia central abordado é: **como caracterizar, modelar e controlar a conversão eletromecânica de um motor BLDC para projetar um ESC funcional?**

---

### Hardware e Firmware

#### Princípio Físico e Modelo do Motor

- **Força de Lorentz:** força sobre condutor percorrido por corrente em campo magnético; torque máximo ocorre quando o ângulo entre campo do estator e do rotor é 90° elétricos — condição que o ESC deve manter continuamente.
- **FCEM (Força Contraeletromotriz / back-EMF):** tensão induzida pela Lei de Faraday; sua magnitude é diretamente proporcional à velocidade angular (ω). É o mecanismo que limita a velocidade máxima do motor e o sinal que o ESC sensorless utiliza para estimar a posição do rotor.
- **Equação elétrica do motor:** V = e_a + I_a·R_a + L_a·(dI_a/dt) — tensão aplicada é consumida pela FCEM, pela queda resistiva e pela queda indutiva da armadura.
- **Equação mecânica do motor:** T_e = T_l + J·(dω/dt) + B·ω — torque eletromagnético é repartido entre torque de carga, inércia e atrito viscoso.
- **Constante de Torque (K_T):** razão linear entre torque eletromagnético e corrente de armadura [Nm/A]. Determinada pelo campo magnético dos imãs (ex: Neodímio), número de espiras e geometria.
- **Constante de FCEM (K_e):** razão linear entre FCEM gerada e velocidade angular [V·s/rad].
- **Constante de Velocidade (K_V):** parâmetro comercial em RPM/V; é a inversão de K_e. Relação de conversão SI: K_T [Nm/A] = 2π / (60 · K_V [RPM/V]).
- **Constante do Motor (K_m):** K_T / √(R_a), em Nm/√W — figura de mérito para comparar eficiência de projetos; invariante a mudanças de enrolamento para mesma carcaça.
- **Equivalência K_T = K_e (em unidades SI):** demonstrada por balanço de potência — a potência elétrica convertida (e_a · I_a) é igual à potência mecânica (T_e · ω), provando que o mesmo mecanismo de acoplamento eletromagnético que gera torque a partir de corrente (princípio motor) é o que gera tensão a partir de movimento (princípio gerador).
- **Efeito da temperatura em K_T e K_e:** aquecimento causa desmagnetização parcial dos imãs permanentes → redução de K_T e K_e → degradação de performance (menos torque por ampere, menor FCEM por rpm).

#### Topologia do ESC e Algoritmos de Controle

- **Ponte Inversora Trifásica (VSI):** circuito base do ESC; seis transistores (três braços, dois por braço — High-Side e Low-Side) conectados ao motor em estrela (Y) ou triângulo (Δ).
- **Comutação de Seis Passos (Controle Trapezoidal):** dois transistores conduzem simultaneamente a cada passo; cada transistor conduz 120° elétricos; as fases alternam a cada 60° elétricos — gera campo magnético giratório discreto.
- **Modulação por Largura de Pulso (PWM):** a própria ponte trifásica ajusta o duty cycle para controlar a tensão média entregue ao motor e, portanto, sua velocidade.
- **Controle Sensorless em Malha Fechada (ZCD):** detecção do cruzamento por zero da FCEM na fase não energizada para determinar a posição do rotor e sincronizar a comutação. A comutação ideal ocorre 30° elétricos após o cruzamento por zero.
- **Ondulação de Torque (Torque Ripple):** consequência intrínseca da comutação de seis passos. Frequência de pulsação = 6× a frequência elétrica fundamental. Causas: (1) incompatibilidade entre a forma de onda real da FCEM (não trapezoidal ideal) e a corrente injetada (não retangular ideal); (2) transientes de comutação — a indutância dos enrolamentos impede variação instantânea de corrente, causando desalinhamento momentâneo dos campos durante a transição de fase, com queda/pico de torque a cada 60° elétricos.

#### Estratégia de Partida Sensorless

- **Limitação do ZCD em velocidade zero:** FCEM = K_e · ω → em ω=0 não há FCEM detectável, tornando impossível determinar a posição do rotor para a comutação inicial.
- **Fase 1 — Alinhamento:** energização de um par específico de fases com corrente contínua por tempo pré-determinado, forçando os imãs do rotor a se alinharem com o campo estático do estator (posição inicial θ₀ conhecida). O objetivo é que o primeiro passo dinâmico seja disparado com o estator orientado a 90° do rotor, maximizando o torque de arranque.
- **Fase 2 — Comutação Forçada (Rampa de Aceleração):** comutação em malha aberta com frequência crescente (perfil de rampa pré-programado), sem realimentação de posição, até que a FCEM atinja amplitude detectável (tipicamente ~5% da velocidade nominal).
- **Riscos da Partida Forçada:** perda de sincronismo (stall) se a carga exceder o torque disponível ou se a rampa for excessivamente agressiva; picos de corrente elevados por ausência de FCEM limitadora; vibração e ineficiência por desalinhamento constante entre campos.

#### Eletrônica de Potência

- **MOSFET de Potência:** chave semicondutora que opera entre corte e saturação. Perdas por condução: P_cond = I_D² · R_DS(on). Perdas por comutação: P_sw proporcional à frequência de chaveamento e à carga total de gate (Q_g).
- **Barramento DC (Link DC) e Indutância Parasita:** cabos de alimentação introduzem indutância parasita (L_par); durante comutação, o di/dt gera picos de tensão (V_spike = L_par · di/dt) que podem destruir os semicondutores. Solução: banco de capacitores de baixa ESR posicionado próximo aos drenos dos MOSFETs.
- **Dimensionamento do banco de capacitores do Link DC:** regra prática de ~220µF por 20A de corrente de fase. A ESR total do banco deve ser minimizada via associação paralela para suportar a corrente de ripple RMS.
- **Técnica de Bootstrap para acionamento High-Side:** o capacitor de bootstrap (C_boot) é carregado quando o transistor Low-Side conduz e atua como "bateria flutuante" para acionar o gate do transistor High-Side acima de V_CC. Limitação: não permite duty cycle de 100% indefinidamente (C_boot precisa ser recarregado periodicamente). Dimensionamento: C_boot ≥ Q_tot / ΔV_boot; recomenda-se capacidade de pelo menos 15×Q_g do MOSFET.

#### Estado da Arte em Algoritmos de Controle

- **ZCD (Detecção de Cruzamento por Zero):** simples e eficaz para médias e altas rotações. Técnica adotada neste trabalho.
- **Integração da FCEM:** atenua ruídos de comutação (filtro natural pela integração), mais robusto que ZCD puro.
- **FOC (Field-Oriented Control):** mantém o vetor de corrente do estator sempre ortogonal ao rotor (90° elétricos) de forma contínua, maximizando torque por ampere e eliminando torque ripple. Requer parâmetros precisos do motor (R_s, L_d, L_q, λ_m, J, B, P).
- **Observadores de Estado (Luenberger):** estimam corrente e FCEM a partir das equações diferenciais do motor para controle sensorless de alto desempenho.
- **Filtro de Kalman Estendido (EKF):** abordagem estocástica para estimação de posição robusta em ambientes ruidosos; elevado custo computacional.
- **Injeção de Alta Frequência:** útil em velocidade zero/baixa (onde FCEM é nula); explora a saliência magnética (L_d ≠ L_q) para detectar posição do rotor parado.

#### Parâmetros Críticos para Modelagem (necessários para FOC/Observadores)

- R_s (resistência de estator), L_s / L_d / L_q (indutâncias), λ_m (fluxo dos imãs), J (inércia), B (atrito), P (número de polos).

---

### Metodologia

- Desenvolvimento exclusivamente teórico-analítico: derivação das equações do motor, demonstração da equivalência K_T = K_e por balanço de potência, análise das fontes de torque ripple, análise das causas de falha na partida sensorless.
- Comparação técnica BLDC vs Motor de Indução Trifásico (MIT): BLDC superior em densidade de potência, eficiência em carga variável e controle preciso; MIT superior em custo inicial e simplicidade construtiva.
- Tabela de conversão de unidades de constantes de motor (SI, oz-in/A, RPM/V, V/krpm).

---

### Ganchos para Resultados

1. **Validação do Torque Ripple:** a teoria prevê pulsações de torque a cada 60° elétricos. Os experimentos práticos deverão quantificar a amplitude dessas pulsações no motor Turnigy XK3674 operando com o ESC desenvolvido.
2. **Eficácia da Partida Sensorless:** a teoria estabelece que alinhamento inadequado ou rampa excessivamente agressiva causa stall. Os testes deverão validar os parâmetros de tempo/corrente de alinhamento e o perfil da rampa que garantem partida confiável.
3. **Transição Malha Aberta → Malha Fechada:** confirmar que o limiar de velocidade (~5% da nominal) para ativação do ZCD é atingido de forma confiável e que o handover não causa oscilações ou perda de sincronismo.
4. **Impacto do Efeito Térmico:** a teoria prevê degradação de K_T e K_V com temperatura. Os testes em regime contínuo deverão verificar se há variação observável de desempenho com o aquecimento dos imãs.

---

## Resumo: Capítulo 2 — Objetivos

### Premissas e Objetivos

**Objetivo único e central do TCC:** estudar o princípio de funcionamento de um controlador de motores Brushless e desenvolver um protótipo funcional capaz de oferecer operação suave e precisa em diferentes cenários de aplicação.

Este capítulo é declarativo, não introduz nova informação técnica. Serve como contrato formal de entregáveis do projeto.

---

### Ganchos para Resultados

1. **"Operação suave"**: deverá ser quantificada nos resultados através da medição da ondulação de torque, da ausência de stall durante a partida e da estabilidade da velocidade em regime.
2. **"Precisa"**: deverá ser quantificada pelo desvio entre a velocidade comandada (duty cycle do PWM) e a velocidade real medida.
3. **"Diferentes cenários de aplicação"**: implica testes com variação de carga e variação de velocidade setpoint.

---

## Resumo: Capítulo 3 — Metodologia

### Premissas e Objetivos

Este capítulo resolve o problema de engenharia de sistemas: **como especificar, dimensionar e integrar fisicamente todos os subsistemas do ESC** (potência, controle, sensoriamento e firmware) de forma a operar com segurança, eficiência e robustez com o motor e bateria selecionados.

A abordagem adotada é top-down: especificação da carga → dimensionamento da fonte → dimensionamento da potência → projeto do controle → simulação → prototipagem.

---

### Hardware e Firmware

#### Motor (Carga)

- **Modelo:** Turnigy XK3674-2200KV
- **Tipo:** Inrunner, 4 polos
- **K_V:** 2200 RPM/V
- **Tensão nominal:** 25,2 V (bateria 6S carregada)
- **Corrente nominal:** 70 A
- **Potência máxima:** 1750 W
- **Velocidade a vazio (25,2V):** 55.440 RPM teórico
- **Velocidade a vazio (22,2V nominal bateria):** 48.840 RPM (~12% de redução)
- **Torque a vazio:** 0,3 Nm (sem efeitos dissipativos)
- **Conexão interna:** triangulo ou estrela sem acesso ao ponto neutro físico

#### Bateria (Fonte de Energia)

- **Modelo:** Turnigy Graphene Panther 1300mAh 6S 75C
- **Configuração:** 6 células LiPo em série (6S)
- **Tensão nominal:** 22,2 V (6 × 3,7V)
- **Tensão máxima (carregada):** 25,2 V (6 × 4,2V)
- **Capacidade:** 1300 mAh
- **Taxa de descarga:** 75C
- **Corrente máxima de descarga:** ~97,5 A (1,3 Ah × 75C) — superior à margem de 91 A requerida
- **Critério de dimensionamento:** corrente nominal do motor (70A) + margem de 30% para transitórios (rotor travado, inversão) = 91A mínimos

#### Sistema de Proteção Passiva (Fusível)

- **Tipo:** Fusível MIDI 100 A
- **Instalação:** externo à PCB, em série com o cabo positivo da bateria (off-board)
- **Justificativa:** terminais MIDI com fixação por parafuso (M5/M6) minimizam R_contato sob alta corrente; instalação off-board protege trilhas e facilita manutenção
- **Margem de segurança:** ~10% acima da corrente de pico estimada (90A)

#### Banco de Capacitores do Barramento DC (Link DC)

- **Critério de projeto:** ~220µF por 20A de corrente de fase (regra prática para ESCs de alta performance)
- **Capacitância calculada:** C_bus ≈ (90A/20) × 220µF ≈ 990µF
- **Topologia implementada:** 2× capacitores eletrolíticos Low-ESR de 470µF/35V em paralelo → total: 940µF
- **Conector de entrada:** XT90 anti-centelha de alta corrente
- **Posicionamento:** o mais próximo possível dos drenos dos MOSFETs High-Side para minimizar o loop de indutância

#### MOSFETs da Ponte Inversora

- **Modelo:** IRFS7530TRL7PP
- **Encapsulamento:** D2PAK-7 (múltiplos terminais de Source para reduzir indutância parasita)
- **V_DSS:** 60 V (margem >100% sobre V_barramento máximo de 25,2V)
- **R_DS(on) máximo:** ~1,4 mΩ
- **Potência dissipada estimada (91A):** P = 91² × 0,0014 ≈ 11,6 W por MOSFET
- **Necessidade:** área de cobre estendida na PCB ou dissipador térmico externo

#### Driver de Gate

- **Modelo:** IR2110 (driver de meia-ponte)
- **Tensão de alimentação:** 15 V (gerado pelo BEC)
- **Corrente de pico:** até 2 A para carga/descarga do gate
- **Interface lógica:** alimentado com V_micro Ref (3,3V do ESP32) no pino VDD para compatibilidade de nível lógico
- **Resistor de gate externo:** 10 Ω (valor comercial acima do mínimo teórico de 5,4Ω para margem de segurança e amortecimento de ringing)
- **Tempo de comutação estimado:** ~290 ns (Q_g ≈ 360nC, I_med ≈ 1,24A) — < 1,5% do período de 50µs a 20kHz
- **Tempos de propagação do driver:** t_on = 120ns, t_off = 94ns — representam < 0,5% do ciclo total a 20kHz

#### Circuito de Bootstrap

- **Componentes:** capacitor eletrolítico de 10µF (reserva de carga) em paralelo com cerâmico de 100nF (resposta em alta frequência)
- **Tensão de alimentação:** 15V do driver
- **Carga armazenada:** Q_arm = 10µF × 15V = 150µC
- **Mínimo requerido:** 15 × Q_g = 15 × 360nC = 5,4µC
- **Queda de tensão por ciclo:** ΔV_boot = 360nC / 10µF = 36mV (0,24% — desprezível)

#### Frequência de Chaveamento

- **Valor definido:** 20 kHz
- **Justificativa 1 (perdas térmicas):** MOSFETs IRFS7530 com Q_g elevado — aumentar para 48kHz ou 96kHz causaria dissipação excessiva. A 20kHz, as perdas são gerenciáveis por dissipação passiva.
- **Justificativa 2 (conforto acústico):** 20kHz está na fronteira do limiar auditivo humano, eliminando o zumbido característico de inversores < 12kHz.
- **Justificativa 3 (ripple de corrente):** com L_motor ≈ 20µH e R_motor ≈ 10mΩ (valores típicos classe 3674), τ = L/R ≈ 2ms. Período a 20kHz = 50µs << τ, garantindo operação em modo de condução contínua com baixa ondulação.
- **Validação do driver:** corrente média de gate I_g,avg = Q_g × f_sw = 354nC × 20kHz ≈ 7,08mA — muito abaixo do limite de 2A do IR2110.

#### Fonte de Alimentação Auxiliar (BEC)

- **Módulo:** LM2596 (conversor buck, 150kHz, 3A)
- **Topologia em cascata:** V_bateria (22,2V–25,2V) → 15V (alimenta IR2110) → 5V/3,3V (alimenta ESP32)
- **Critério de seleção:** eficiência superior a 85%, eliminando o peso e volume de uma segunda bateria

#### Sensoriamento de Corrente (Shunt)

- **Topologia:** Low-Side Current Sensing (resistor shunt no ramo inferior da ponte)
- **Justificativa vs Hall:** resposta instantânea para proteção de curto-circuito sem atrasos de propagação
- **Resistência do shunt:** 0,5 mΩ
- **Potência dissipada (91A):** P_shunt ≈ 4,14 W
- **Impacto na eficiência:** η_loss = 4,14W / 2020W ≈ 0,2% — desprezível
- **Tensão de sensoriamento (91A):** V_sense = 45,5 mV — insuficiente para o ADC do ESP32 (0–3,3V)
- **Ganho do amplificador projetado:** A_v = 3,3V / 0,0455V ≈ 73 (resistores de precisão 1%)

#### Microcontrolador

- **Modelo:** ESP32-DevKit C (módulo ESP32-WROOM-32)
- **Clock máximo:** 240 MHz (dual-core, execução paralela de tarefas — crítico para ESC)
- **Resolução PWM:** 16 bits (precisão de controle de velocidade)
- **Níveis lógicos:** 3,3V (requer interface via IR2110 para acionamento dos MOSFETs)
- **Periférico utilizado:** MCPWM (Motor Control PWM) — gera os 6 sinais de controle da ponte com inserção automática de Dead Time para prevenção de shoot-through

#### Dashboard de Telemetria via Wi-Fi (validação sem cabo USB)

- **Motivação:** isolamento galvânico virtual entre circuito de potência e computador do operador; mitigação de ground loops, surtos no barramento e risco à porta USB durante ensaios de potência
- **Arquitetura:** ESP32 em modo AP (`ESC-Dashboard`, `192.168.4.1`); módulo `wifi_telemetry`; ESPAsyncWebServer; LittleFS (`index.html`, `chart.min.js`); HTTP polling `GET /data` (1 s no browser, snapshot atualizado a 100 ms no firmware)
- **Payload JSON:** FSM, correntes, VBAT, RPM, PI, latência tick, heap, `ps4c`/R2/Circle, fault
- **Coexistência:** Wi-Fi inicializado antes do Bluetooth; ADC1 inalterado; WebSocket descartado por consumo de heap (~19 KB/cliente)
- **Deploy:** `pio run -t upload` + `pio run -t uploadfs`

#### Firmware — Máquina de Estados de Comutação

- **Estado 1 — Alinhamento (Estático):**
  - Energização de um par de fases (ex: Fase A High, Fase B Low)
  - Duração: 500 ms (corrente DC constante)
  - Objetivo: posição inicial θ₀ conhecida antes do início do movimento

- **Estado 2 — Aceleração em Malha Aberta (Blind Commutation):**
  - Comutação de seis passos com temporizador interno (sem realimentação)
  - Incremento linear de frequência (rampa)
  - Duração: apenas até 5%–10% da velocidade nominal ser atingida (FCEM detectável acima do limiar de histerese dos comparadores LM339)

- **Estado 3 — Auto-Comutação (Malha Fechada / ZCD):**
  - Interrupção externa disparada a cada cruzamento por zero detectado pelos comparadores
  - Atraso calculado de 30° elétricos após o ZCD antes de executar a próxima comutação
  - Compensação de fase do filtro RC: T_wait = T_30° − T_lag (correção dinâmica a cada ciclo para manter ortogonalidade dos campos)

#### Mapeamento MCPWM — Tabela de Seis Passos

| Passo | Fases | High-Side PWM | Low-Side ON | Fase em Hi-Z |
|-------|-------|---------------|-------------|--------------|
| 1 | A+ B- | T1 (A) | T4 (B) | C |
| 2 | A+ C- | T1 (A) | T6 (C) | B |
| 3 | B+ C- | T3 (B) | T6 (C) | A |
| 4 | B+ A- | T3 (B) | T2 (A) | C |
| 5 | C+ A- | T5 (C) | T2 (A) | B |
| 6 | C+ B- | T5 (C) | T4 (B) | A |

#### Sistema de Realimentação ZCD

- **Reconstrução do Neutro Virtual:** 3 resistores de 33kΩ, um por fase, com extremidades reunidas em nó comum → fornece V_ref (tensão média do sistema) para os comparadores
- **Divisor de tensão para proteção do ADC:** R_high = 33kΩ, R_low = 3,3kΩ → fator de atenuação α ≈ 0,0909; tensão máxima no pino do ESP32: 25,2V × 0,0909 ≈ 2,29V (abaixo do limite de 3,3V)
- **Filtro passa-baixa para rejeição de ruído PWM:** capacitor cerâmico C_f = 10nF em paralelo com R_low → frequência de corte: f_c ≈ 5,3kHz (atenua os 20kHz do PWM, preserva a FCEM fundamental)
- **Atraso de fase do filtro na velocidade máxima:** f_el_max = (48.840 RPM × 4 polos) / 120 ≈ 1.628 Hz; φ_max = arctan(1628/5300) ≈ 17,07° — inferior aos 30° padrão, compensável via software
- **Comparador:** LM339N (4 comparadores independentes em DIP-14)
  - Saída em coletor aberto (open-collector) — permite interface com 3,3V do ESP32 via pull-up de 10kΩ
  - Entrada (+): tensão de fase atenuada e filtrada
  - Entrada (−): tensão do neutro virtual atenuada
  - Saída: conectada a pinos GPIO do ESP32 configurados para interrupção externa
- **Escalabilidade:** limite inferior 3S (11,1V mínimo para IR2110 e V_GS dos MOSFETs); limite superior 6S (25,2V para manter margem de segurança contra V_DSS=60V do IRFS7530 frente a picos indutivos de comutação)

---

### Metodologia

- **Simulação computacional (LTSpice ou Proteus):** validação prévia da lógica de controle, formas de onda de tensão de linha e fase, verificação de que a sequência de seis passos não gera sobreposição de condução (shoot-through).
- **Projeto de PCB:** placa FR4 com trilhas reforçadas e planos de terra adequados para suportar as altas correntes do barramento DC.
- **Procedimento de testes:** início com cargas leves e alimentação limitada (teste de fumaça / smoke test) → progressão para acionamento do motor em rampa de velocidade.

---

### Ganchos para Resultados

1. **Validação da simulação LTSpice:** verificar se as formas de onda de corrente de fase e tensão de neutro virtual correspondem ao previsto pelo modelo trapezoidal. **[RESPONDIDO PARCIALMENTE NO CAP. 4]**
2. **Eficácia do banco de capacitores do Link DC:** a simulação sem capacitores revelou picos de corrente na bateria. A PCB final deve demonstrar redução desses transientes.
3. **Limiar de transição malha aberta → malha fechada:** validar empiricamente que 5%–10% da velocidade nominal é suficiente para o ZCD com os comparadores LM339 e a rede de filtragem projetada.
4. **Compensação do atraso de fase do filtro:** validar que a correção T_wait = T_30° − T_lag mantém a comutação no ponto ótimo de torque em toda a faixa de velocidade operacional.
5. **Temperatura de operação dos MOSFETs:** validar se a dissipação de ~11,6W/transistor é gerenciada pela área de cobre da PCB ou se será necessário dissipador externo.
6. **Precisão do sensoriamento de corrente com ganho de 73:** verificar a linearidade da medição e se o offset de 0V é adequado para proteção de sobrecorrente.

---

## Resumo: Capítulo 4 — Resultados e Discussão

### Premissas e Objetivos

Este capítulo valida experimentalmente e computacionalmente as decisões de projeto dos capítulos anteriores. Está estruturado em duas frentes principais: (1) simulação em LTSpice para validação do hardware de potência em condição de rotor bloqueado; e (2) análise dos esquemáticos finais do ESC, documentando as decisões de projeto definitivas e suas justificativas físicas — incluindo divergências em relação aos valores preliminares da metodologia.

---

### Hardware e Firmware

#### Parâmetros Finais da Simulação LTSpice

- **Topologias simuladas:** meia-ponte monofásica (análise isolada) + ponte trifásica completa (validação inter-fases)
- **Modelo da bateria:** fonte DC 22,2V com resistência série de 19mΩ (emula dinâmica de descarga e perdas ôhmicas internas da LiPo 6S)
- **MOSFETs simulados:** modelo comportamental BSC028N06LS3
- **Resistores de gate na simulação:** 10Ω (limitam corrente do driver e amortecem ringing parasita)
- **Sinal de acionamento High-Side Fase A:** PULSE(0 15 0 100n 100n 25u 50u) — amplitude 15V, f_sw = 20kHz (período 50µs), duty cycle 50%, tempos de transição 100ns
- **Modelo do motor (rotor bloqueado):** R = 10mΩ e L = 20µH por fase em estrela → condição de máximo estresse elétrico sem oposição da FCEM
- **Motivo da modelagem estática (Passo 1 fixo):** validação da transferência de energia em um único instante da comutação trapezoidal; transistor Low-Side da Fase A mantido em corte; Dead Time dinâmico dispensado por isolamento ideal assumido

#### Resultados Quantitativos da Simulação

**Correntes de fase (primeiros 100µs — Passo 1):**
- I_A: ascende linearmente até ~13,5A durante T_on (0–25µs); decaimento suave durante T_off (25–50µs) via diodo de roda livre intrínseco do MOSFET inferior da Fase A; no segundo ciclo atinge ~26A (cumulativo)
- I_B: mesma magnitude de I_A com polaridade invertida (−13,5A)
- I_C: permanente em ~0A (ruído numérico apenas) — confirma eficácia do estado Hi-Z no braço não excitado

**Tensões de fase referenciadas ao neutro (Passo 1):**
- V_AN: oscila em torno de +11,1V durante T_on
- V_BN: oscila em torno de −11,1V durante T_on
- V_CN: oscila em torno de 0V (fase flutuante)
- Ringing de alta frequência sobreposto: artefato numérico do simulador (tanque LC não amortecido com capacitâncias parasitas dos transistores); no físico, seria atenuado pelas perdas no núcleo ferromagnético e pelo filtro RC do circuito de leitura
- **Validação física:** V_neutro = 22,2V / 2 = 11,1V — resultado direto da divisão de potencial no fechamento estrela com Fase A em V_DC e Fase B em 0V

**Bateria sem filtro capacitivo:**
- Tensão de barramento: decai gradualmente de 22,2V para ~21,9V durante T_on (V_drop = I_a × R_ser = 13A × 19mΩ)
- Picos de corrente nos instantes de comutação (50µs e 100µs): >130A e >200A respectivamente
- Quedas instantâneas de tensão (voltage sags) até ~19,5V
- **Conclusão validada:** submeter a bateria LiPo a este nível de ripple e picos transientes causaria superaquecimento severo e degradação química prematura → confirma necessidade imperativa do banco de capacitores de 940µF Low-ESR

#### Esquemático Final — Divergências e Decisões de Projeto Definitivas

**Estágio de Alimentação (Supply):**
- **Fusível:** MIDI 80A (modelo 0498080.M) — *divergência: metodologia previa 100A*
- **Banco capacitivo do Link DC (topologia final):** 6× capacitores eletrolíticos de 220µF (reduz ESR total via paralelo) + 3× cerâmicos de 1µF (desacoplamento de alta frequência, suprime transientes que os eletrolíticos não filtram pela ESL)
- **BEC (regulação auxiliar):** dois módulos LM2596 em **paralelo** (um para 15V "Vcc 1", outro para 5V "Vcc 2") — *divergência: metodologia previa cascata*; a topologia paralela elimina gargalo térmico de drenar corrente em cascata e garante isolamento entre circuito de gate (15V) e lógica digital (5V)
- **Separação de terra:** PGND (terra de potência) e SGND (terra de sinal) interligados por componente limitador de laço único (RJ1) — técnica Star Ground

**Estágio de Controle e Interface de Acionamento:**
- **ESP32:** alimentado por Vcc 2 (5V); regulador interno reduz para 3,3V exportado como "Vmicro Ref" para pinos VDD dos três IR2110 → garante compatibilidade de nível lógico entre GPIO do ESP32 e threshold dos drivers
- **IR2110 — segregação de terra:** VSS (referencial lógico) → SGND; COM (retorno de potência) → PGND
- **Controle de shutdown:** ESP32 controla pinos SD de cada IR2110 individualmente; pino OC Trip permite que evento de falta interrompa o MCPWM no hardware em microssegundos
- **Bootstrap (topologia final):** diodo de recuperação ultra-rápida UF4004 + 4,7µF (reserva de carga) em paralelo com 0,1µF (resposta em alta frequência) — *divergência: metodologia previa 10µF + 100nF*
- **Resistência de gate assimétrica (topologia definitiva):** 10Ω no turn-on (amortece ringing e EMI); diodo Schottky 1N5819 em paralelo com o resistor para turn-off (caminho de baixíssima impedância para escoamento de Q_g → transistor desliga mais rápido que liga, previne shoot-through)

**Estágio de Sensoriamento e Proteção:**
- **Monitoramento de tensão da bateria:** divisor 39kΩ / 4,7kΩ → fator 0,1075 → mapeia 25,2V (6S) para 2,71V e 9,0V (3S descarregado) para 0,97V no ADC; filtro RC (R_th ≈ 4,19kΩ, C = 0,1µF) com f_c ≈ 379Hz atenua os 20kHz do inversor em quase duas décadas
- **Amplificador de corrente de fase:** INA240A1DR (substitui o OpAmp genérico da metodologia) — ganho fixo de 20V/V, tecnologia Enhanced PWM Rejection para suprimir dv/dt de comutação; polarização diferencial REF1=3,3V e REF2=0V estabelece offset interno de 1,65V → saída bipolar em 0V–3,3V com zero de corrente em 1,65V (pré-requisito para futura implementação de FOC)
- **Resistor shunt (topologia final):** 3× shunts de liga metálica de 1mΩ em encapsulamento 3920 (modelo CSS2H-3920R-1L00F) no Low-Side de cada fase — *divergência: metodologia previa 0,5mΩ único*
- **OCP (Proteção de Sobrecorrente por Hardware):** LM339N com saída Wired-OR — Vdac Ref na entrada (+), Isense na entrada (−); em operação nominal (Isense < Vdac): saída em alta impedância, R10 pull-up mantém OC Trip em 3,3V; em falta (Isense > Vdac em qualquer fase): comparador satura, leva OC Trip a 0V em nanosegundos → interrupção prioritária no ESP32 para bloqueio imediato do MCPWM

**Estágio de Potência — PCB e Ponte Inversora:**
- **6× MOSFETs IRFS7530-7PPBF** em D2PAK-7 (múltiplos pinos de Source reduzem indutância parasita e ringing durante transientes de até 90A)
- **Resistores pull-down de 10kΩ** (R13–R18) em cada par Gate-Source: garante estado fail-safe (V_GS = 0V) durante power-up, reset do MCU ou ruptura de trilha, prevenindo shoot-through por acoplamento capacitivo de ruído eletromagnético
- **Sensoriamento de corrente no Low-Side:** V_shunt em mV referenciada ao PGND → sinal extremamente limpo, sem exposição do INA240 às oscilações de tensão de modo comum (0V–25,2V) dos nós flutuantes das fases

**Layout e Fabricação da PCB:**
- **Substrato:** FR4, 150×200mm, espessura 1,5mm, cobre nu sem máscara de solda (solder mask)
- **Método de fabricação:** fresadora CNC (substituiu corrosão química manual) — elimina undercutting, preserva espessura e massa total do cobre
- **Regras de DRC para CNC:** largura mínima de trilha lógica 0,6mm, clearance mínimo 0,6mm
- **Roteamento de potência:** copper pours sólidos em vez de trilhas convencionais; gargalos mínimos de 5mm de largura
- **Reforço das trilhas de potência:** deposição manual de camadas de estanho e condutores de cobre sólido sobre os polígonos expostos → aumenta área da seção transversal, reduz R_ôhmica
- **EMC — Star Ground:** PGND concentrado na Bottom Layer sob o inversor; SGND restrito à periferia dos circuitos lógicos/sensores; interligação exclusiva por ponto único
- **EMC — Distanciamento do ESP32:** posicionado no extremo oposto aos MOSFETs para atenuar acoplamento magnético (di/dt alto) inversamente proporcional ao quadrado da distância
- **Escopo inicial do protótipo:** demonstração de viabilidade do design com motor e bateria 4S, limitado a 3A (não operação em regime extremo de 90A)

---

### Metodologia

- Simulação LTSpice em dois cenários (monofásico e trifásico) com modelo de rotor bloqueado para análise de pior caso.
- Projeto de PCB no Altium Designer com DRC customizado para fabricação CNC.
- Análise comparativa entre os valores dimensionados na metodologia e os valores implementados no esquemático final (rastreamento de divergências e justificativas).

---

### Ganchos para Resultados (Pendentes de Validação Experimental)

1. **Corrente real do motor vs. simulação:** a simulação prevê ~13,5A no primeiro T_on sob rotor bloqueado com V_bat = 22,2V. O protótipo deve confirmar este valor (ou identificar divergências causadas por parâmetros físicos reais do motor).
2. **Eficácia do banco de capacitores (940µF):** a simulação quantificou os transientes sem filtro. O protótipo deve demonstrar a redução real desses picos após a instalação dos capacitores.
3. **Operação da proteção OCP (LM339 + INA240):** validar o tempo de resposta do Wired-OR em condição real de sobrecorrente e confirmar que o bloqueio do MCPWM ocorre dentro dos limites seguros.
4. **Temperatura de junção dos MOSFETs:** validar a necessidade (ou não) de dissipador térmico adicional além dos copper pours do PCB.
5. **Qualidade do sinal ZCD com o circuito de sensoriamento final:** confirmar que a reconstrução do neutro virtual, a filtragem RC e os comparadores LM339 fornecem sinais de ZCD limpos e sem disparos espúrios na faixa de 5% a 100% da velocidade nominal.
6. **Partida sensorless com o firmware embarcado no ESP32:** validar a máquina de estados (alinhamento 500ms → rampa → ZCD) com o hardware físico, incluindo o ajuste fino dos parâmetros de rampa para o motor Turnigy XK3674.
7. **Impacto da assimetria de turn-on/turn-off (Schottky 1N5819):** confirmar quantitativamente a redução do shoot-through e do ringing em relação a um gate com resistor simétrico.

---

*Arquivo atualizado em: 2026-06-21*  
*Fonte: leitura e análise do arquivo `Docs/Thesis/main.tex` e capítulos `1_introducao_teorica.tex`, `2_objetivos.tex`, `3_metodologia.tex` e `4_resultados_discussao.tex`.*

---

## Resumo: `1_introducao_teorica.tex` — Leitura Direta e Isolada

> **Nota:** Esta seção é o resultado da leitura dedicada e isolada do arquivo `capitulos/1_introducao_teorica.tex`, seguindo a taxonomia padrão. O conteúdo anterior (gerado a partir do `main.tex`) é preservado integralmente.

---

### Premissas e Objetivos

O problema de engenharia central deste capítulo é duplo:

1. **Paradoxo da nomenclatura BLDC:** o motor é alimentado por DC mas opera internamente como uma máquina AC síncrona de ímãs permanentes (PMSM). A resolução desse paradoxo é o ponto de partida: o ESC é o dispositivo que sintetiza as formas de onda AC necessárias a partir de uma fonte DC, tornando as equações lineares de motores DC aplicáveis ao BLDC.

2. **Construção do modelo de projeto:** o capítulo estabelece o arcabouço matemático completo (modelo elétrico + modelo mecânico + constantes K_T e K_V) que fundamenta toda decisão de dimensionamento de componentes, seleção de motor e projeto do algoritmo de controle dos capítulos seguintes.

**Estrutura de seções do capítulo:**
- §1 Motores CC Sem Escova (princípio, limitações, categorização como motor síncrono)
  - §1.1 Fundamentos Físicos da Conversão Eletromecânica (Lorentz + Faraday)
  - §1.2 Modelo Matemático e Definição das Constantes (equações elétrica e mecânica)
  - §1.3 A constante de Torque K_T
  - §1.4 As constantes de Velocidade K_V (K_e e K_V)
  - §1.5 A Equivalência entre K_T e K_e (prova por balanço de potência)
  - §1.6 Conversão de Unidades (tabela SI vs não-SI)
  - §1.7 A Constante do Motor K_m como Figura de Mérito
  - §1.8 Influência da Temperatura
- §2 Controlador Eletrônico de Velocidade (ESC)
  - §2.1 Consequências Práticas da Seleção de K_V
  - §2.2 Princípios de Operação (ponte inversora, comutação trapezoidal, ZCD)
    - §2.2.1 Comutação de Seis Passos
    - §2.2.2 Análise da Ondulação de Torque (Torque Ripple)
    - §2.2.3 Modulação por Largura de Pulso (PWM)
  - §2.3 Estratégia de Partida em Malha Aberta para Operação Sensorless
    - §2.3.1 Alinhamento do Rotor
    - §2.3.2 Comutação Forçada (Rampa de Aceleração)
  - §2.4 Desafios e Limitações da Partida Forçada
    - §2.4.1 Análise do Fenômeno de Perda de Sincronismo (Stall)
    - §2.4.2 Outros Efeitos Adversos (picos de corrente, vibração, ineficiência)
- §3 Aspectos de Eletrônica de Potência
  - §3.1 O MOSFET de Potência e Perdas (condução e comutação)
  - §3.2 O Barramento DC e a Indutância Parasita (Link DC, dimensionamento de C_min)
  - §3.3 Técnica de Bootstrap para Acionamento High-Side (princípio + dimensionamento de C_boot)
- §4 Estado da Arte em Algoritmos de Controle e Estimação
  - §4.1 Métodos Baseados em FCEM (ZCD, Integração da FCEM)
  - §4.2 Controle Orientado de Campo (FOC) e Observadores (Luenberger, EKF, Adaptativos)
  - §4.3 Técnicas Baseadas em Análise de Sinais e Harmônicos (Current Ripple, Injeção de Alta Frequência)
- §5 Parâmetros Críticos para Modelagem Dinâmica (R_s, L_s/L_d/L_q, λ_m, J, B, P)
- §6 Vantagens e Desvantagens no Uso de Motores BLDC (comparativo BLDC vs MIT)
- §7 Aplicações (veículos elétricos, robótica, automação industrial, VANTs)

---

### Hardware e Firmware

#### Modelo Físico e Matemático do Motor BLDC

- **Classificação:** motor síncrono de ímãs permanentes (PMSM) alimentado por fonte DC externa via ESC. A designação "DC" refere-se apenas à fonte de alimentação, não ao princípio de operação interno.
- **Configuração mais utilizada:** trifásica — melhor equilíbrio entre oscilação de torque e custo de construção. Motores monofásicos e bifásicos existem mas são marginais.
- **Motores BLDC são síncronos:** o rotor gira exatamente na mesma frequência do campo magnético giratório do estator. Não há escorregamento (fenômeno exclusivo dos motores de indução).
- **Princípio gerador de torque — Força de Lorentz:** torque eletromagnético é proporcional ao seno do ângulo elétrico entre os vetores de campo do estator e do rotor. Torque máximo → ângulo de 90° elétricos. Esta é a condição que todo algoritmo de controle de alto desempenho (FOC, ZCD) busca manter continuamente.
- **Princípio de limitação de velocidade — Lei de Faraday / FCEM:** a FCEM (back-EMF) é induzida pelo movimento dos imãs do rotor sobre os enrolamentos do estator. Pela Lei de Lenz, a FCEM se opõe à tensão de alimentação. O motor acelera até que FCEM ≈ V_alimentação, ponto em que a corrente resultante é apenas suficiente para vencer o atrito.
- **Equação elétrica (KVL):** a tensão de alimentação é dividida entre a FCEM, a queda resistiva na armadura (I_a × R_a) e a queda indutiva (L_a × dI_a/dt). "Armadura" é o termo técnico para o enrolamento onde ocorre a principal conversão de energia.
- **Equação mecânica (2ª Lei de Newton rotacional):** o torque eletromagnético é consumido pelo torque de carga, pelo torque inercial (J × dω/dt) e pelo torque de atrito viscoso (B × ω).

#### Constantes do Motor

- **K_T (Constante de Torque) [Nm/A]:** relação linear entre torque eletromagnético e corrente de armadura. Determinada por: força do campo magnético dos imãs (Neodímio → K_T maior), número de espiras do enrolamento (proporcional a N) e geometria do estator (comprimento e raio). Motor com K_T alto → menor corrente para mesmo torque → menores perdas Joule → maior eficiência.
- **K_e (Constante de FCEM) [V·s/rad]:** relação linear entre FCEM gerada e velocidade angular. Também denotada como k_b na literatura.
- **K_V (Constante de Velocidade) [RPM/V]:** parâmetro comercial predominante nos datasheets; definido como velocidade a vazio / tensão aplicada. Relação com K_e: K_V = 1/K_e (inversão simples).
- **Equivalência K_T = K_e em SI:** demonstrada por balanço de potência — igualdade entre a potência elétrica convertida (e_a × I_a) e a potência mecânica (T_e × ω), com substituição das definições de K_e e K_T. Prova que torque a partir de corrente e tensão a partir de movimento são duas manifestações do mesmo acoplamento eletromagnético.
- **Conversão prática:** K_T [Nm/A] = 2π / (60 × K_V [RPM/V]). Tabela de fatores de conversão:
  - K_T: 1 oz-in/A = 0,00706155 Nm/A; 1 mNm/A = 0,001 Nm/A
  - K_e: 1 V/krpm = 0,009549 V·s/rad
  - K_V: 1 RPM/V = 0,10472 rad/(V·s)
- **K_m (Constante do Motor) [Nm/√W]:** K_T / √(R_a). Representa torque por watt de calor dissipado. Invariante a mudanças de enrolamento para mesmo tamanho de carcaça → figura de mérito para comparar qualidade de projetos de motor.
- **Influência da temperatura:** aquecimento → desmagnetização parcial dos imãs → redução de K_T e K_e → menos torque por ampere, necessidade de maior velocidade para mesma FCEM → degradação geral de performance. As "constantes" são, na prática, dependentes da temperatura de operação.

#### Trade-off de Seleção de K_V

- **Alto K_V** (ex: 2200KV do Turnigy XK3674): menos espiras, fio grosso → baixo K_T, baixa R, baixa L → requer alta corrente para torque → atinge altas velocidades. Aplicações: drones de corrida, fusos de usinagem, aeromodelismo.
- **Baixo K_V**: mais espiras, fio fino → alto K_T, alta R, alta L → alto torque com baixa corrente → menor velocidade máxima. Aplicações: drones de carga, guinchos, atuadores robóticos de acionamento direto.

#### Topologia do ESC e Algoritmos

- **Circuito base:** ponte inversora trifásica (VSI — Voltage Source Inverter). Motor conectado em Estrela (Y) ou Triângulo (Δ).
- **Blocos funcionais do ESC:**
  - Circuito de entrada: converte tensão da bateria via PWM, gera tensão média variável, pode inverter sequência de fases para reversão.
  - Circuito de controle: calcula corrente a fornecer às bobinas; usa sinal PWM como referência de velocidade e sentido.
  - Circuito de saída: fornece corrente necessária às bobinas do estator.
  - Recursos opcionais: filtro de ruído EMI, proteção térmica, proteção contra sobrecarga.
- **Formas de onda de corrente:** senoidal (alto rendimento, requer FOC) ou trapezoidal (baixo custo, requer controle de seis passos). As formas de onda das três fases devem ser defasadas entre si para garantir rotação contínua.
- **Comutação de Seis Passos (Controle Trapezoidal):**
  - 2 transistores conduzem por passo; cada transistor conduz 120° elétricos; comutação a cada 60° elétricos.
  - Em malha aberta: sequência fixa — ineficiente e vulnerável à perda de sincronismo sob variações de carga.
  - **Estratégia adotada neste TCC:** Controle Trapezoidal em Malha Fechada Sensorless — sequência de comutação sincronizada dinamicamente com a posição do rotor via ZCD da FCEM na fase não energizada.
- **PWM para controle de velocidade:** a própria ponte trifásica ajusta o duty cycle, gerando tensão média proporcional à velocidade desejada. Em velocidade fixa, fonte DC pura é suficiente.

#### Ondulação de Torque (Torque Ripple) — Análise Detalhada

- **Definição:** pulsações no torque de saída com frequência = 6 × f_elétrica_fundamental (uma pulsação por evento de comutação).
- **Efeitos:** oscilações de velocidade, ruído acústico, vibrações mecânicas — especialmente prejudiciais em aplicações de alta precisão.
- **Causa 1 — Incompatibilidade de Formas de Onda:** o modelo trapezoidal ideal assume corrente retangular perfeita e FCEM trapezoidal perfeita. Na prática, ambas se desviam do ideal. O torque instantâneo total é T_e = (e_a·i_a + e_b·i_b + e_c·i_c) / ω; qualquer desvio de forma de onda entre corrente e FCEM gera variação de torque.
- **Causa 2 — Transientes de Comutação (causa predominante):** a indutância dos enrolamentos impede variação instantânea de corrente. Na transição entre passos (ex: A-B → A-C), a corrente na fase desligada (B) decai gradualmente enquanto a corrente na fase ligada (C) sobe gradualmente. A taxa de variação de entrada e saída não é simétrica → mergulho ou pico de torque a cada 60° elétricos.
- **Interpretação física unificada:** durante os intervalos de comutação, o vetor de corrente do estator está em transição entre posições estáveis. O ângulo entre campo do estator e campo do rotor desvia da orientação ótima de 90° → torque de saída menor para mesma corrente de entrada → queda transitória na eficiência de conversão. O torque ripple é a manifestação mecânica desses intervalos de conversão sub-ótima, intrínsecos à natureza discreta do chaveamento de seis passos.

#### Estratégia de Partida Sensorless

- **Problema fundamental:** FCEM = K_e × ω → em ω = 0 (repouso), FCEM = 0. Impossível determinar posição do rotor pelos comparadores. O controlador opera "às cegas".
- **Fase 1 — Alinhamento:**
  - Energização de par específico de enrolamentos com corrente DC por tempo pré-determinado (ex: tensão positiva em C, fases A e B ao terra).
  - Os imãs do rotor se alinham com o campo magnético estático, minimizando a relutância magnética — analogia à agulha de bússola.
  - Posição θ₀ conhecida ao final. O objetivo é que o PRIMEIRO passo dinâmico subsequente seja disparado com o campo do estator a 90° do campo do rotor → máximo torque de arranque.
  - Parâmetros críticos: duração do alinhamento e magnitude da corrente. Corrente baixa ou tempo insuficiente → alinhamento incompleto → torque inicial fraco → falha na partida.
- **Fase 2 — Comutação Forçada (Rampa de Aceleração):**
  - Comutação em malha aberta com frequência crescente (perfil de rampa pré-programado), sem realimentação.
  - O controlador "assume" que o rotor consegue acompanhar o campo giratório do estator.
  - Continua até ~5% da velocidade nominal, ponto em que a FCEM atinge amplitude detectável → handoff para malha fechada ZCD.
- **Riscos da Partida Forçada:**
  - **Stall (perda de sincronismo):** ocorre quando torque de carga + inércia > torque eletromagnético disponível em determinado passo. Rampa excessivamente agressiva é a causa mais comum. Resultado: movimento errático, vibrações intensas, ruído audível, colapso do torque, picos de corrente perigosos para o inversor.
  - **Picos de corrente:** com FCEM nula ou baixa, a corrente é limitada apenas por R_a e V_bat → risco de sobrecorrente se o duty cycle do PWM não for controlado.
  - **Vibração e ineficiência:** desalinhamento permanente durante a rampa → torque ripple elevado → parte da energia elétrica consumida é dissipada em vez de convertida em trabalho mecânico.
- **Natureza transitória:** a partida forçada é uma estratégia de bootstrap exclusivamente para levar o motor de ω=0 até a velocidade mínima operacional de malha fechada. Não é adequada para operação contínua.

#### Eletrônica de Potência — Aspectos Teóricos

- **MOSFET de Potência (Canal-N):** opera em chaveamento (corte ↔ saturação), não em região linear. Dois mecanismos de perda:
  - Perdas por condução: P_cond = I_D² × R_DS(on). R_DS(on) minimizado em componentes modernos para reduzir aquecimento.
  - Perdas por comutação: P_sw proporcional à frequência de chaveamento (f_sw) e à carga total de gate (Q_g). Transição não instantânea entre estados → período com tensão e corrente simultaneamente não nulas.
- **Barramento DC — Indutância Parasita:** cabos de alimentação introduzem L_par. Na comutação, di/dt elevado → V_spike = L_par × di/dt → risco de destruição dos semicondutores se V_spike > V_DSS. Solução: banco de capacitores de baixa ESR posicionado próximo aos drenos dos MOSFETs High-Side.
- **Dimensionamento do C_min do Link DC:**
  - Fórmula analítica clássica: C_min = (I_peak × D × (1-D)) / (f_sw × ΔV). Esforço máximo no capacitor ocorre com D = 0,5.
  - **Limitação prática em alta corrente/baixa tensão (ex: VANTs):** o fator limitante não é C_min, mas a capacidade de condução de corrente de ripple (I_Ripple,RMS) e a ESR. Capacitores eletrolíticos têm correlação inversa entre capacitância e ESR.
  - **Regra prática de engenharia:** sobredimensionamento para 10µF a 20µF por ampere de corrente de fase. Objetivo primário: reduzir ESR total via associação paralela para que I_RMS não exceda o Ripple Current Rating → previne degradação dielétrica por superaquecimento.
- **Técnica de Bootstrap para acionamento High-Side:**
  - Problema: MOSFET Canal-N no High-Side exige V_gate > V_source, que sobe para ~V_CC quando o transistor liga → necessidade de tensão flutuante superior a V_CC.
  - Solução bootstrap: capacitor C_boot + diodo D_boot. C_boot carregado durante condução do Low-Side. Durante acionamento do High-Side, C_boot age como "bateria flutuante".
  - **Limitação crítica:** duty cycle de 100% é inviável indefinidamente — o Low-Side deve comutar periodicamente para recarregar C_boot. Sem recarga, V_GS cai → MOSFET superior entra na região linear → falha por superaquecimento.
  - **Dimensionamento de C_boot:** carga total por ciclo Q_tot = Q_g (carga do MOSFET) + (I_qbs + I_leak) × T_on (consumo quiescente do driver e fugas). Como Q_g >> consumo do driver em frequências típicas, Q_g é o fator dominante. Capacitância mínima: C_boot ≥ Q_tot / ΔV_boot. Regra prática: C_boot dimensionado para armazenar pelo menos 15 × Q_g, garantindo tensão rígida V_GS.

#### Estado da Arte — Comparativo de Algoritmos de Controle Sensorless

| Algoritmo | Princípio | Faixa de operação | Complexidade | Adotado neste TCC |
|-----------|-----------|-------------------|--------------|-------------------|
| ZCD (Zero-Crossing Detection) | Monitoramento da FCEM na fase não energizada | Médias e altas rotações | Baixa | **Sim** |
| Integração da FCEM | Integral da área sob a curva da FCEM | Médias e altas rotações | Média | Não |
| FOC (Field-Oriented Control) | Mantém campo do estator sempre a 90° do rotor via controle vetorial de corrente | Toda a faixa | Alta | Não (referência futura) |
| Observador de Estado (Luenberger) | Estimação de FCEM via equações diferenciais do motor; correção pelo erro entre corrente estimada e medida | Toda a faixa (com modelo preciso) | Alta | Não |
| EKF (Filtro de Kalman Estendido) | Estimação estocástica com predição + correção; robusto a ruído | Toda a faixa | Muito alta (custo computacional) | Não |
| Observadores Adaptativos | Ajuste dinâmico dos parâmetros do modelo (R, fluxo) durante operação para compensar variações térmicas | Toda a faixa | Muito alta | Não |
| Detecção de Current Ripple | Contagem de ondulações de corrente geradas pela interação rotor-estator (saliência por ranhuras) | Baixa velocidade | Alta | Não |
| Injeção de Alta Frequência | Sinal HF injetado; resposta de corrente varia com posição do rotor por saliência magnética (L_d ≠ L_q) | Velocidade zero + baixa velocidade | Muito alta | Não |

#### Parâmetros Críticos para Modelagem Dinâmica (Necessários para FOC/Observadores)

- **R_s (Resistência de Estator):** queda ôhmica nos enrolamentos. Erros em R_s → desvios na estimativa de fluxo em baixas velocidades.
- **L_s, L_d, L_q (Indutâncias):** L_s para motores SPM (Surface Permanent Magnet); L_d e L_q para IPM (Interior Permanent Magnet). A diferença L_d ≠ L_q (saliência) é o que permite torque de relutância e detecção por injeção de sinal.
- **λ_m (Fluxo dos imãs permanentes):** crucial para cálculo de T_e e K_e. Varia com temperatura (desmagnetização temporária) → afeta K_T.
- **J (Momento de Inércia) e B (Atrito Viscoso):** ditam a resposta de velocidade. J é vital para o projeto dos ganhos K_p e K_i do controlador PID de velocidade.
- **P (Número de Polos):** define a relação entre frequência elétrica e velocidade mecânica. Erro neste parâmetro inviabiliza qualquer algoritmo de estimação.
- **Observação:** levantamento desses parâmetros exige equipamentos de precisão ou rotinas de auto-comissionamento no inversor — complexidade superior em relação ao método trapezoidal via ZCD adotado neste projeto.

#### Comparativo BLDC vs Motor de Indução Trifásico (MIT)

| Critério | BLDC | MIT |
|----------|------|-----|
| Princípio | Campo girante por comutação eletrônica via ESC | Indução eletromagnética |
| Custo inicial | Mais alto | Mais baixo |
| Rendimento (carga variável / velocidade ajustável) | Superior | Inferior |
| Rendimento (baixo torque) | Inferior | Superior (relevante em VEs) |
| Densidade de potência/torque (baixa velocidade) | Superior | Inferior |
| Controle | Complexo (requer ESC dedicado) | Mais simples |
| Vida útil / manutenção | Maior (sem escovas) | Alta confiabilidade e simplicidade construtiva |
| Escorregamento | Nenhum (motor síncrono) | Presente (característica intrínseca) |
| Ruído | Menor | Maior |

#### Aplicações dos Motores BLDC Mencionadas no Capítulo

- Veículos elétricos: carros, motocicletas, bicicletas, scooters (propulsão — alto rendimento e baixa manutenção)
- Robótica e automação industrial (controle preciso de posição e velocidade)
- VANTs / drones (alta densidade de potência em volume reduzido)
- Eletrônicos de consumo: ventiladores, ferramentas elétricas
- Fusos de usinagem (alto K_V, alta velocidade)

---

### Metodologia

- O capítulo é **exclusivamente teórico-analítico**. Nenhuma simulação, experimento ou configuração física é descrita aqui.
- Método principal: **derivação analítica** das equações do motor a partir dos princípios de Lorentz e Faraday → síntese no modelo elétrico + mecânico → extração das constantes K_T e K_e como fatores de proporcionalidade → demonstração da equivalência K_T = K_e por conservação de energia.
- Segundo método: **análise qualitativa comparativa** — trade-offs de K_V, comparativo BLDC vs MIT, comparativo de algoritmos de controle.
- Terceiro método: **análise de falha** — decomposição das causas do torque ripple (incompatibilidade de formas de onda + transientes de comutação) e das causas de falha na partida sensorless (stall, picos de corrente).

---

### Ganchos para Resultados

1. **Torque Ripple quantificado:** a teoria prevê pulsações de torque na frequência de 6 × f_elétrica e aponta duas causas (formas de onda não-ideais + transientes de comutação). Os resultados **devem** medir ou estimar a amplitude dessas pulsações no Turnigy XK3674 operando com o ESC produzido.

2. **Validade do modelo linearizado K_T e K_e:** o capítulo prova que as equações lineares DC são aplicáveis ao BLDC *desde que* o ESC execute a comutação corretamente. Os resultados **devem** confirmar (ou delimitar as condições de validade de) essa linearidade — especialmente em regime transitório e na partida.

3. **Falha de partida vs. parâmetros de alinhamento:** a teoria estabelece que corrente de alinhamento insuficiente ou tempo curto → falha na partida. Os resultados **devem** apresentar a calibração empírica dos parâmetros de alinhamento (tempo e duty cycle) que garantem partida confiável no motor específico do projeto.

4. **Handoff malha aberta → malha fechada em ~5% da velocidade nominal:** a teoria define o limiar teórico de ~5% da velocidade nominal como ponto de transição. Os resultados **devem** confirmar se este limiar é atingível e confiável com os comparadores LM339 e a rede de filtragem dimensionada.

5. **Impacto térmico sobre K_T e K_e:** a teoria afirma que o aquecimento dos imãs degrada as constantes do motor. Os resultados em regime contínuo **devem** verificar se há variação observável de desempenho (queda de velocidade ou torque) com o aquecimento.

6. **Capacidade do ZCD como estimador de posição:** o capítulo posiciona o ZCD como uma das técnicas mais simples e eficazes para médias e altas rotações. Os resultados **devem** caracterizar a faixa de velocidade em que o ZCD opera com confiabilidade no hardware construído (piso de velocidade abaixo do qual o sinal de FCEM é insuficiente).

7. **Justificativa do não-uso de FOC/EKF:** o capítulo apresenta o estado da arte e aponta que FOC e observadores requerem parâmetros precisos do motor (R_s, L_d/L_q, λ_m, J, B) não disponíveis no datasheet do Turnigy. Os resultados **devem** — mesmo que implicitamente — validar que o controle trapezoidal via ZCD é suficiente para o escopo proposto (operação suave e precisa), justificando a não-adoção das técnicas avançadas.

---

*Seção adicionada em: 2026-06-21 | Fonte: `capitulos/1_introducao_teorica.tex` (leitura direta e isolada)*

---

## Resumo: `2_objetivos.tex` — Leitura Direta e Isolada

> **Nota:** Esta seção é o resultado da leitura dedicada e isolada do arquivo `capitulos/2_objetivos.tex`, seguindo a taxonomia padrão. Todo conteúdo anterior é preservado integralmente.

---

### Premissas e Objetivos

O capítulo de Objetivos é composto por uma única sentença declarativa, sem subdivisões. Seu conteúdo integral, semanticamente extraído:

> **Estudar o princípio de funcionamento de um controlador de motores Brushless, tornando possível o projeto e desenvolvimento de um protótipo que possa oferecer uma operação suave e precisa em diferentes cenários de aplicação.**

**Decomposição analítica do objetivo em três entregas de engenharia:**

1. **Entrega de Conhecimento:** domínio teórico do princípio de funcionamento de um ESC para motor BLDC — inclui comutação eletrônica, estratégias de controle (trapezoidal sensorless), modelagem do motor (K_T, K_e, FCEM) e eletrônica de potência associada.

2. **Entrega de Hardware/Firmware:** projeto e construção de um protótipo funcional de ESC — inclui inversor trifásico, drivers de gate, microcontrolador, sensoriamento de FCEM e firmware de controle embarcado.

3. **Entrega de Validação:** demonstração de "operação suave e precisa em diferentes cenários de aplicação" — implica testes experimentais com variação de carga, variação de velocidade setpoint e avaliação qualitativa/quantitativa do desempenho.

**Problema de engenharia subjacente (implícito no objetivo):** controlar a velocidade e o torque de um motor BLDC sem o uso de sensores de posição dedicados (sensores Hall), a partir de uma fonte de energia DC (bateria), com precisão e suavidade suficientes para aplicações práticas reais.

---

### Hardware e Firmware

O capítulo não introduz nenhum componente, topologia ou lógica de firmware específica. Toda a especificação técnica é delegada aos capítulos de Introdução Teórica e Metodologia.

**Inferências de escopo técnico implícitas no objetivo:**

- **"Protótipo":** implica construção física em PCB, não apenas simulação.
- **"Operação suave":** implica ausência de oscilações de velocidade perceptíveis, torque ripple minimizado e partida sem stall — requisitos que impõem restrições ao projeto do firmware (tempo morto, rampa de aceleração, limiar de ZCD).
- **"Operação precisa":** implica correspondência entre duty cycle de PWM comandado e velocidade real entregue — requisito que impõe critérios de validação para o capítulo de Resultados.
- **"Diferentes cenários de aplicação":** implica que o protótipo deve ser testado com variação de carga e variação de velocidade, não apenas em ponto fixo de operação.

---

### Metodologia

O capítulo não descreve nenhuma metodologia. É estritamente declarativo.

**Observação de engenharia:** a ausência de subdivisões no capítulo de Objetivos indica que o TCC adota um objetivo único e não hierarquizado. Isso estabelece que todos os capítulos subsequentes (Metodologia, Resultados) devem ser avaliados em função de um único critério de sucesso: **o protótipo de ESC opera de forma suave e precisa?**

---

### Ganchos para Resultados

Os três termos-chave do objetivo funcionam como critérios de aceitação (acceptance criteria) do projeto, todos obrigatoriamente respondidos no capítulo de Resultados e Discussões:

1. **"Operação suave"** → O capítulo de Resultados **deve** apresentar evidência de que o motor parte sem stall, acelera sem oscilações bruscas e mantém velocidade estável em regime — seja por osciloscópio, por leitura de FCEM ou por análise de corrente de fase.

2. **"Operação precisa"** → O capítulo de Resultados **deve** quantificar o desvio entre a velocidade comandada (duty cycle do PWM) e a velocidade real do rotor — mesmo que indiretamente pela frequência de ZCD detectada.

3. **"Diferentes cenários de aplicação"** → O capítulo de Resultados **deve** descrever ao menos dois cenários de teste distintos (ex: carga leve vs. carga moderada, velocidade baixa vs. velocidade alta) para validar a robustez do protótipo além de um único ponto de operação.

---

*Seção adicionada em: 2026-06-21 | Fonte: `capitulos/2_objetivos.tex` (leitura direta e isolada)*

---

## Resumo: `3_metodologia.tex` — Leitura Direta e Isolada

> **Nota:** Esta seção é o resultado da leitura dedicada e isolada do arquivo `capitulos/3_metodologia.tex`, seguindo a taxonomia padrão. Todo conteúdo anterior é preservado integralmente.

---

### Premissas e Objetivos

Este capítulo resolve o problema central de engenharia de sistemas do TCC: **como especificar, dimensionar, integrar e validar todos os subsistemas físicos do ESC** — potência, controle, sensoriamento e firmware — de forma a operar com segurança, eficiência e robustez dentro das restrições impostas pelo motor e pela fonte de energia selecionados.

A abordagem é top-down, estruturada em cinco etapas sequenciais:

1. **Arquitetura do Sistema** — definição do fluxo de energia e dos blocos funcionais
2. **Dimensionamento do Hardware de Potência** — seleção e validação de componentes
3. **Sistema de Controle e Firmware** — microcontrolador, MCPWM, máquina de estados
4. **Sistema de Realimentação (ZCD)** — circuito de detecção de FCEM e estimação de posição
5. **Simulação Computacional + Montagem e Prototipagem** — validação pré-física e construção

**Estrutura de seções do capítulo:**
- §1 Arquitetura do Sistema
- §2 Dimensionamento e Hardware de Potência
  - §2.1 Especificações do Motor e Carga
  - §2.2 Fonte de Alimentação Principal (Bateria)
  - §2.3 Sistema de Proteção Contra Sobrecorrentes
  - §2.4 Filtragem do Barramento DC (Link DC)
  - §2.5 Dimensionamento do Inversor (MOSFETs)
  - §2.6 Driver de Gate e Circuito de Bootstrap
  - §2.7 Determinação da Frequência de Chaveamento
  - §2.8 Fonte de Alimentação Auxiliar (BEC)
  - §2.9 Sensoriamento de Corrente e Proteção (Shunt)
- §3 Sistema de Controle e Firmware
  - §3.1 O Microcontrolador ESP32
  - §3.2 Lógica de Comutação de Seis Passos
  - §3.3 Implementação Prática com o Periférico MCPWM
  - §3.4 Estratégia de Partida e Transição para Malha Fechada
- §4 Sistema de Realimentação e Detecção de Cruzamento por Zero
  - §4.1 Reconstrução do Neutro Virtual e Divisores de Tensão
  - §4.2 Filtragem de Ruído PWM
  - §4.3 Análise e Compensação do Atraso de Fase do Filtro
  - §4.4 Estágio Comparador com LM339
  - §4.5 Considerações de Sensibilidade e Escalabilidade
- §5 Simulação Computacional
- §6 Montagem e Prototipagem

---

### Hardware e Firmware

#### Arquitetura Geral do Sistema

- **Fluxo de energia:** Bateria (alta descarga) → Fusível MIDI (proteção passiva) → Link DC (filtragem) → Inversor Trifásico (comutação) → Motor BLDC
- **Fluxo de controle:** ESP32 (gera PWM via MCPWM) → Drivers IR2110 (tradução de nível lógico) → Gates dos MOSFETs → Fases do motor
- **Fluxo de realimentação:** Tensão das fases do motor → Divisor resistivo (atenuação) → Filtro RC (rejeição de PWM) → Comparadores LM339 (digitalização ZCD) → GPIO com interrupção externa do ESP32
- **Fluxo de proteção:** Shunt resistivo (sensoriamento de corrente) → Amplificador de sinal → Comparador de sobrecorrente → Desativação do PWM no ESP32

#### Motor (Carga — Especificações Completas)

- **Modelo:** Turnigy XK3674-2200KV
- **Tipo:** Inrunner (rotor interno)
- **Número de polos:** 4
- **Constante de velocidade:** 2200 RPM/V (K_V)
- **Tensão nominal:** 25,2 V (bateria 6S carregada) — **nota:** o datasheet indica tensão nominal, mas a bateria entrega 22,2V nominal
- **Corrente nominal:** 70 A
- **Potência máxima:** 1750 W
- **Velocidade a vazio (25,2V):** 55.440 RPM (teórico, sem atrito)
- **Velocidade a vazio (22,2V nominal):** 48.840 RPM (~12% de redução — aceita para este projeto)
- **Torque a vazio:** 0,3 Nm (sem efeitos dissipativos)
- **Parâmetros elétricos estimados da literatura (ausentes no datasheet):** L ≈ 20µH, R ≈ 10mΩ por fase (valores típicos para classe 3674, 4 polos)
- **Constante de tempo elétrica estimada:** τ = L/R = 20µH / 10mΩ ≈ 2 ms

#### Bateria (Fonte de Energia)

- **Modelo:** Turnigy Graphene Panther 1300mAh 6S 75C
- **Tecnologia:** LiPo (Lítio-Polímero) — maior taxa de descarga comercialmente disponível (até >100C)
- **Configuração:** 6S (6 células em série)
- **Tensão nominal por célula:** 3,7 V; tensão de carga máxima: 4,2 V
- **Cálculo de tensão do pack:** V_pack = N_S × V_célula → 6 × 3,7V = 22,2V nominal; 6 × 4,2V = 25,2V carregada
- **Capacidade:** 1300 mAh = 1,3 Ah
- **Taxa de descarga:** 75C
- **Corrente máxima de descarga:** I = C-rate × Capacidade [Ah] = 75 × 1,3 = 97,5 A
- **Critério de dimensionamento:** corrente nominal do motor (70A) + margem de segurança de 30% para picos (rotor travado, inversão de sentido) = 91A. A bateria supera este requisito (97,5A > 91A).
- **Impacto da tensão nominal (22,2V vs 25,2V):** redução de velocidade a vazio de ~12%, sem impacto funcional para o projeto.
- **Parâmetro crítico de projeto:** a tensão 6S (22,2V–25,2V) define o K_V operacional do motor e a tensão máxima de dimensionamento dos MOSFETs.

#### Sistema de Proteção Passiva — Fusível

- **Tipo:** Fusível MIDI 100 A
- **Instalação:** externo à PCB (off-board), em série com o cabo positivo da bateria
- **Critério de dimensionamento:** margem de ~10% acima da corrente de pico estimada (90A) → atua apenas em falha real (curto-circuito ou travamento mecânico), sem interrupções espúrias em operação nominal
- **Justificativa do tipo MIDI vs ATO/ATC:** terminais com fixação por parafuso (M5/M6) → alta pressão de contato → baixa R_contato → menor aquecimento sob alta corrente
- **Justificativa da instalação off-board:** preserva integridade das trilhas de cobre em caso de fusão violenta; facilita manutenção em campo; remove fonte de calor concentrada próxima aos transistores

#### Link DC — Filtragem do Barramento

- **Problema:** indutância parasita dos cabos de alimentação gera picos de tensão destrutivos (V_spike = L_par × di/dt) durante comutação
- **Critério de dimensionamento:** regra prática para ESCs de alta performance → ~220µF por 20A de corrente de fase
- **Cálculo de capacitância mínima:** C_bus ≈ (I_peak/20) × 220µF = (90/20) × 220µF ≈ 990µF
- **Topologia implementada:**
  - Conector XT90 anti-centelha (entrada da bateria)
  - 2× capacitores eletrolíticos Low-ESR de **470µF / 35V** em paralelo → total: 940µF
  - Posicionados o mais próximo possível dos drenos dos MOSFETs High-Side (minimiza o loop de indutância)
- **Prioridade do projeto:** minimização da ESR total (via associação paralela) em detrimento de capacitância exata — garante que I_RMS não exceda o Ripple Current Rating, prevenindo degradação dielétrica por superaquecimento

#### MOSFETs da Ponte Inversora Trifásica

- **Modelo:** IRFS7530TRL7PP
- **Encapsulamento:** D2PAK-7 (múltiplos terminais de Source → minimiza indutância parasita da pastilha)
- **V_DSS:** 60 V → margem >100% sobre V_barramento máximo de 25,2V (necessária por causa dos picos indutivos de comutação)
- **R_DS(on) máximo:** ~1,4 mΩ
- **Cálculo de perda por condução na corrente de pico (91A):** P_diss = I² × R_DS(on) = 91² × 0,0014 ≈ 11,6 W por transistor
- **Exigência térmica:** dissipação de 11,6W requer área de cobre estendida na PCB ou dissipador externo para manter T_junção dentro dos limites operacionais seguros
- **Quantidade:** 6 transistores (3 braços × 2 por braço — High-Side e Low-Side)

#### Driver de Gate — IR2110

- **Função:** interface entre lógica de 3,3V do ESP32 e gates dos MOSFETs que exigem 10V–15V para saturação plena; gera a tensão flutuante necessária para o High-Side via Bootstrap
- **Modelo:** IR2110 (driver de meia-ponte)
- **Tensão de alimentação:** 15V (gerado pelo BEC LM2596)
- **Corrente de pico de acionamento de gate:** até 2A
- **Cálculo da resistência de gate mínima:** R_total ≥ V_drv / I_max = 15V / 2A = 7,5Ω. Subtraindo R_G,int (2,1Ω do IRFS7530): R_g,ext_min = 5,4Ω
- **Valor comercial selecionado:** **10Ω** (margem de segurança + amortecimento de ringing)
- **Cálculo do tempo de comutação com 10Ω:** I_med = 15V / 12,1Ω ≈ 1,24A; t_sw = Q_g / I_med = 360nC / 1,24A ≈ 290 ns
- **Validação:** 290ns < 1,5% do período de 50µs (20kHz) → perdas por comutação baixas sem comprometer estabilidade eletromagnética
- **Tempos de propagação do IR2110:** t_on = 120ns, t_off = 94ns → representam < 0,5% do ciclo total a 20kHz → desprezíveis para a dinâmica do controle
- **Corrente média de gate calculada (pior caso):** I_g,avg = Q_g,max × f_sw = 354nC × 20kHz ≈ 7,08 mA → muito abaixo do limite de 2A do driver → operação em regime seguro confirmada

#### Circuito de Bootstrap

- **Topologia:** capacitor C_boot + diodo D_boot. C_boot carregado durante condução do Low-Side → atua como "bateria flutuante" durante acionamento do High-Side
- **Critério de dimensionamento:** Q_min = 15 × Q_g = 15 × 360nC = 5,4µC
- **Componentes selecionados:** capacitor eletrolítico de **10µF** (reserva de energia) em paralelo com cerâmico de **100nF** (resposta em alta frequência)
- **Carga armazenada:** Q_arm = C_boot × V_drv = 10µF × 15V = 150µC >> 5,4µC → ampla margem de robustez
- **Queda de tensão por ciclo:** ΔV_boot = Q_g / C_boot = 360nC / 10µF = 36mV (0,24% de 15V) → desprezível, V_GS permanece estável

#### Frequência de Chaveamento

- **Valor definido:** **20 kHz**
- **Justificativa 1 — Perdas térmicas:** P_sw ∝ f_sw. Frequências típicas de aeromodelismo (48kHz, 96kHz) causariam dissipação excessiva nos IRFS7530 (Q_g significativo). A 20kHz, as perdas são gerenciáveis por dissipação passiva.
- **Justificativa 2 — Conforto acústico:** magnetostrição nas lâminas e bobinas gera ruído audível excitado pelos harmônicos da corrente PWM. 20kHz está no limiar ultrassônico → elimina o zumbido agudo característico de inversores < 12kHz.
- **Justificativa 3 — Ripple de corrente / torque:** τ_elétrico = L/R ≈ 2ms >> T_sw = 50µs (a 20kHz). A condição T_sw << τ garante operação no modo de condução contínua com baixa ondulação de corrente → torque mais estável.
- **Validação do IR2110 a 20kHz:** atrasos de propagação < 0,5% do ciclo; corrente média de gate 7,08mA << 2A → estabilidade confirmada.

#### Fonte de Alimentação Auxiliar (BEC)

- **Decisão:** circuito BEC (Battery Elimination Circuit) para evitar o peso/volume de uma segunda bateria
- **Módulo:** **LM2596** (conversor buck step-down, 150kHz, 3A)
- **Topologia em cascata:** V_bat (22,2V–25,2V) → **15V** (alimenta IR2110) → **5V/3,3V** (alimenta ESP32)
- **Eficiência típica:** > 85%

#### Sensoriamento de Corrente — Shunt Resistivo

- **Topologia:** Low-Side Current Sensing (resistor shunt no ramo de retorno — lado baixo)
- **Justificativa vs. Sensor Hall (ACS7xx):** resposta instantânea para proteção de curto-circuito; sem atrasos de propagação; sem não-linearidades de sensores magnéticos integrados
- **Resistência do shunt:** **0,5 mΩ** (liga metálica, encapsulamento de potência 3920 ou 5930)
- **Cálculo de potência dissipada (91A):** P_shunt = 91² × 0,0005 ≈ 4,14 W
- **Impacto na eficiência global:** η_loss = 4,14W / (22,2V × 91A) = 4,14W / 2020W ≈ 0,2% → desprezível
- **Tensão de sensoriamento máxima (91A):** V_sense = 91A × 0,5mΩ = 45,5 mV
- **Problema:** 45,5 mV é insuficiente para a faixa do ADC do ESP32 (0–3,3V)
- **Solução:** amplificador não-inversor com ganho calculado: A_v = V_ADC_max / V_sense = 3,3V / 0,0455V ≈ 72,5 → fixado em **A_v = 73** com resistores de precisão de 1%

#### Microcontrolador ESP32

- **Modelo:** ESP32-DevKit C (módulo ESP32-WROOM-32)
- **Clock máximo:** 240 MHz
- **Núcleos:** dual-core (execução paralela de tarefas — crítico para ESC)
- **Resolução PWM:** 16 bits
- **Nível lógico:** 3,3V → exige interface via IR2110 para acionar os MOSFETs
- **Periférico principal:** **MCPWM** (Motor Control Pulse Width Modulator)
  - Desonera a CPU da geração dos sinais PWM
  - Insere automaticamente o **Dead Time** entre os transistores de um mesmo braço para prevenir shoot-through
  - Duty Cycle aplicado ao High-Side para controle de corrente/velocidade
  - Low-Side mantido em condução plena (ou chaveado de forma complementar para frenagem regenerativa)

#### Lógica de Comutação de Seis Passos — Tabela de Estados dos MOSFETs

| Passo | Pos. Elétrica | High A | Low B | High C | Low A | High B | Low C |
|-------|--------------|--------|-------|--------|-------|--------|-------|
| 1 | 0°–60° | ON | ON | OFF | OFF | OFF | OFF |
| 2 | 60°–120° | ON | OFF | OFF | OFF | OFF | ON |
| 3 | 120°–180° | OFF | OFF | ON | OFF | ON | OFF |
| 4 | 180°–240° | OFF | OFF | ON | ON | OFF | OFF |
| 5 | 240°–300° | OFF | ON | OFF | ON | OFF | OFF |
| 6 | 300°–360° | OFF | OFF | OFF | OFF | ON | ON |

#### Mapeamento MCPWM por Passo

| Passo | Fases | High-Side (PWM) | Low-Side (ON) | Fase em Hi-Z |
|-------|-------|-----------------|---------------|--------------|
| 1 | A+ B- | T1 (A) | T4 (B) | C |
| 2 | A+ C- | T1 (A) | T6 (C) | B |
| 3 | B+ C- | T3 (B) | T6 (C) | A |
| 4 | B+ A- | T3 (B) | T2 (A) | C |
| 5 | C+ A- | T5 (C) | T2 (A) | B |
| 6 | C+ B- | T5 (C) | T4 (B) | A |

*"PWM / LOW" = modulação; "LOW / ACTIVE" = condução ao terra; "Hi-Z" = alta impedância*

#### Máquina de Estados de Partida Sensorless (Firmware)

- **Problema:** FCEM = K_e × ω → nula em ω=0. Comparadores cegos na partida.
- **Estado 1 — Alinhamento (estático):**
  - Aplicação de tensão DC constante em par de fases (ex: Fase A High, Fase B Low) por **500 ms**
  - O rotor se alinha com o campo estático → posição θ₀ conhecida
  - Duração e magnitude são parâmetros críticos: insuficientes → torque inicial fraco → falha na partida
- **Estado 2 — Aceleração em Malha Aberta (Blind Commutation):**
  - Comutação de seis passos via temporizador interno, sem realimentação dos comparadores
  - Frequência de comutação incrementada linearmente (rampa)
  - Mantido até 5%–10% da velocidade nominal → FCEM supera o limiar de histerese e ruído dos comparadores LM339
- **Estado 3 — Malha Fechada (Auto-Comutação via ZCD):**
  - Handover quando microcontrolador detecta sequência válida de ZCDs nos comparadores
  - Temporizador de malha aberta desativado
  - A partir daqui: cada evento de ZCD dispara uma interrupção externa no ESP32 → atraso calculado de **30° elétricos** → executa próxima comutação de fase
  - Motor opera auto-sincronizado em malha fechada

#### Sistema de Realimentação ZCD — Circuito Completo

**Bloco 1 — Reconstrução do Neutro Virtual:**
- Problema: motor Turnigy XK3674 possui fechamento interno (Δ ou Y) sem acesso ao ponto neutro físico
- Solução: 3 resistores de **33kΩ**, um em cada fase (A, B, C), com extremidades reunidas em nó comum → fornece V_ref (tensão média) para os comparadores

**Bloco 2 — Divisor Resistivo de Proteção (por fase):**
- R_high = **33kΩ** (lado alto, conectado à fase do motor)
- R_low = **3,3kΩ** (lado baixo, conectado ao SGND)
- Cálculo do fator de atenuação: V_out = V_in × R_low / (R_high + R_low) = 25,2 × 3,3k / 36,3k ≈ **2,29V** no pino do ESP32 para bateria 6S carregada
- Objetivo: manter V_pino < 3,3V em todos os casos (incluindo spikes de comutação)
- Fator de atenuação: ~1:11

**Bloco 3 — Filtro RC Passa-Baixa (rejeição de ruído PWM):**
- Componente: capacitor cerâmico **C_f = 10nF** em paralelo com R_low (3,3kΩ)
- Cálculo da frequência de corte: f_c = 1 / (2π × (R_high || R_low) × C_f) = 1 / (2π × 3000 × 10nF) ≈ **5,3 kHz**
- Objetivo: atenuar o ruído de comutação (~20kHz) enquanto preserva a componente fundamental da FCEM (proporcional à velocidade do motor)

**Bloco 4 — Análise e Compensação do Atraso de Fase:**
- O filtro RC introduz um polo que atrasa o sinal de FCEM detectado pelo comparador em φ = arctan(f_el / f_c) graus
- Cálculo da frequência elétrica máxima do motor: f_el_max = (48.840 RPM × 4 polos) / 120 ≈ **1.628 Hz**
- Cálculo do atraso de fase máximo: φ_max = arctan(1628 / 5300) ≈ **17,07°**
- Como φ_max (17,07°) < intervalo de comutação padrão (30°), a compensação é viável via software
- **Estratégia de compensação:** calcular dinamicamente φ a cada ciclo e ajustar o temporizador de espera: T_wait = T_30° − T_lag
- Objetivo: manter o ângulo entre campos do estator e rotor próximo de 90° em toda a faixa de velocidade operacional

**Bloco 5 — Comparadores LM339N:**
- **Modelo:** LM339N (DIP-14, 4 comparadores independentes)
- **Saída:** coletor aberto (open-collector) → permite pull-up de 10kΩ para 3,3V (nível lógico do ESP32), com alimentação do comparador em tensão superior (5V ou 12V) para linearidade
- **Configuração por canal:**
  - Entrada (+): tensão de fase atenuada e filtrada
  - Entrada (−): tensão do neutro virtual atenuada
  - Saída: GPIO do ESP32 configurado para **interrupção externa**
- **3 comparadores utilizados** (um por fase A, B, C); o 4º canal disponível

**Bloco 6 — Escalabilidade e Limitações:**
- **Limite inferior de tensão (3S = 11,1V):** tensão mínima determinada pelos drivers IR2110 e pelo V_GS(th) dos MOSFETs. Abaixo de 3S, os transistores podem operar na região linear → superaquecimento.
- **Limite superior de tensão (6S = 25,2V):** picos indutivos de comutação podem atingir o dobro da tensão de barramento (~50V), próximo ao V_DSS = 60V dos IRFS7530. Baterias 7S (29,4V) ou 8S (33,6V) colocariam os semicondutores em risco de ruptura catastrófica. O limite de 6S garante a margem de segurança necessária.
- **Baixa velocidade / baixa tensão de barramento:** atenuação de 11:1 reduz amplitude do sinal de FCEM no comparador → risco de instabilidade. Solução recomendada: histerese no comparador via resistor de realimentação positiva de 1MΩ entre saída e entrada (+).

#### Simulação Computacional

- **Software:** LTSpice ou Proteus (modelos comportamentais, devido à complexidade de modelos SPICE proprietários dos drivers de gate)
- **Objetivos da simulação:**
  1. Verificar as formas de onda de tensão de linha e de fase
  2. Garantir que a sequência de seis passos não gera sobreposição de condução (shoot-through) nos braços do inversor
- **Escopo:** validação da lógica de controle e integridade dos sinais de disparo antes da montagem física

#### Prototipagem e PCB

- **Substrato:** PCB projetada para suportar altas correntes do barramento DC
- **Requisitos de layout:** trilhas reforçadas, planos de terra adequados
- **Procedimento de teste:**
  1. **Smoke test (teste de fumaça):** cargas leves + alimentação limitada → verificação de segurança e ausência de curtoscircuitos
  2. **Acionamento em rampa de velocidade:** progressão gradual para validação funcional completa do protótipo

---

### Metodologia

- **Abordagem:** engenharia de sistemas top-down. Sequência: especificação da carga → fonte de energia → proteção → potência → controle → realimentação → simulação → prototipagem.
- **Dimensionamento analítico** de todos os componentes críticos com cálculos explícitos de margens de segurança (30% de margem na corrente da bateria, >100% em V_DSS, 10% no fusível, 15× Q_g no bootstrap).
- **Decisão por parâmetros estimados:** indutância (L ≈ 20µH) e resistência (R ≈ 10mΩ) do motor foram extraídos da literatura por classe (3674, 4 polos) devido à ausência desses dados no datasheet do fabricante (Turnigy).
- **Trade-off de frequência de chaveamento:** análise explícita de três critérios conflitantes (perdas térmicas, ruído acústico, ripple de corrente) para justificar a escolha de 20kHz como ponto ótimo de operação.
- **Validação teórica pré-fabricação:** compatibilidade do driver IR2110 verificada por cálculo de tempo de propagação (< 0,5% do ciclo), corrente média de gate (7,08mA << 2A) e tempo de comutação (290ns < 1,5% do período).
- **Compensação de atraso de fase via software:** abordagem dinâmica (T_wait = T_30° − T_lag) como alternativa ao ajuste físico do filtro RC.
- **Protocolo de testes:** smoke test → rampa de velocidade → validação funcional.

---

### Ganchos para Resultados

1. **Validação dos parâmetros estimados do motor (L ≈ 20µH, R ≈ 10mΩ):** o capítulo assume explicitamente valores da literatura por ausência de datasheet. Os resultados **devem** confirmar (ou corrigir) esses parâmetros através de medição direta (ponte RLC ou método de rampa de corrente) ou inferência experimental — pois eles afetam diretamente o dimensionamento da frequência de chaveamento e o modelo do filtro RC.

2. **Validação térmica dos MOSFETs (11,6W por transistor):** o cálculo de P_diss = 11,6W é teórico e assume R_DS(on) máximo em temperatura de junção elevada. Os resultados **devem** apresentar a temperatura real de operação dos transistores (via sensor NTC ou câmera termográfica) sob carga para confirmar que a área de cobre da PCB é suficiente ou documentar a necessidade de dissipador externo.

3. **Validação da capacitância do Link DC (940µF):** o dimensionamento teórico prevê que 940µF é suficiente para absorver os transientes de corrente. A simulação e os ensaios práticos **devem** quantificar a eficácia do banco (amplitude residual de ripple de tensão no barramento) e confirmar que os capacitores não sobreaquecem.

4. **Calibração empírica dos parâmetros de partida sensorless:** o capítulo fixa arbitrariamente 500ms para o alinhamento e um perfil de rampa genérico. Os resultados **devem** apresentar a calibração experimental desses parâmetros (tempo de alinhamento, taxa de incremento da rampa, limiar de transição para ZCD) para o motor Turnigy XK3674 específico.

5. **Validação do ganho do amplificador de corrente (A_v = 73):** o ganho foi calculado assumindo que 91A de pico gera 45,5mV no shunt de 0,5mΩ. Os resultados **devem** confirmar a linearidade e precisão da cadeia de sensoriamento (shunt + amplificador) e validar o ajuste do limiar de OCP.

6. **Eficácia da compensação de atraso de fase (φ_max ≈ 17,07°):** o capítulo propõe compensação via software (T_wait = T_30° − T_lag). Os resultados **devem** demonstrar que a compensação é executada corretamente no firmware e que a comutação ocorre no ponto ótimo (ou documentar o desvio angular residual) em toda a faixa de velocidade operacional.

7. **Limiar de transição malha aberta → malha fechada (5%–10% da velocidade nominal):** a metodologia define esse limiar como o ponto em que a FCEM supera o limiar de histerese e ruído dos comparadores LM339. Os resultados **devem** confirmar empiricamente que esse limiar é atingível de forma confiável e repetível com os componentes especificados.

8. **Operação do LM339 em baixa velocidade e eficácia da histerese recomendada:** o capítulo aponta o risco de instabilidade em baixas rotações (atenuação 11:1 reduz o sinal de FCEM) e recomenda histerese. Os resultados **devem** caracterizar a velocidade mínima de operação confiável do ZCD no hardware construído.

9. **Verificação de ausência de shoot-through:** a simulação deve confirmar que a sequência de seis passos, combinada com o Dead Time automático do MCPWM, nunca resulta em condução simultânea dos transistores High-Side e Low-Side do mesmo braço.

---

*Seção adicionada em: 2026-06-21 | Fonte: `capitulos/3_metodologia.tex` (leitura direta e isolada)*

---

## Resumo: `4_resultados_discussao.tex` — Leitura Direta e Isolada

> **Nota:** Esta seção é o resultado da leitura dedicada e isolada do arquivo `capitulos/4_resultados_discussao.tex`, seguindo a taxonomia padrão. Todo conteúdo anterior é preservado integralmente.

---

### Anotações de Desenvolvimento Encontradas no Arquivo (Comentários TeX)

> Estes são comentários presentes no código-fonte LaTeX com anotações diretas de desenvolvimento, preservados por seu valor de engenharia:
>
> - `%TODO: Inserir Introdução ao capítulo.` — **a introdução do capítulo de Resultados ainda está ausente no arquivo.** Antes da submissão, é necessário redigir um parágrafo de abertura que contextualize as seções de simulação e projeto eletrônico.
> - `%TODO? Falar sobre a modelagem dos capacitores da simulação?` — **decisão em aberto** sobre incluir ou não a justificativa para a ausência dos capacitores do Link DC na simulação (eles foram deliberadamente omitidos para expor o estresse máximo sobre a bateria). Esta discussão pode fortalecer a justificativa do banco capacitivo no projeto físico.

---

### Premissas e Objetivos

Este capítulo responde às hipóteses, dimensionamentos e expectativas levantadas nos capítulos anteriores por meio de dois vetores complementares de validação:

1. **Validação Computacional (Simulação LTSpice):** confirmar analiticamente, antes da montagem física, que o hardware de potência projetado opera dentro dos parâmetros esperados — correntes de fase, tensões de neutro, esforço sobre a bateria e impacto da ausência do banco capacitivo do Link DC.

2. **Projeto Eletrônico e Análise dos Esquemáticos:** documentar as decisões de projeto definitivas do hardware físico, incluindo todas as **divergências em relação ao dimensionamento preliminar da Metodologia**, e justificar fisicamente cada alteração topológica.

**Estrutura de seções do capítulo:**
- §1 Validação Computacional (Simulação) — *Introdução pendente (TODO)*
  - Modelo de simulação (alimentação, comutação, carga)
  - Resultado 1: Dinâmica das correntes de fase (I_A, I_B, I_C)
  - Resultado 2: Tensões de fase referenciadas ao neutro (V_AN, V_BN, V_CN)
  - Resultado 3: Análise dinâmica da bateria sem filtragem capacitiva
- §2 Projeto Eletrônico e Análise dos Esquemáticos — *Introdução pendente (TODO)*
  - §2.1 Circuito de Alimentação Auxiliar e Regulação (Supply)
  - §2.2 Unidade de Processamento Lógico e Interface de Acionamento (ESP32 + IR2110)
  - §2.3 Condicionamento de Sinais e Sensoriamento (INA240 + LM339 OCP + divisor de tensão)
  - §2.4 Estágio de Potência e Ponte Inversora (MOSFETs + shunts + pull-downs)
  - §2.5 Estratégias de Layout, Roteamento e Fabricação da PCB

---

### Hardware e Firmware

#### Modelo da Simulação LTSpice

- **Software:** LTSpice
- **Cenários elaborados:** (1) topologia monofásica (meia-ponte) para análise isolada; (2) topologia trifásica completa para validação do fluxo de corrente inter-fases
- **Modelo da bateria:** fonte DC de 22,2V com resistência série (R_ser) de **19mΩ** — emula a dinâmica de descarga e perdas ôhmicas internas da LiPo 6S
- **Modelo dos MOSFETs:** transistores comportamentais BSC028N06LS3
- **Resistores de gate na simulação:** 10Ω (limitar corrente de pico do driver e amortecer ringing parasita)
- **Modelo dos drivers de gate:** fontes de tensão do tipo pulso (PULSE)
- **Parâmetro do sinal de acionamento High-Side da Fase A:** `PULSE(0 15 0 100n 100n 25u 50u)` → amplitude: 15V; período: 50µs (f_sw = 20kHz); duty cycle: 50%; tempos de transição subida/descida: 100ns
- **Modelo do motor (rotor bloqueado):** três ramos RL em estrela — R = 10mΩ e L = 20µH por fase; neutro comum
- **Justificativa do modelo de rotor bloqueado:** condição de máximo estresse elétrico sem oposição da FCEM → avalia o limite operacional do hardware; garante que os resultados representem o pior caso
- **Escopo estático do Passo 1:** apenas Fase A (High-Side PWM) e Fase B (Low-Side ON) ativos. Transistor Low-Side da Fase A mantido em corte. Dead Time dinâmico dispensado (isolamento ideal assumido entre estados de condução).
- **Capacitores do Link DC:** **deliberadamente omitidos** da simulação para expor o esforço máximo sobre a bateria e validar a necessidade do banco capacitivo

#### Resultados Quantitativos da Simulação

**Resultado 1 — Correntes de Fase (primeiros 100µs, Passo 1):**
- **I_A:** ascensão linear até ~13,5A durante T_on (0–25µs). Decaimento suave durante T_off (25–50µs) via diodo de roda livre (freewheeling diode) intrínseco ao MOSFET inferior da Fase A. No segundo ciclo (50–100µs), acumula e atinge ~26A.
- **I_B:** mesma magnitude de I_A com polaridade invertida (−13,5A → −26A)
- **I_C:** permanente em ~0A (somente ruído numérico do simulador)
- **Física do comportamento:** topologia High-Side PWM / Low-Side ON faz a corrente passar por duas bobinas em série (L_eq × di/dt + R_eq × i). A inclinação constante (di/dt) é ditada pela reatância indutiva do motor. No T_off, a energia magnética armazenada nos indutores sustenta a corrente via diodo de roda livre → decaimento lento.
- **Validação confirmada:** I_C ≈ 0A valida a eficácia do estado Hi-Z no braço não excitado — **condição sine qua non para medição limpa da FCEM durante a operação em malha fechada**

**Resultado 2 — Tensões de Fase Referenciadas ao Neutro (Passo 1):**
- **V_AN:** oscila em torno de +11,1V durante T_on
- **V_BN:** oscila em torno de −11,1V durante T_on
- **V_CN:** oscila em torno de 0V (fase flutuante)
- **Física do V_neutro = 11,1V:** com Fase A em V_DC = 22,2V e Fase B em 0V, o nó neutro assume o potencial médio exato de 22,2V/2 = 11,1V — divisão de potencial direta no fechamento estrela
- **Ringing sobreposto:** artefato numérico do simulador — fase C flutuante + indutores puros + capacitâncias parasitas dos MOSFETs formam tanque LC não amortecido. No circuito físico real, seria atenuado pelas perdas no núcleo ferromagnético do motor e pelo filtro RC do circuito de leitura de ZCD.
- **Ensaios paralelos:** confirmaram que o transistor inferior está em 15V (condução plena) e os transistores da fase flutuante em 0V — robustez da topologia contra disparos espúrios confirmada
- **Validação confirmada:** formação e estabilidade do potencial de neutro ratificam a viabilidade da técnica de **reconstrução de neutro virtual** adotada no hardware

**Resultado 3 — Análise Dinâmica da Bateria (sem Link DC):**
- **Tensão durante T_on:** decai gradualmente de 22,2V para ~21,9V (V_drop = I_a × R_ser = 13A × 19mΩ)
- **Corrente nos instantes de comutação (50µs e 100µs):** picos agudos de >**130A** e >**200A** respectivamente
- **Tensão durante os picos (voltage sags):** cai para ~19,5V
- **Corrente durante T_off:** cessa completamente (corrente de motor circula em roda livre pelos diodos da ponte, não drena da bateria)
- **Causa física dos picos:** elevada taxa di/dt necessária para carregar capacitâncias parasitas dos MOSFETs + corrente de recuperação reversa (reverse recovery) dos diodos corporais no turn-on. Sem o banco capacitivo, toda a carga transitória é fornecida pela bateria.
- **Conclusão validada pela simulação:** submeter a LiPo a correntes de ripple e picos transientes desta magnitude resultaria em superaquecimento severo e degradação química prematura das células → **confirma a necessidade imperativa do banco de 940µF Low-ESR** no projeto físico final

#### Projeto Eletrônico — Componentes Definitivos e Divergências em Relação à Metodologia

**Estágio de Alimentação (Supply) — Esquemático Final:**

| Componente | Valor na Metodologia | Valor no Esquemático Final | Justificativa da Divergência |
|---|---|---|---|
| Fusível MIDI | 100A | **80A** (modelo 0498080.M) | — (não explicado explicitamente no texto) |
| Capacitores do Link DC | 2× 470µF / 35V em paralelo (total: 940µF) | **6× 220µF eletrolíticos** + **3× 1µF cerâmicos** em paralelo | Mais unidades de 220µF em paralelo → ESR total menor por unidade; cerâmicos de 1µF suprimem transientes de alta frequência que os eletrolíticos não filtram (pela ESL); topologia mais robusta a ripple |
| BEC (regulação auxiliar) | 2×LM2596 em **cascata** (22,2V→15V→5V) | 2×LM2596 em **paralelo** (22,2V→15V e 22,2V→5V independentes) | Elimina gargalo térmico (regulador de 5V não drena corrente pelo de 15V); maior eficiência; isolamento térmico e elétrico entre Vcc1 (gate drive) e Vcc2 (lógica digital) |
| Separação de terra | — | **PGND** e **SGND** interligados por RJ1 (único ponto — Star Ground) | Previne ground bounce sobre o referencial do microcontrolador causado pelas correntes de comutação do motor |

**Unidade de Processamento e Interface de Acionamento — Esquemático Final:**

- **ESP32:** alimentado por Vcc2 (5V). O regulador linear interno reduz para 3,3V, exportado como **"Vmicro Ref"** pelo pino 1. O Vmicro Ref é roteado diretamente para os pinos VDD dos três IR2110 → garante que o logic high threshold dos drivers seja idêntico à tensão de saída dos GPIOs do ESP32, eliminando falhas de interpretação por incompatibilidade de nível.
- **IR2110 — segregação de terra:** VSS (referencial lógico) → SGND; COM (retorno de potência de alta corrente) → PGND.
- **Controle de segurança bidirecional:** ESP32 controla pinos Shutdown (SD) de cada IR2110 individualmente. Pino **OC Trip (Overcurrent Trip)** permite que evento de falta detectado pelos sensores interrompa o MCPWM no hardware do ESP32 em microssegundos.
- **Bootstrap — Topologia final** (divergência da Metodologia):
  - Diodo: **UF4004** (recuperação ultra-rápida) — *a Metodologia não especificava o modelo do diodo*
  - Capacitor: **4,7µF** (eletrolítico — reserva de carga) em paralelo com **0,1µF** (cerâmico — resposta em alta frequência) → *Metodologia previa 10µF + 100nF*
- **Resistência de gate assimétrica** (NOVA decisão, não presente na Metodologia):
  - Turn-on: resistores de **10Ω** (amortece ringing e picos de EMI durante a subida)
  - Turn-off: diodos Schottky **1N5819** em paralelo reverso com os resistores (caminho de baixíssima impedância para escoamento imediato de Q_g → transistor desliga mais rápido do que liga → **previne shoot-through**)

**Condicionamento de Sinais e Sensoriamento — Esquemático Final:**

- **Monitoramento de tensão da bateria:**
  - Divisor resistivo: **39kΩ** (R_high) / **4,7kΩ** (R_low) → fator de atenuação: 0,1075
  - Mapeamento: 25,2V (6S carregada) → 2,71V; 9,0V (3S descarregada) → 0,97V — toda a janela operacional dentro de 0V–3,3V do ADC do ESP32
  - Filtro passa-baixa: capacitor de **0,1µF** com R_th ≈ 4,19kΩ → f_c ≈ **379 Hz** — os 20kHz do inversor ficam quase duas décadas acima da f_c (atenuação de −20dB/década), impedindo que flutuações de comutação cheguem ao ADC
  - *Esta subcircuito não estava detalhado na Metodologia*

- **Amplificador de corrente de fase — INA240A1DR** (divergência: Metodologia previa OpAmp genérico):
  - Ganho fixo: **20V/V**
  - Tecnologia **Enhanced PWM Rejection:** suprime grandes variações de dv/dt inerentes à comutação de motores
  - Técnica de polarização diferencial para leitura bidirecional: REF1 = **3,3V** (Vmicro Ref); REF2 = **SGND** (0V) → offset interno de **1,65V** na saída
  - Mapeamento bidirecional: 0A → 1,65V; corrente positiva → sobe em direção a 3,3V; corrente negativa → desce em direção a 0V
  - **Pré-requisito para implementação futura de FOC:** a bidirecionalidade é condição necessária para os algoritmos de controle vetorial de corrente
  - 3× INA240A1DR, um por fase (A, B, C) — posicionados no Low-Side

- **OCP (Proteção de Sobrecorrente por Hardware) — LM339N:**
  - Arquitetura Wired-OR com pull-up R10: 3 comparadores (um por fase) com saídas em coletor aberto interligadas em nó único (pino OC Trip)
  - **Ajuste topológico realizado durante revisão do projeto:** inversão da polaridade — Vdac Ref na entrada (+); Isense na entrada (−). Em operação nominal (Isense < Vdac): saídas em alta impedância → R10 mantém OC Trip em 3,3V. Em falta (Isense > Vdac em qualquer fase): comparador satura → OC Trip vai a 0V em nanosegundos → interrupção prioritária no ESP32 → bloqueio imediato do MCPWM
  - **Vantagem crítica:** ação puramente em hardware, sem latência de software → salvaguarda a ponte inversora contra catástrofes térmicas e elétricas

**Estágio de Potência — Esquemático Final:**

- **6× MOSFETs IRFS7530-7PPBF** em encapsulamento D2PAK-7 (múltiplos terminais de Source → minimiza indutância parasita e atenua ringing durante transientes de até 90A)
- **Resistores de pull-down 10kΩ (R13 a R18)** — Gate-Source de cada MOSFET (NOVO, não estava na Metodologia):
  - Função: garantir estado fail-safe (V_GS = 0V) durante power-up, reset do MCU ou ruptura de trilha de acionamento
  - Previne acionamento espúrio por acoplamento capacitivo de ruído eletromagnético → evita shoot-through catastrófico no barramento DC
- **3× resistores shunt de liga metálica** (divergência: Metodologia previa 0,5mΩ único):
  - Modelo: **CSS2H-3920R-1L00F**
  - Resistência: **1mΩ** por fase (vs. 0,5mΩ na Metodologia)
  - Encapsulamento: 3920 (potência)
  - Posição: Low-Side de cada braço da ponte (sensoriamento referenciado ao PGND, sem exposição do INA240 às oscilações de modo comum de 0V–25,2V dos nós flutuantes das fases)

**Layout e Fabricação da PCB — Decisões Definitivas:**

- **Substrato:** FR4, 150×200mm, espessura 1,5mm, **cobre nu** (sem solder mask comercial)
- **Método de fabricação:** **fresadora CNC** (substituiu corrosão química manual)
  - Motivo: corrosão de placas com grandes áreas de cobre gera undercutting (corrosão lateral) e variações de espessura. CNC remove mecanicamente apenas o material necessário para os clearances, preservando a massa total e a espessura original do cobre.
- **Regras de DRC para CNC:** largura mínima de trilha lógica: **0,6mm**; clearance mínimo entre elementos: **0,6mm**
- **Roteamento de potência:** copper pours sólidos (sem trilhas convencionais); gargalos mínimos de **5mm** de largura no Top Layer
- **Reforço de corrente das trilhas:** deposição manual de camadas de estanho e condutores de cobre sólido sobre os polígonos expostos → aumenta área da seção transversal A → reduz R_ôhmica (ρ × L/A)
- **EMC — Star Ground:**
  - PGND: camada inferior (Bottom Layer), concentrado sob o inversor
  - SGND: periferia dos circuitos lógicos e sensores
  - Interligação exclusiva por ponto único → previne ground loops e ground bounce no referencial do microcontrolador
- **EMC — Distanciamento físico do ESP32:** posicionado no extremo oposto à ponte de MOSFETs → atenua acoplamento magnético (di/dt alto) em proporção ao inverso do quadrado da distância → protege o microcontrolador contra perturbações eletromagnéticas e travamentos espúrios
- **Escopo do protótipo inicial:** testado com **bateria 4S e limite de 3A** (demonstração de viabilidade do design) — operação em regime de 90A não é esperada nesta fase de validação

---

### Metodologia

- **Simulação LTSpice em dois estágios:** (1) validação monofásica isolada (meia-ponte); (2) validação trifásica completa do fluxo de corrente inter-fases com modelo de rotor bloqueado.
- **Modelo de pior caso:** rotor bloqueado (sem FCEM) + capacitores do Link DC propositalmente omitidos → expõe o máximo estresse elétrico sobre a bateria e os semicondutores.
- **Processo de análise dos esquemáticos:** comparação sistemática entre os valores dimensionados na Metodologia e os valores finais implementados nos esquemáticos (Altium Designer), com justificativa física de cada divergência.
- **Fabricação:** migração de corrosão química para fresagem CNC por razões de precisão geométrica e integridade do cobre.
- **Protocolo de testes inicial:** bateria 4S + corrente limitada a 3A → smoke test de segurança antes de progressão para o regime nominal.

---

### Ganchos para Resultados

> **Observação:** este capítulo **é** o capítulo de Resultados e Discussões. As seções abaixo identificam os elementos que ainda estão **ausentes ou incompletos** no arquivo atual e que **devem ser preenchidos antes da entrega final.**

1. **Introdução do capítulo (TODO pendente):** redigir parágrafo de abertura que contextualize as seções de simulação e análise de esquemáticos dentro do objetivo geral do TCC.

2. **Discussão sobre a modelagem dos capacitores na simulação (TODO em aberto):** decidir se inclui ou não o parágrafo explicando por que o Link DC foi omitido da simulação. **Recomendação:** incluir — fortalece a justificativa do banco capacitivo e demonstra raciocínio de engenharia rigoroso.

3. **Validação experimental do protótipo físico (ausente no arquivo atual):** os esquemáticos e o layout da PCB estão documentados, mas os **resultados dos testes práticos** (smoke test, acionamento do motor, medições com osciloscópio) ainda não foram inseridos no capítulo. Estes são os resultados mais críticos para validar o objetivo do TCC ("operação suave e precisa").

4. **Comparação correntes simuladas vs. correntes medidas:** I_A/I_B ≈ 13,5A na simulação (rotor bloqueado, 50% duty cycle). Os ensaios experimentais **devem** confirmar (ou explicar desvios de) este valor com o hardware físico.

5. **Eficácia real do banco de capacitores (940µF):** a simulação quantificou os transientes **sem** filtro (V_sag até 19,5V, I_pico > 200A). O protótipo físico **deve** apresentar medição de ripple de tensão no barramento **com** o banco instalado, quantificando a atenuação efetiva dos transientes.

6. **Validação da partida sensorless (máquina de estados):** o firmware foi descrito na Metodologia mas seus resultados (forma de onda ZCD, tempo de convergência para malha fechada, ocorrência de stall) ainda não constam no capítulo.

7. **Validação da cadeia de sensoriamento de corrente (INA240 + shunt 1mΩ):** confirmar linearidade e precisão do ganho de 20V/V com offset de 1,65V; verificar a eficácia da rejeição de PWM em condição real de comutação.

8. **Validação do circuito OCP (LM339 Wired-OR):** demonstrar o tempo de resposta real da proteção em condição de sobrecorrente simulada e confirmar que o bloqueio do MCPWM ocorre dentro dos limites térmicos seguros dos MOSFETs.

9. **Temperatura real dos MOSFETs em operação:** validar se os copper pours + reforço de estanho são suficientes para manter T_junção dentro dos limites seguros para a corrente de teste (3A inicial) e projetar o comportamento em regime nominal.

10. **Justificativa da divergência do fusível (80A vs. 100A):** ~~justificativa ausente~~ → **RESOLVIDO em 2026-06-22**. Justificativa inserida no Capítulo 4, subseção Supply: curva de tempo-corrente do MIDI permite picos transitórios acima do nominal sem atuação; 80A protege melhor contra sobrecargas sustentadas; modelo 0498080.M é valor padronizado comercialmente.

---

*Seção adicionada em: 2026-06-21 | Fonte: `capitulos/4_resultados_discussao.tex` (leitura direta e isolada)*

---

## Registro Retroativo — Capítulo 4, Seções 3 e 4: Firmware e ZCD (sessão anterior a 2026-06-22)

> **Nota:** Estas seções foram escritas em sessão anterior e não possuíam registro na MEMORIA\_TCC.md. Registradas retroativamente em 2026-06-22 após auditoria de completude.

---

### `\section{Desenvolvimento e Validação do Firmware}`

Seção de validação arquitetural do firmware, independente dos ensaios físicos. Organizada em quatro subsecções:

| Subseção | Conteúdo |
|----------|----------|
| Arquitetura Modular e Isolamento da HAL | Demonstração por inspeção estrutural da inversão de dependência: `pid_regulator` não referencia `board_config.h`; conversão de tensão→corrente confinada em `ina240_current_sensors`; compilação condicional `BOARD_ENABLE_BEMF_ZCD` e `MOTOR_CONTROL_USE_SPEED_MODE` |
| Máquina de Estados e Robustez das Transições | Sequência determinística de inicialização (9 etapas); análise das guardas de transição IDLE→RUNNING (UVLO + fault check); sub-FSM de partida ALIGN→RUN\_OPEN→RUN\_SPEED; inversão de sentido bloqueada com torque ativo |
| Rotinas de Proteção e Tratamento de Exceções | OCP hardware (LM339 → ISR IRAM\_ATTR → $t_{resp}$ microssegundos); OCP software (`motor_control_tick()`, 1 ms); UVLO (debounce 100 ms, histerese 200 mV/célula); Detecção de Stall (3 critérios independentes); sequência unificada `enter_fault_state()` |
| Parametrização e Comportamento com A2212/10T | Justificativa de `MOTOR_POLE_PAIRS = 7U` e `MOTOR_OPEN_LOOP_COMM_HZ_MAX = 300.0f`; equação de estimativa de RPM; mapeamento R2→setpoint; filtro exponencial de RPM (7/8 + 1/8); interface DualShock 4 e lightbar FSM |

**Lacunas (LACUNA BANCADA)** presentes nesta seção:
- Oscilograma da sequência de partida (ALIGN → RUN\_OPEN → RUN\_SPEED)
- Oscilograma do evento OCP (4 canais)
- Gráfico de tensão de barramento durante UVLO
- Ganhos Kp e Ki finais do PI (corrente e velocidade)
- Comparação RPM estimado vs tacômetro óptico
- Oscilograma da BEMF a 2571 RPM (preparação para ZCD)

---

### `\section{Estado de Implementação da Detecção de Cruzamento por Zero (ZCD)}`

Seção que estabelece explicitamente o escopo da implementação atual: **`BOARD_ENABLE_BEMF_ZCD = 0`** — a malha fechada ZCD não foi ativada nesta versão. Justificativas:

1. Prioridade de validação arquitetural em malha aberta antes de introduzir nova variável de falha
2. Limitação da bancada atual (3 A com A2212) insuficiente para caracterizar o ponto de handover ZCD de forma reprodutível
3. Escopo delimitado: implementação ZCD + validação empírica de `T_wait` é o próximo passo, a ser conduzido com o motor Turnigy XK3674 em escalonamento de potência

**Hardware ZCD projetado e presente na PCB:** divisores resistivos de fase, filtros RC, neutro virtual, comparadores LM339 — a ausência é de software, não de hardware.

---

### Atualização dos Ganchos para Resultados (Cap. 4)

Os itens abaixo da lista de pendências (registrada em 2026-06-21) foram endereçados por seções já escritas:

| Item | Status | Onde endereçado |
|------|--------|-----------------|
| 3 — Validação experimental do protótipo | ✅ Esqueleto escrito | `\subsection{Validação Estática}` (Setup 1-3) + `\subsection{Análise Dinâmica}` (Setup 4) — 2026-06-22 |
| 5 — Eficácia do banco de capacitores | ✅ Esqueleto escrito | Setup 3 — ripple DC em acoplamento AC — 2026-06-22 |
| 6 — Validação da partida sensorless (stall, ZCD) | ✅ Esqueleto escrito | Sub-teste 4.1 (partida trapezoidal) + Sub-teste 4.5 (stall por travamento) — 2026-06-22 |
| 7 — Validação INA240 + shunt 1 mΩ | ✅ Esqueleto escrito | Setup 1 (calibração de offset e ganho) — 2026-06-22 |
| 8 — Validação OCP LM339 | ✅ Esqueleto escrito | Setup 3 (OCP trip) — 2026-06-22 |
| 4 — Comparação correntes simuladas vs medidas | ⏳ Pendente | Lacunas nos Sub-testes 4.1 (correntes de fase com motor) |
| 9 — Temperatura MOSFETs | ⏳ Pendente | Não coberto nas seções atuais |

*Registro retroativo adicionado em: 2026-06-22 | Auditoria de completude do MEMORIA\_TCC.md*

---

## Configuração de Motor para Testes em Bancada — A2212/10T 1400kV

> **Nota:** Esta seção documenta a introdução do motor **Brushless Outrunner A2212/10T 1400kV** como carga de teste para a fase de testes em bancada do protótipo ESC, em substituição ao Turnigy XK3674-2200KV especificado na Metodologia para o regime nominal.

---

### Especificações do Motor A2212/10T 1400kV

| Parâmetro | Valor |
|-----------|-------|
| Modelo | A2212/10T 1400kV |
| Tipo | Outrunner (rotor externo) |
| Constante de velocidade K_V | 1400 RPM/V |
| Número de polos magnéticos | 14 |
| Pares de polos (p) | 7 |
| Tensão de teste | 4S LiPo (14,8 V nominal) |
| Corrente de teste | limitada a 3 A (validação inicial de bancada) |
| `MOTOR_POLE_PAIRS` | `7U` |
| `MOTOR_OPEN_LOOP_COMM_HZ_MAX` | `300.0f` Hz |
| RPM máx. em malha aberta | ≈ 2571 RPM (300 × 60 / 7) |

---

### Firmware e Hardware

#### Justificativa Técnica — `#define MOTOR_POLE_PAIRS 7U`

O motor A2212/10T possui **14 polos magnéticos distribuídos na carcaça giratória (rotor externo)**, arquitetura construtiva outrunner de uso amplo em aeromodelismo. A relação fundamental entre a frequência elétrica de comutação do ESC e a velocidade mecânica do rotor é:

\[
f_e = p \cdot \frac{n}{60} \quad \Leftrightarrow \quad n = \frac{f_e \times 60}{p}
\]

onde \(f_e\) é a frequência elétrica em Hz, \(p\) o número de **pares de polos** e \(n\) a velocidade mecânica em RPM. Com 14 polos magnéticos, \(p = 7\) pares, e portanto:

\[
n = \frac{f_e \times 60}{7}
\]

Este parâmetro é a constante de proporcionalidade entre o ritmo de chaveamento elétrico do ESC e a rotação real do eixo. A consequência prática de um erro neste valor é o **dessincronismo permanente** entre o campo giratório do estator e os ímãs do rotor: o ESC comuta às frequências erradas, o rotor não acompanha o campo, a FCEM estimada diverge da real, e qualquer algoritmo de estimação de posição (ZCD, observador) opera em regime de falha. O parâmetro P é também denominado nos parâmetros críticos para modelagem dinâmica (Seção §5 do Capítulo 1) como requisito indispensável para FOC e observadores de estado.

**Comparação com o motor original do projeto (Turnigy XK3674-2200KV):**

| Parâmetro | Turnigy XK3674 (projeto nominal) | A2212/10T (testes bancada) |
|-----------|----------------------------------|---------------------------|
| Tipo | Inrunner | Outrunner |
| Polos | 4 | 14 |
| Pares de polos (p) | 2 | **7** |
| K_V | 2200 RPM/V | 1400 RPM/V |
| `MOTOR_POLE_PAIRS` | `2U` | **`7U`** |
| f_el para 3600 RPM | 120 Hz | **420 Hz** |

#### Justificativa Técnica — `#define MOTOR_OPEN_LOOP_COMM_HZ_MAX 300.0f`

Durante o **Estado 2 — Comutação Forçada (Rampa de Aceleração)** da estratégia de partida sensorless, o firmware incrementa linearmente a frequência elétrica de comutação em malha aberta, sem realimentação de posição do rotor. O parâmetro `MOTOR_OPEN_LOOP_COMM_HZ_MAX` define o limite superior desta rampa.

O valor `300.0f` Hz para o motor A2212/10T é fundamentado em dois critérios físicos independentes:

**Critério 1 — Prevenção de perda de sincronismo magnético (engasgo/stall):**

A comutação em malha aberta impõe ao rotor um campo giratório cuja taxa de variação angular deve ser mecanicamente compatível com a capacidade do motor de gerar torque de seguimento. Se a frequência de comutação cresce mais rapidamente do que o rotor consegue acompanhar — dado o momento de inércia rotacional \(J\) e o torque eletromagnético disponível \(T_e = K_T \cdot I_a\) —, ocorre **perda de sincronismo**: o ângulo entre o campo do estator e o campo do rotor supera 90° elétricos, o torque de seguimento colapsa, a corrente sobe abruptamente (pela ausência de FCEM limitadora), e o ESC dispara proteção de sobrecorrente ou o rotor trava (stall). O limite de 300 Hz representa a frequência elétrica máxima sustentável pelo A2212 com a taxa de incremento da rampa adotada (+1,5 Hz por passo comutado), garantindo torque suficiente em cada passo para manter o sincronismo até a transição à malha fechada.

**Critério 2 — Garantia de FCEM detectável para handover ao ZCD:**

A transição para malha fechada por ZCD (Estado 3) só é possível quando a FCEM \(e = K_e \cdot \omega\) atinge amplitude suficiente para superar o limiar de ruído e histerese dos comparadores LM339. Com \(p = 7\) e `MOTOR_OPEN_LOOP_COMM_HZ_MAX = 300 Hz`:

\[
n_{handover} = \frac{300 \text{ Hz} \times 60}{7} \approx 2571 \text{ RPM}
\]

Nesta velocidade, a FCEM é proporcional a \(K_e \cdot \omega_{handover}\), suficientemente elevada para leitura confiável e estável pelos comparadores da rede de reconstrução de neutro virtual, viabilizando o handover sem oscilações de sincronismo nem disparos espúrios.

**Nota de escalonamento:** para o motor original Turnigy XK3674 (\(p = 2\)), o mesmo teto de 120 Hz correspondia a 3600 RPM. Para o A2212 com \(p = 7\), 120 Hz corresponderia a apenas 1028 RPM — velocidade insuficiente para FCEM detectável. O limite de 300 Hz foi estabelecido para manter a mesma faixa de operação relativa (percentual da velocidade nominal) na transição à malha fechada.

---

### Metodologia

- A troca do motor de teste não altera a lógica de comutação, as malhas de controle PI, os circuitos de proteção (OCP, UVLO) nem os demais parâmetros de firmware.
- Apenas os parâmetros físicos do motor — `MOTOR_POLE_PAIRS` e `MOTOR_OPEN_LOOP_COMM_HZ_MAX` — foram ajustados, derivando automaticamente `MOTOR_SPEED_MAX_RPM ≈ 2571 RPM`.
- Os testes em bancada com A2212 visam validar a máquina de estados de partida sensorless (alinhamento 500 ms → rampa → ZCD) e a eficácia das proteções (OCP hardware e software) em condições de carga controlada (corrente limitada a 3 A), antes de progressão para o motor nominal de alta corrente.

---

### Ganchos para Resultados

1. **Validação da partida sensorless com A2212 (p=7):** confirmar que a rampa de 5→300 Hz, com incremento de +1,5 Hz/passo, permite aceleração suave até 2571 RPM sem stall no motor outrunner de 14 polos.

2. **Limiar de handover malha aberta → ZCD:** confirmar empiricamente que a FCEM gerada pelo A2212 a ≈ 2571 RPM (f_el = 300 Hz) é suficiente para leitura estável pelos comparadores LM339 com a rede de filtragem projetada (f_c ≈ 5,3 kHz).

3. **Estimativa de RPM calibrada (p=7):** validar que a expressão \(n = (10^6 / (6 \cdot T_{step})) \cdot (60/14)\) fornece estimativa coerente com a velocidade real medida por taquômetro óptico externo.

4. **Torque ripple a 14 polos:** a teoria prevê pulsações de torque na frequência de \(6 \cdot f_{el}\); com p=7 e velocidade de operação, verificar se a amplitude das pulsações difere qualitativamente do modelo para motores de 4 polos.

---

*Seção adicionada em: 2026-06-21 | Motor A2212/10T 1400kV adotado como carga de testes em bancada. `MOTOR_POLE_PAIRS = 7U`, `MOTOR_OPEN_LOOP_COMM_HZ_MAX = 300.0f`.*

---

## Registro de Escrita — Capítulo 4: Testes Físicos de Bancada (2026-06-22)

> **Fonte:** `Docs/Thesis/capitulos/4_resultados_discussao.tex` — adição de `\section{Testes Físicos de Bancada}` e `\subsection{Validação Estática e Calibração de Hardware}`.

---

### O que foi adicionado

Uma nova seção de resultados experimentais foi redigida seguindo a técnica de "Esqueleto de Artigo": o texto assume que os testes foram executados com sucesso, descreve a física e a lógica de cada procedimento, e demarcar lacunas (`\textbf{[LACUNA BANCADA: ...]}`) onde dados reais ainda precisam ser inseridos.

#### Parágrafo introdutório da seção

Descreve a filosofia de **energização incremental** como protocolo geral dos testes: motor desconectado e limitação de corrente nas fases iniciais, escalando progressivamente após validação de cada subsistema.

#### `\subsection{Validação Estática e Calibração de Hardware}`

Três setups sequenciais com motor desconectado, alimentação 12 V / 100 mA:

| Setup | Bloco validado | Método |
|-------|---------------|--------|
| 1 | Offset e ganho do INA240 (calibração de corrente) | Multímetro nas saídas dos 3 canais; verificação cruzada com `ina240_calibrate_offset()` via telemetria; injeção de 1,0 A forçado pelo shunt e medição de ΔV (esperado: 20 mV) |
| 2 | Sinais lógicos e dead-time de 500 ns | Osciloscópio nos pinos AH e AL; cursores Delta-T; verificação de simetria de bordas e amplitude $V_{GS}$ com bootstrap carregado |
| 3 | OCP trip do LM339 e ripple do barramento DC | Setup de 4 canais para capturar cadeia de proteção (INA240 → OC Trip → gate AH → IR2110 SD); medição de ripple em acoplamento AC (20 mV/div, 10 µs/div) |

---

### Lacunas demarcadas no texto (a preencher após bancada)

| Nº | Variável | Setup |
|----|----------|-------|
| 1 | $V_{off,A}$, $V_{off,B}$, $V_{off,C}$ (offset dos 3 canais INA240) | Setup 1 |
| 2 | $\Delta V_{out,medido}$ com $I = 1{,}0\,\text{A}$ e erro percentual de ganho | Setup 1 |
| 3 | $t_{dead,AH \to AL}$ e $t_{dead,AL \to AH}$ (dead-time medido) | Setup 2 |
| 4 | $V_{GS,HS}$ e $V_{GS,LS}$ com bootstrap carregado | Setup 2 |
| 5 | $t_{OCP,total}$, $t_{LM339}$, $t_{IR2110,SD}$ (cadeia OCP) | Setup 3 |
| 6 | $V_{pp,ripple}$ em acoplamento AC; comparação com modelo teórico; $ESR_{ef}$ calculada | Setup 3 |

---

### Figuras previstas (com `% TODO` no LaTeX)

| Label | Conteúdo |
|-------|----------|
| `fig:bancada_ina240_offset` | Foto do multímetro na saída do INA240 canal A em repouso |
| `fig:bancada_deadtime` | Oscilograma dos sinais AH/AL com cursores no dead-time |
| `fig:bancada_ocp_trip` | Oscilograma 4 canais do evento de disparo OCP |
| `fig:bancada_ripple_dc` | Oscilograma do ripple do barramento em acoplamento AC |

---

### Consistência com o restante do documento

- Parâmetros do firmware respeitados: dead-time 500 ns, limiar OCP 8 A → $V_{dac} \approx 1{,}81\,\text{V}$, ganho INA240 20 V/V, shunt 1 mΩ, GPIO 26 (OC Trip), DAC1 GPIO 25, calibração runtime por `ina240_calibrate_offset(64)`.
- A calibração do INA240 **não** usa constante estática em `board_config.h`; é runtime, com o multímetro servindo apenas de verificação cruzada.
- Equações inseridas: $\Delta V_{out} = G \cdot V_{shunt}$ (Eq. do ganho INA240); $di/dt|_{max} = V_{DC}/L$ (análise de tempo de resposta OCP); $\Delta V_C = I_{ripple} \cdot D / (f_{sw} \cdot C_{bus})$ (ripple teórico do barramento).
- Novos termos usados no texto: **ESR** e **ESL** — entradas adicionadas ao `GLOSSARIO_TERMOS.md` em 2026-06-22.

---

### Ganchos para Resultados (atualização de itens pendentes)

Os itens 5, 7 e 8 da lista de pendências registrada em 2026-06-21 agora possuem **esqueleto textual** no arquivo LaTeX, aguardando dados experimentais:

- **Item 5** — "Eficácia real do banco de capacitores (940 µF)": coberto pelo Setup 3, parágrafo de ripple DC.
- **Item 7** — "Validação da cadeia de sensoriamento de corrente (INA240 + shunt 1 mΩ)": coberto pelo Setup 1.
- **Item 8** — "Validação do circuito OCP (LM339 Wired-OR)": coberto pelo Setup 3, parágrafo de OCP trip.

---

*Seção adicionada em: 2026-06-22 | Fonte: `capitulos/4_resultados_discussao.tex` (adição de `\section{Testes Físicos de Bancada}` e `\subsection{Validação Estática e Calibração de Hardware}`).*

---

## Registro de Escrita — Capítulo 4: Análise Dinâmica e Controle do Motor A2212 (2026-06-22)

> **Fonte:** `Docs/Thesis/capitulos/4_resultados_discussao.tex` — adição de `\subsection{Análise Dinâmica e Controle do Motor A2212}`.

---

### O que foi adicionado

Subseção de testes dinâmicos com o motor A2212/10T conectado, alimentação 12 V / 3 A. Cinco sub-testes em ordem crescente de severidade:

| Sub-teste | Fenômeno físico validado | Lacunas |
|-----------|--------------------------|---------|
| 4.1 — Partida e 6-Step | Sequência ALIGN → RUN\_OPEN → RUN\_SPEED; correntes de fase trapezoidais; verificação do alinhamento magnético | Corrente de alinhamento; oscilograma de partida; frequência no handover |
| 4.2 — Estresse FreeRTOS | Determinismo do MCPWM (hardware autônomo) sob carga assíncrona de Bluetooth | Histograma de período do PWM; σ\_T medido |
| 4.3 — Resposta ao Degrau | Slew limiter a 2 A/s; rastreamento do PI de corrente; anti-windup | Gráfico I\_target vs I\_measured; Kp, Ki finais; t\_s e overshoot |
| 4.4 — Limite Inferior | Frequência elétrica mínima em malha aberta (stall por dessincronismo); motiva ZCD | f\_el,min e RPM correspondente |
| 4.5 — Proteção de Stall | Travamento mecânico; saturação do PI; detecção por ausência de passo; enter\_fault\_state() | Tempo total de resposta ao stall; verificação do zero do integrador pós-disarm |

### Equações inseridas

- $I_{align} \approx D_{align} \cdot V_{DC} / R_{eq}$ (corrente de alinhamento estático)
- $\Delta I_{max} = 2\,\text{A/s} \times 1\,\text{ms} = 2\,\text{mA}$ (slew limiter por ciclo)
- Equações PI com anti-windup (Euler + clamping)

### Figuras previstas (com `% TODO` no LaTeX)

| Label | Conteúdo |
|-------|----------|
| `fig:bancada_partida_correntes` | Oscilograma 3 canais: correntes de fase durante ALIGN → RUN\_OPEN → RUN\_SPEED |
| `fig:bancada_resposta_degrau` | Gráfico I\_target vs I\_measured na resposta ao degrau do R2 |
| `fig:bancada_stall` | Gráfico I\_measured + duty cycle + estado FSM durante o travamento mecânico |

### Consistência com firmware

- Todos os parâmetros referenciados existem em `board_config.h`: `MOTOR_TARGET_SLEW_AMPS_PER_S = 2`, `MOTOR_STALL_STEP_TIMEOUT_MULT = 4`, `MOTOR_OPEN_LOOP_COMM_HZ_MAX = 300.0f`, `MOTOR_POLE_PAIRS = 7U`
- Sequência `enter_fault_state()` descrita na Seção 6.8 da DOCUMENTACAO\_PROGRAMACAO.md
- Telemetria via Serial 115200 baud **e** dashboard Wi-Fi (HTTP polling) conforme Seções 5.4 e 8 da DOCUMENTACAO\_PROGRAMACAO.md

*Seção adicionada em: 2026-06-22 | Fonte: `capitulos/4_resultados_discussao.tex` (adição de `\subsection{Análise Dinâmica e Controle do Motor A2212}`).*

---

## Registro de Escrita — Capítulo 4: Métricas de Desempenho e Escalabilidade de Software (2026-06-22)

> **Fonte:** `Docs/Thesis/capitulos/4_resultados_discussao.tex` — adição de `\subsection{Métricas de Desempenho e Escalabilidade de Software}`.

---

### O que foi adicionado

Subseção de instrumentação do próprio microcontrolador (Setup 5 — bancada local, apenas USB). Dois sub-testes:

| Sub-teste | Grandeza medida | API usada | Lacunas |
|-----------|----------------|-----------|---------|
| 5.1 — Latência da Malha | $t_{tick}$ (µs) e $t_{idle}$ por ciclo de 1 ms | `esp_timer_get_time()` | $t_{tick,min}$, $t_{tick,max}$, $\bar{t}_{tick}$; $\rho_{CPU}$ calculado; correlação com eventos BT |
| 5.2 — Recursos de Memória | Heap livre e uso de Flash/RAM | `esp_get_free_heap_size()` + relatório PlatformIO | $H_{livre,IDLE}$, $H_{livre,RUNNING}$, $\eta_{heap}$; tabela de segmentos ELF (.text, .rodata, .data, .bss) |

### Equações inseridas

- $t_{tick} = t_{saída} - t_{entrada}$ (latência medida)
- $t_{idle} = 1000\,\mu\text{s} - t_{tick}$ (tempo ocioso por ciclo)
- $\rho_{CPU,controle} = t_{tick}/1000 \times 100\,\%$ (carga de CPU da malha)
- $\eta_{heap} = (H_{total} - H_{livre})/H_{total} \times 100\,\%$ (utilização de heap)

### Análise de escalabilidade inserida (sem lacuna — texto analítico)

Estimativas de custo computacional por operação de ponto flutuante na FPU Xtensa LX6 a 240 MHz (~15 ns/op), com custo aproximado de algoritmos futuros: Observador de Luenberger (<1 µs), ZCD por software (2–4 µs), EKF simplificado (<5 µs). Confirma margem para evolução sem degradar o determinismo de 1 kHz.

*Seção adicionada em: 2026-06-22 | Fonte: `capitulos/4_resultados_discussao.tex` (adição de `\subsection{Métricas de Desempenho e Escalabilidade de Software}`).*

---

## Registro de Escrita — Dashboard Wi-Fi (Metodologia, Resultados e Documentação) (2026-06-22)

> **Fonte:** `capitulos/3_metodologia.tex`, `capitulos/4_resultados_discussao.tex`, `Firmware/DOCUMENTACAO_PROGRAMACAO.md`, `Firmware/GLOSSARIO_TERMOS.md`.

---

### O que foi adicionado

| Arquivo | Conteúdo |
|---------|----------|
| `3_metodologia.tex` | `\subsection{Dashboard de Telemetria via Wi-Fi}` — justificativa de segurança (ground loops, surtos, isolamento galvânico virtual); arquitetura AP + LittleFS + HTTP polling; coexistência BT/Wi-Fi; diagrama de camadas atualizado |
| `4_resultados_discussao.tex` | `\subsection{Interface de Monitoramento e Telemetria Sem Fio}` — validação funcional assumida; gráficos sem travamento; figura `fig:dashboard_wifi_telemetry` com lacuna para screenshot |
| `DOCUMENTACAO_PROGRAMACAO.md` | Seção 8 (dashboard) já existente; reforço da motivação de segurança de bancada em §8.1 |
| `GLOSSARIO_TERMOS.md` | Entradas AP, Chart.js, Dashboard, ESPAsyncWebServer, HTTP polling, LittleFS, ps4c, wifi_telemetry; Wi-Fi corrigido de "não utilizado" para AP ativo |
| `MEMORIA_TCC.md` | Resumo Cap. 3 — bloco Dashboard Wi-Fi; consistência telemetria serial + Wi-Fi |

### Justificativa de engenharia (eixo central)

A dashboard não é recurso estético: elimina o cabo USB durante ensaios de potência, reduzindo risco de **loops de terra** e **acoplamento de transientes** do barramento BLDC ao computador do operador. A aquisição de dados permanece confiável via JSON periódico e gráficos Chart.js servidos localmente (sem CDN).

### Lacunas pendentes

- **Figura `fig:dashboard_wifi_telemetry`:** screenshot da dashboard com motor A2212 em operação
- **Sub-teste 5.2:** heap com Wi-Fi AP + BT ativos (valor típico observado em bancada: ~55 KB livres)

### Arquivos de firmware envolvidos

- `src/wifi_telemetry.cpp` / `.h`
- `src/main.cpp` (`push_wifi_telemetry`, ordem de init)
- `data/index.html`, `data/chart.min.js`
- `include/board_config.h` (`WIFI_AP_*`)
- `platformio.ini` (LittleFS, ESPAsyncWebServer, AsyncTCP)

*Seção adicionada em: 2026-06-22.*
