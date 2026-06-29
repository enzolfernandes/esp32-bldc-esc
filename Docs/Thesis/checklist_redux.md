# ⚡ GIG REDUX: Bancada ESC em 1 Dia (Cap. 4)

> **Contrato:** 6 evidências críticas + 1 opcional. Escopo alinhado ao capítulo 4 já parcialmente preenchido (Setups 0–1a, regressões Wi-Fi, `t_tick`, heap).
> **Referência completa (31 IDs):** [`checklist_gamificado.md`](checklist_gamificado.md)
> **Drop Point:** `Docs/Thesis/ensaio_bancada/AAAA-MM-DD/` *(crie no início do dia)*
> **Orçamento:** ~4 h bancada + ~2 h LaTeX/figuras *(build Wi-Fi off por padrão)*

### 🦾 Loadout

| Item | Valor |
|------|--------|
| Motor | A2212/10T 1400 kV |
| Fonte | 12 V, limite **3 A** |
| Controle | PS4 pareado (Bluetooth) |
| Scope | **1 canal** |
| Notebook | Serial USB 115200 *(Wi-Fi opcional só no extra)* |

### 🔧 Dois builds (não misturar no mesmo ensaio)

| Perfil | `BOARD_ENABLE_WIFI_TELEMETRY` | Quando usar |
|--------|-------------------------------|-------------|
| **Padrão (hoje)** | `0` | Todos os ensaios nucleares — PS4 estável |
| **Extra Wi-Fi** | `1` + `pio run -t upload` + `uploadfs` | Só se sobrar ≥45 min no fim do dia |

Default em `board_config.h`: Wi-Fi **off**. Telemetria serial a **500 ms** em `RUNNING`.

---

## ✅ Já feito — não repetir

| ID | Status | Arquivo |
|----|--------|---------|
| **14** | Offset INA240 aprovado (reserva canal A) | `14_ina240_offset.txt` |
| **15** | Ganho cancelado (limitação bancada) | `15_ina240_ganho.txt` |
| **27 / 29** | `t_tick` e heap **já no** `4_resultados_discussao.tex` (Wi-Fi off) | — |
| **Wi-Fi legado** | Regressão BT+AP documentada no cap. 4 | — |

---

## 🎯 Roteiro do dia (ordem fixa)

```
08:00  ID 30 — pio run (notebook, motor desligado)
08:15  Setup — motor, fonte 3 A, PS4, pasta do dia, serial gravando
08:30  ID 01 + 04 — partida (scope fase A + log serial)
09:15  ID 06 OU 08 — degrau RPM *ou* BEMF (escolha 1)
09:45  ID 26 — stall mecânico
10:15  ID 02 + 18 — OCP (motor desconectado / fonte limitada)
11:00  LaTeX — figuras, lacunas, compilar PDF
12:00  [OPCIONAL] ID 09 — screenshot defer (reflash Wi-Fi)
```

---

## 🔴 NÚCLEO (obrigatório)

### ID 30 — Memória PlatformIO

- [ ] **Pendente**
- **Ação:** Na pasta `Firmware/`: `pio run`. Copiar bloco RAM/Flash do final do build.
- **Datashard:** `30_pio_memory.txt`
- **LaTeX:** Sub-teste 5.2 (única lacuna numérica fácil sem hardware).

---

### ID 01 + 04 — Partida FSM + rampa RPM/corrente

- [ ] **Pendente**
- **Build:** Wi-Fi **off**.
- **Ação:**
  1. Scope: tensão fase A vs SGND *(ou pinça na fase A)*.
  2. Gravar **30–60 s** com R2 (partida completa ALIGN → RUN_OPEN → RUN_SPEED).
  3. Em paralelo: gravar **serial** (PuTTY / monitor PlatformIO) na mesma rodada.
  4. Plotar RPM × tempo e `im` a partir do log serial → figura `04_rampa_rpm_im.png`.
- **Marcar no scope:** ALIGN, transição para RUN_OPEN, regime em RUN_SPEED.
- **Datashard:** `01_partida_faseA_scope.png`, `04_rampa_rpm_im.png`, `04_rampa_rpm_im.csv` *(se exportar do log)*
- **LaTeX:** Lacunas de partida (FSM, sub-teste 4.1, `fig:dashboard_csv_ramp` pode usar o gráfico serial).

**Dica:** Uma figura de rampa serial substitui CSV da dashboard neste escopo.

---

### ID 02 + 18 — OCP hardware (LM339)

- [ ] **Pendente**
- **Pré-requisito:** Motor desconectado; fonte **100 mA** ou desligada na ponte se possível.
- **Ação:**
  1. Forçar trip: aterrar entrada **(−)** do LM339 (via resistor limitador).
  2. Scope **1 canal**, trigger na **descida** de OC Trip (GPIO 26) ou no gate AH.
  3. **Mínimo:** 1 oscilograma + medir `t_OCP,total` (ns ou µs) com cursores.
  4. Decomposição LM339/IR2110 é **opcional** — só `t_OCP,total` basta.
- **Datashard:** `02_ocp_gate.png` *(ou `02_ocp_octrip.png`)*, `18_ocp_latencia.txt`
- **LaTeX:** Proteção OCP (sec. firmware) + Setup 3.

**Frase se medição grossa:** *"Ordem de grandeza compatível com cadeia LM339 + IR2110 em µs; margem frente a \(di/dt\) do ensaio."*

---

### ID 26 — Stall mecânico → FAULT

- [ ] **Pendente**
- **Ação:**
  1. Motor em regime (~R2 médio, modo SPEED).
  2. Travar eixo com segurança (pinça, dedo com cuidado, carga mecânica).
  3. Confirmar: duty → ~95 %, serial `falha=STALL` ou `MOTOR_FAULT_STALL`, lightbar **vermelha**.
  4. Options → clear fault → re-arm limpo (sem segundo trip imediato).
- **Datashard:** `26_stall_evento.png` *(print serial ou gráfico se tiver tempo)*, `26_stall_evento.csv` *(opcional)*
- **LaTeX:** Sub-teste 4.5 + critérios de stall na sec. proteções.

**Mínimo aceitável:** 1 captura de tela da serial + foto da lightbar vermelha.

---

## 🟡 ESCOLHA UM (se o relógio apertar, pule os dois — não são bloqueantes)

### ID 06 — Degrau PI velocidade *(malha fechada)*

- [ ] **Pendente**
- **Ação:** Modo SPEED (`MOTOR_CONTROL_USE_SPEED_MODE=1`). Degrau brusco no R2 (subida forte). Gravar serial; plotar RPM × tempo.
- **Datashard:** `06_degrau_rpm.png`, `notas_pi.txt` *(Kp/Ki do `board_config.h` — sem retuning)*
- **Prioridade:** Faça **06** se a rampa (04) já mostrar regime estável e faltar prova de PI.

### ID 08 — BEMF fase flutuante *(sensorless futuro)*

- [ ] **Pendente**
- **Ação:** Motor no teto (~300 Hz elétricos / ~2571 RPM estimados). Scope 1 ch na **3ª fase Hi-Z** vs SGND, 5–20 ms/div. Capturar forma trapezoidal + cruzamento por zero.
- **Datashard:** `08_bemf_fase_flutuante.png`
- **Prioridade:** Faça **08** se a banca for forte em sensorless. **Não** vender como ZCD implementado (`BOARD_ENABLE_BEMF_ZCD=0`).

---

## 🟢 EXTRA (só se sobrar ≥45 min + reflash)

### ID 09 — Screenshot dashboard (modo defer)

- [ ] **Pendente**
- **Build:** `BOARD_ENABLE_WIFI_TELEMETRY=1`, `WIFI_TELEMETRY_DEFER_IN_RUNNING=1`, `pio run -t upload` + `uploadfs`.
- **Ação:** RUNNING ≥30 s → label *Gravando corrida…* → soltar R2 → banner *Última corrida: N amostras* → Print 192.168.4.1.
- **Datashard:** `09_dashboard_screenshot.png`
- **Se falhar (BT cai):** Anotar em `31_sintese_wifi.txt`: *"Defer não validado; regressão legada documentada no cap. 4."* — **não é reprovação.**

---

## 🚫 FORA DO ESCOPO REDUX (não executar hoje)

| IDs | Motivo oficial *(copiar no `.tex` se houver lacuna)* |
|-----|------------------------------------------------------|
| 03 | UVLO: ensaio com fonte regulada 12 V; validação com pack 4S reservada. |
| 05 | Tempo de rampa teórico vs medido — extrair do CSV 04 *se sobrar tempo no PC*. |
| 07, 10, 11, 12, 28, 31 | Wi-Fi: regressão e defer documentados; validação operacional pendente. |
| 16, 17 | Dead-time/Vgs: \(t_{dead}=500\,\text{ns}\) configurado no MCPWM; medição scope 1 ch imprecisa. |
| 19, 20 | Ripple/ESR: validado por simulação LTSpice. |
| 21, 22 | Corrente ALIGN / trifásica: coberto qualitativamente pelo ID 01. |
| 23 | PWM determinismo: MCPWM em hardware + \(t_{tick}\) já documentado. |
| 24 | Degrau corrente: modo CURRENT não é foco (SPEED padrão). |
| 25 | \(f_{el,min}\): motivação do ZCD no texto; ensaio reservado. |
| 13 | Boot antes/depois: Setup 0 já documenta correção. |

**Resposta padrão na banca:**

> *"Parâmetros secundários foram validados qualitativamente ou por simulação; o escopo experimental de um dia concentrou-se em partida, OCP, stall e [BEMF ou degrau, se executado]."*

---

## 📋 Checklist de fechamento (LaTeX)

Antes de enviar o PDF:

- [ ] Toda `[LACUNA BANCADA]` restante → figura inserida **ou** parágrafo de limitação (tabela FORA acima).
- [ ] Mesma imagem pode fechar várias lacunas (ex.: `01` + sec. FSM; `02` + Setup 3).
- [ ] Não prometer dashboard estável em RUNNING sem ID 09 concluído.
- [ ] BEMF: enquadrar como evidência de Hi-Z/FCEM para **futuro** ZCD.
- [ ] Compilar `main.tex` e conferir figuras quebradas.

---

## 📁 Contrato de arquivos (pasta do dia)

```
ensaio_bancada/AAAA-MM-DD/
├── 30_pio_memory.txt          # obrigatório
├── 01_partida_faseA_scope.png  # obrigatório
├── 04_rampa_rpm_im.png         # obrigatório
├── 04_rampa_rpm_im.csv         # opcional (log serial)
├── 02_ocp_gate.png             # obrigatório (ou octrip)
├── 18_ocp_latencia.txt         # obrigatório (t_OCP,total)
├── 26_stall_evento.png         # obrigatório (mín.: print serial)
├── 06_degrau_rpm.png           # escolha 06 OU 08
├── 08_bemf_fase_flutuante.png  # escolha 06 OU 08
└── 09_dashboard_screenshot.png   # extra
```

---

## ⏱️ Orçamento de tempo

| Cenário | Duração |
|---------|---------|
| Núcleo (30+01+04+02+18+26) | **3–4 h** |
| + 06 ou 08 | **+30–40 min** |
| + LaTeX | **1–2 h** |
| + Extra Wi-Fi (09) | **+45–60 min** (reflash) |

---

💡 **Regra de ouro:** Se algo der errado às 14h, entregue o **núcleo de 5 itens** (sem 06/08/09) e feche lacunas no texto. Motor + OCP + stall = TCC de PoC defensável.
