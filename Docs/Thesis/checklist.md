# 💽 GIG: Teste de Bancada ESC (Upload 4.1)

> **Contrato Principal:** Coletar 31 evidências experimentais para o TCC.
> **Versão 1 dia (REDUX):** [`checklist_redux.md`](checklist_redux.md) — 6 ensaios críticos + 1 opcional.
> **Drop Point (Pasta de Saída):** `Docs/Thesis/ensaio_bancada/AAAA-MM-DD/` *(Crie no início do dia!)*
>

### 🦾 Loadout do Rigger

- **Atuador Core:** Motor A2212/10T 1400 kV
- **Célula de Força:** Fonte 12 V / limite 3 A
- **Interface de Controle:** PS4 pareado
- **Ghost in the Shell:** Firmware Atual (Build de Teste)
- **Cyberdeck & Ferramentas:** Osciloscópio (1 canal), multímetro, pinça de corrente (opcional), notebook (Wi-Fi `ESC-Dashboard` + serial USB).

---

## 🏙️ NÓ A: Calibração Estática (Setup e Instrumentação)

Motor Desconectado. Configurações iniciais.

### ID 14: Offset INA240

- [x] **Extração Concluída (Com Reserva)**
**Ação:** Boot completo (Wi-Fi AP + PS4). Multímetro DC em OUT de cada INA240 (vs SGND). Serial: `INA240 offset`, logs `[Post-WiFi]` e `[Post-PS4]`. IDLE 30 s com motor desconectado e MCPWM desarmado.
**Extrato (Valores):** $V_{off,A}$ = 1670 mV | $V_{off,B}$ = 1480 mV | $V_{off,C}$ = 1510 mV. `[Post-WiFi]` adc_zero = 1660, bench_corr = +10 | `[Post-PS4]` adc_zero = 1640, bench_corr = +30. IDLE $I_A$: 1ª linha +0,01 A, típico +0,3 a 0,9 A, regime $\pm 0{,}1$ A, pico +1,07 A. $I_B/I_C \lesssim \pm 0{,}5$ A.
**Veredito:** Offset aprovado; zero em IDLE aprovado com reserva (~90% dentro de $\pm 0{,}5$ A em A; picos ~1 A). Deriva pós-RF no GPIO 34 (ver Setup 1a na tese).
**Datashard (Arquivo):** `14_ina240_offset.txt`

### ID 15: Ganho INA240

- [ ] **Pulado — limitação de bancada**
**Ação:** Previsto: validar $G = 20\,\text{V/V}$ na fase B ($I_{inj} \approx 1{,}0\,\text{A}$, $\Delta V_{out} \approx 20\,\text{mV}$, critério $|\varepsilon_G| \lesssim 5\,\%$).
**Veredito:** Cancelado. Sem resistor limitador $\geq 1\,\text{W}$ nem protoboard para injeção segura no shunt B (fonte única; estoque limitado a $1/4\,\text{W}$).
**Substituto:** ID 10 (dashboard $\times$ multímetro); OCP ID 02/18 (opcional).
**Datashard (Arquivo):** `15_ina240_ganho.txt` (registro do cancelamento)

### ID 16: Dead-time

- [ ] **Pendente**
**Ação:** PWM 50%, 20 kHz braço A. Tirar 2 fotos com o mesmo trigger (gate AH e gate AL) ou anotar o configurado.
**Extrato (Valores):** $t_{dead,AH \rightarrow AL}$ = ___ ns | $t_{dead,AL \rightarrow AH}$ = ___ ns
**Datashard (Arquivo):** `16_deadtime_AH.png`, `16_deadtime_AL.png`

### ID 17: $V_{GS}$ Bootstrap

- [ ] **Pendente**
**Ação:** Scope no gate HS e LS (50% duty). Verificar platô ~10–15 V (HS) e ~12 V (LS).
**Extrato (Valores):** $V_{GS,HS}$ = ___ V | $V_{GS,LS}$ = ___ V | Droop? [Sim / Não]
**Datashard (Arquivo):** `17_vgs_HS.png`, `17_vgs_LS.png`

### ID 02: Oscilograma OCP

- [ ] **Pendente**
**Ação:** DAC OCP ativo. Forçar OCP aterrando entrada (−) do LM339. 3 capturas (trigger em OC Trip descida): (1) INA240 ch A, (2) GPIO 26, (3) Gate AH.
**Extrato (Valores):** $t_{resposta} \approx$ ___ ns
**Datashard (Arquivo):** `02_ocp_ina240.png`, `02_ocp_octrip.png`, `02_ocp_gate.png`

### ID 18: Latência OCP

- [ ] **Pendente**
**Ação:** A partir das capturas do ID 02, medir tempos de propagação.
**Extrato (Valores):** $t_{OCP,total}$ = ___ ns | $t_{LM339}$ = ___ ns | $t_{IR2110,SD}$ = ___ ns
**Datashard (Arquivo):** `18_ocp_latencia.txt`

### ID 19: Ripple Vbus (AC)

- [ ] **Pendente**
**Ação:** PWM vazio 20 kHz. Scope no barramento, AC Coupling, 20 mV/div.
**Extrato (Valores):** $V_{pp,ripple}$ = ___ mV
**Datashard (Arquivo):** `19_ripple_vbus.png`

### ID 20: Ripple Teórico e ESR

- [ ] **Pendente**
**Ação:** Calcular ESR baseada no ID 19 e na fórmula $\Delta V_C$.
**Extrato (Valores):** $\Delta V_{C,teo}$ = ___ mV | $ESR_{ef}$ = ___ m$\Omega$
**Datashard (Arquivo):** `20_ripple_esr.txt`

### ID 30: Memória PlatformIO

- [ ] **Pendente**
**Ação:** Rodar `pio run` e copiar o bloco RAM/Flash.
**Extrato (Valores):** `.text` = ___ B | `.rodata` = ___ B | `.data` = ___ B | `.bss` = ___ B | RAM = ___ %
**Datashard (Arquivo):** `30_pio_memory.txt`

### ID 29: Heap Livre (Parte 1 — IDLE)

- [ ] **Pendente**
**Ação:** Ler heap via JSON com motor parado (BT+AP).
**Extrato (Valores):** $H_{livre,IDLE}$ = ___ bytes
**Datashard (Arquivo):** Anotar para salvar junto com a parte 2 em `29_heap.txt`

---

## 🌪️ NÓ B: Rampa + Wi-Fi

Motor conectado. Testes de aceleração e estabilidade.

**Build de teste Wi-Fi (defer):** `BOARD_ENABLE_WIFI_TELEMETRY=1`, `WIFI_TELEMETRY_DEFER_IN_RUNNING=1` + `pio run -t uploadfs`. **Build de bancada padrão:** Wi-Fi off (`BOARD_ENABLE_WIFI_TELEMETRY=0`).

### ID 21: Corrente de Alinhamento

- [ ] **Pendente**
**Ação:** Pinça DC na Fase A; pressionar R2; ler média durante ALIGN.
**Extrato (Valores):** $I_{align}$ = ___ A | Desvio teórico = ___ %
**Datashard (Arquivo):** `21_i_align.txt`

### ID 01: Partida FSM (Oscilograma)

- [ ] **Pendente**
**Ação:** Pinça na Fase A ou tensão Fase–GND. Gravar 30–60 s com R2. Marcar ALIGN, RUN_OPEN e RUN_SPEED.
**Extrato (Valores):** $t_{rampa,med}$ = ___ s
**Datashard (Arquivo):** `01_partida_faseA_scope.png`, `04_rampa_rpm_im.csv`

### ID 04: Gráfico Rampa RPM + Corrente

- [ ] **Pendente**
**Ação:** Exportar CSV da Dash durante o ensaio anterior (R2 completo).
**Extrato (Valores):** Regiões demarcadas com sucesso.
**Datashard (Arquivo):** `04_rampa_rpm_im.png`

### ID 05: Duração Rampa 5 Hz → 300 Hz

- [ ] **Pendente**
**Ação:** Usar o CSV do ID 04 para subtrair $t_2$ (300 Hz) e $t_1$ (5 Hz).
**Extrato (Valores):** $T_{rampa,med}$ = ___ s | $T_{rampa,teo}$ = ___ s | Desvio = ___ %
**Datashard (Arquivo):** `05_rampa_tempo.txt`

### ID 22: Correntes Trifásicas (Partida)

- [ ] **Pendente**
**Ação:** Extrair dados do CSV. Verificar simetria.
**Extrato (Valores):** ALIGN = ___ ms | $f_{el}$ transição = ___ Hz | Simetria: [Sim / Não]
**Datashard (Arquivo):** `22_partida_correntes.png`

### ID 09: Screenshot Dashboard

- [ ] **Pendente**
**Ação:** Com defer ativo: motor RUNNING ≥30 s, soltar R2. Verificar banner *Última corrida: N amostras* e gráficos preenchidos pelo batch. Print Screen em 192.168.4.1.
**Datashard (Arquivo):** `09_dashboard_screenshot.png`

### ID 10: Validação Corrente Dash vs Multímetro

- [ ] **Pendente**
**Ação:** Multímetro em série. Em regime estável, ler `im` via `GET /data` em IDLE ou amostra de `GET /data/batch` após corrida (modo defer). Comparar com multímetro e UART.
**Extrato (1500 RPM):** Multímetro = ___ A | Dash = ___ A | $\varepsilon_I$ = ___ %
**Extrato (2000 RPM):** Multímetro = ___ A | Dash = ___ A | $\varepsilon_I$ = ___ %
**Datashard (Arquivo):** `10_validacao_corrente.txt`

### ID 07: Coerência RPM Dashboard ↔ Serial

- [ ] **Pendente**
**Ação:** Comparar `rpm` no JSON vs UART simultaneamente.
**Extrato (Valores):** $\varepsilon$ (1500 RPM) = ___ % | $\varepsilon$ (2000 RPM) = ___ %
**Datashard (Arquivo):** `07_coerencia_rpm.txt`

### ID 11 & 12: Estabilidade Contínua Wi-Fi (modo defer)

- [ ] **Pendente**
**Ação:** Build defer (`BOARD_ENABLE_WIFI_TELEMETRY=1`). Motor RUNNING ≥30 s com label *Gravando corrida…* e `buf_n` crescente. Soltar R2; confirmar `/data/batch` com `ready:true` e `n>0`. Exportar CSV em IDLE. Anotar BT estável? [S/N]
**Extrato (Valores):** $T_{RUNNING}$ = ___ s | `buf_n` máx = ___ | `n` batch = ___ | Quedas BT/HTTP = ___ | Replay gráficos OK? [S/N]
**Datashard (Arquivo):** `11_csv_estabilidade.png`, `11_ensaio_estabilidade.csv`, `12_estabilidade_notas.txt`

*Ensaio legado (JSON denso live em RUNNING ≥6 min): substituído — instável; ver Cap. 4 regressões.*

---

## ⚡ NÓ C: Dinâmica

Testes de resposta, limites e stress mecânico.

### ID 23: Determinismo PWM

- [ ] **Pendente**
**Ação:** Motor em regime. Scope em AH, persistência/histograma $\geq$ 1 min. Apertar botões aleatórios no PS4.
**Extrato (Valores):** $T_{min}$ = ___ $\mu$s | $T_{max}$ = ___ $\mu$s | $\sigma_T$ = ___ ns
**Datashard (Arquivo):** `23_pwm_histograma.png`, `23_pwm_periodo.txt`

### ID 24: Degrau PI Corrente

- [ ] **Pendente**
**Ação:** Modo Corrente (`SPEED_MODE = 0`). R2 degrau rápido sobe/desce. Log serial.
**Extrato (Valores):** $K_p$ = ___ | $K_i$ = ___ | $t_s$ = ___ s | $M_p$ = ___ % | $e_{ss}$ = ___ A
**Datashard (Arquivo):** `24_degrau_corrente.png`, `24_degrau_corrente.csv`

### ID 06: Degrau PI Velocidade

- [ ] **Pendente**
**Ação:** Modo Speed (`SPEED_MODE = 1`). Degrau no R2. Medir tempo de acomodação.
**Extrato (Valores):** $K_p^\omega$ = ___ | $K_i^\omega$ = ___ | $t_s$ = ___ s | $M_p$ = ___ %
**Datashard (Arquivo):** `06_degrau_rpm.png`, `notas_pi.txt`

### ID 25: Frequência Mínima (Malha Aberta)

- [ ] **Pendente**
**Ação:** Reduzir R2 lentamente até vibrar/travar.
**Extrato (Valores):** $f_{el,min}$ = ___ Hz ($\approx$ ___ RPM) | Sintoma: ________________
**Datashard (Arquivo):** `25_fel_min.txt`

### ID 26: Stall Mecânico (Boss Fight!)

- [ ] **Pendente**
**Ação:** $I_{ref} \approx 1{,}5$ A. Travar eixo com segurança. Log serial (duty vai a 95%, FAULT). Dar re-arm.
**Extrato (Valores):** $t_{stall,total}$ = ___ ms | $t_{PI,sat}$ = ___ ms | $t_{det}$ = ___ ms | Re-arm limpo? [S / N]
**Datashard (Arquivo):** `26_stall_evento.png`, `26_stall_evento.csv`

---

## 📡 NÓ D: UVLO, BEMF e Métricas de Software

Ensaios de proteção, sinal e processamento.

### ID 03: UVLO Bateria 4S

- [ ] **Pendente**
**Ação:** Baixar tensão da fonte até dar FAULT. Subir até recuperar (debounce). Log serial `v` e `fault`.
**Extrato (Valores):** $V_{trip}$ = ___ V | $V_{rec}$ = ___ V | $t_{debounce} \approx$ ___ ms
**Datashard (Arquivo):** `03_uvlo_vbus.csv`, `03_uvlo_vbus.png`

### ID 08: BEMF Fase Flutuante

- [ ] **Pendente**
**Ação:** Motor no teto em malha aberta (~300 Hz). Scope 1 ch na 3ª fase vs SGND. Escala 5–20 ms/div.
**Extrato (Valores):** Capturar cruzamento por zero e forma trapezoidal.
**Datashard (Arquivo):** `08_bemf_fase_flutuante.png`

### ID 27: Latência Cenário A

- [ ] **Pendente**
**Ação:** BT + Serial (sem Wi-Fi cliente). Motor RUNNING por 30 s. Log `lat`.
**Extrato (Valores):** $t_{tick,min}$ = ___ $\mu$s | $t_{tick,max}$ = ___ $\mu$s | $\bar{t}_{tick}$ = ___ $\mu$s | $\rho_{CPU}$ = ___ %
**Datashard (Arquivo):** `27_ttick_cenario_A.csv`

### ID 28: Latência Cenário B

- [ ] **Pendente**
**Ação:** BT + Wi-Fi AP + defer (`WIFI_TELEMETRY_DEFER_IN_RUNNING=1`) + Dashboard polling 1 Hz. Motor RUNNING 30 s. Log `lat` e `lmax` via serial (500 ms).
**Extrato (Valores):** $t_{tick,min,max,\bar{t}}$ = ___ / ___ / ___ $\mu$s | `lmax` = ___ $\mu$s
**Datashard (Arquivo):** `28_ttick_cenario_B.csv`

### ID 29: Heap Livre (Parte 2 — RUNNING)

- [ ] **Pendente**
**Ação:** Ler `heap` via JSON completo em IDLE (não durante `buffering:true`). Motor parado vs após corrida.
**Extrato (Valores):** $H_{livre,RUNNING}$ = ___ bytes | $\Delta H$ = ___ | $\eta_{heap}$ = ___ %
**Datashard (Arquivo):** `29_heap.txt`

### ID 31: Síntese Impacto Wi-Fi

- [ ] **Pendente**
**Ação:** Compilar IDs 12, 27, 28 e 29. Comparar Cenário A vs B (defer). Documentar se defer restaurou estabilidade BT — **não assumir sucesso sem dados**.
**Extrato (Valores):** $\Delta t_{tick,max}$ = ___ $\mu$s | $\bar{t}_{tick,B}$ = ___ $\mu$s | Quedas BT = ___ | Batch OK? [S/N]
**Datashard (Arquivo):** `31_sintese_wifi.txt`

---

## 🏆 NÓ E: Side Quest (Opcional)

### ID 13: Boot Antes/Depois

- [ ] **Pendente**
**Ação:** Medir pico de corrente do firmware antigo vs repouso corrigido *(se tiver o binário antigo)*.
**Extrato (Valores):** $I_{pico,antigo}$ = ___ A ou [N/A]
**Datashard (Arquivo):** `13_boot_corrente.png`

---

💡 **Dica Notion:** Selecione os títulos `## NÓ A` … `## NÓ E` e converta em **Toggle Heading 2** para expandir só a fase ativa durante o ensaio.
