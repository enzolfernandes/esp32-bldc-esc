# Legenda — Fluxograma 1 (documentação formal)

Use este arquivo como legenda da figura no LaTeX (`\caption` / nota de rodapé).  
O grafo Mermaid **não** inclui este bloco — evita poluir o layout.

---

## Arquivos

| Arquivo | Uso |
|---------|-----|
| `fluxograma1_documentacao_fluxo_Completo.mmd` | Mapa completo (A + B + C + acoplamentos ①–⑨) |
| `fluxograma1_documentacao_fluxo_A.mmd` | Figura separada — Fluxo A (Cap. 3) |
| `fluxograma1_documentacao_fluxo_B.mmd` | Figura separada — Fluxo B |
| `fluxograma1_documentacao_fluxo_C.mmd` | Figura separada — Fluxo C |
| `fluxograma2_processo.mmd` | Visão operacional resumida (1× A4) |

---

## Cores dos painéis

| Painel | Fundo | Borda | Significado |
|--------|-------|-------|-------------|
| Fluxo A | Azul claro | Azul | Aplicação ~20 ms · LED IDLE |
| Fluxo B | Verde claro | Verde | Controle 1 kHz · LED RUNNING |
| Fluxo C | Vermelho claro | Vermelho | ISR OC · LED FAULT |
| Envelope ESC | Cinza claro | Cinza | Sistema único · fluxos concorrentes |

---

## Símbolos

| Símbolo | Significado |
|---------|-------------|
| Oval | Início / retorno de ISR |
| Retângulo | Processo |
| Losango | Decisão |
| Paralelogramo implícito | Entrada/Saída (Serial HMI, PS4, telemetria) |
| Seta sólida | Sequência **dentro** do mesmo fluxo |
| Seta tracejada numerada | **Acoplamento** entre fluxos (flags, arm, setpoint) |

---

## Tabela de acoplamentos (fluxograma completo)

| ID | Origem | Destino | Mecanismo no firmware |
|----|--------|---------|------------------------|
| ① | Fluxo A · `P04` fsm_system_init | Fluxo B · disparo 1 ms | `motor_control_init()` em `fsm_system_init()` cria `esp_timer` 1 kHz |
| ② | Fluxo A · `P04` fsm_system_init | Fluxo C · OC Trip | `lm339_protection_arm()` em `fsm_system_init()` habilita EXTI |
| ③ | Fluxo A · `ARM` | Fluxo B · `motor_control ativo?` | `motor_control_on_arm()` → `s_active=true` |
| ④ | Fluxo A · `P24` desarme | Fluxo B · ticks inativos | `motor_control_on_disarm()` → tick retorna sem PWM |
| ⑤ | Fluxo A · `P26` setpoint | Fluxo B · PI corrente | `motor_control_set_target_*()` alimenta malha 1 kHz |
| ⑥ | Fluxo B · flag OC SW | Fluxo A · `P21` | `motor_control_consume_software_fault()` |
| ⑦ | Fluxo B · flag STALL | Fluxo A · `P21` | idem → `enter_fault_state()` |
| ⑧ | Fluxo C · flag HW OC | Fluxo A · `P21` | `s_fault_pending` na ISR → `fsm_system_tick` |
| ⑨ | Fluxo A · `P21` | Fluxo A · `F1` FAULT | Consumo de flags / UVLO / LM339 → `enter_fault_state()` |

---

## Nota calibração INA240 (rodapé da figura — Fluxo A)

**Perfil bancada (default):** `esc_radio_quiet_init()` desliga BT; cal INA240 em `fsm_system_init()` se necessário; offset manual (`INA240_USE_MANUAL_OFFSET=1`, 1670/1480/1510 mV).

**Perfil histórico PS4+Wi-Fi:** três etapas (`initVariant` early cal → recal pós-AP → recal pós-BT) — APIs no driver, não invocadas no `main.cpp` atual.

No Fluxo B, fase A (GPIO 34) usa **mediana 16×** + EMA α=0,05 em runtime. Ver [§4.3.4.1](../../DOCUMENTACAO_PROGRAMACAO.md#4341-ina240--canal-a-gpio-34).

---

## Nota ZCD (rodapé da figura)

Configuração padrão do projeto: `BOARD_ENABLE_BEMF_ZCD = 0` em `board_config.h`.  
Comutação em **malha aberta** (timer). Com `ZCD = 1`, após handover o Fluxo B usa ramo **6-step BEMF ZCD + 30° elétricos**.

---

## Legenda LaTeX (copiar para `\caption`)

```latex
Fluxograma formal do firmware ESC: três fluxos concorrentes do mesmo sistema
(Arduino \SI{20}{ms}, \esp_timer\ \SI{1}{kHz}, ISR OC em \si{\micro\second}).
Setas sólidas: sequência interna; tracejadas numeradas ①–⑨: acoplamentos (Tabela~\ref{tab:acoplamentos_esc}).
Cores: azul = Fluxo A, verde = Fluxo B, vermelho = Fluxo C.
Comutação padrão em malha aberta (\texttt{BOARD\_ENABLE\_BEMF\_ZCD=0}).
```

---

## Exportação SVG / PDF

### Mermaid Live

1. Abrir [https://mermaid.live](https://mermaid.live)
2. Colar o conteúdo do `.mmd`
3. **Actions → Export SVG** (preferível para LaTeX `\includesvg`)
4. Para PDF: abrir SVG no Inkscape ou Browser → Imprimir → PDF

### mermaid-cli (Node.js)

```bash
cd Firmware/docs/fluxogramas
npx @mermaid-js/mermaid-cli -i fluxograma1_documentacao_fluxo_Completo.mmd -o fluxograma1_documentacao_fluxo_Completo.svg -b transparent
npx @mermaid-js/mermaid-cli -i fluxograma1_documentacao_fluxo_A.mmd -o fluxograma1_documentacao_fluxo_A.svg -b transparent
npx @mermaid-js/mermaid-cli -i fluxograma1_documentacao_fluxo_B.mmd -o fluxograma1_documentacao_fluxo_B.svg -b transparent
npx @mermaid-js/mermaid-cli -i fluxograma1_documentacao_fluxo_C.mmd -o fluxograma1_documentacao_fluxo_C.svg -b transparent
npx @mermaid-js/mermaid-cli -i fluxograma2_processo.mmd -o fluxograma2_processo.svg -b transparent
```

### LaTeX (exemplo)

```latex
\begin{figure}[htbp]
  \centering
  \includesvg[width=\textwidth]{imagens/fluxograma1_documentacao_fluxo_Completo.svg}
  \caption{... texto da legenda acima ...}
  \label{fig:fluxo_firmware_completo}
\end{figure}
```

**Impressão:** mapa completo → A2/A3 paisagem; figuras A/B/C separadas → A4 retrato; Fluxograma 2 → A4 paisagem.

### Ajuste de tracejadas no Inkscape

Se as setas ①–⑨ ficarem finas ou sobrepostas no SVG exportado:

1. Selecionar arestas tracejadas
2. Aumentar `Stroke width` para 1,5–2 pt
3. Cor sugerida: `#555555`

---

## Sugestão de figuras no TCC

| Figura | Arquivo SVG | Capítulo |
|--------|-------------|----------|
| Visão operacional | `fluxograma2_processo.svg` | Metodologia / Resultados |
| Fluxo A detalhado | `fluxograma1_documentacao_fluxo_A.svg` | Implementação firmware |
| Fluxo B detalhado | `fluxograma1_documentacao_fluxo_B.svg` | Implementação firmware |
| Fluxo C detalhado | `fluxograma1_documentacao_fluxo_C.svg` | Segurança / proteções |
| Mapa completo (opcional) | `fluxograma1_documentacao_fluxo_Completo.svg` | Apêndice |
| Tabela acoplamentos | `\ref{tab:acoplamentos_esc}` | Junto à figura completa |
