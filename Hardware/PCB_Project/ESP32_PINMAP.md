# Mapa de pinos ESP32 ↔ ESC (DevKit V1 / DevKitC v4)

Referência para roteamento em `Control.SchDoc` (símbolo **U3** `DEVKIT_V1_ESP32-WROOM-32`) e PCB.

Fonte firmware: [`Firmware/include/board_config.h`](../../Firmware/include/board_config.h)  
Pinout físico: [`Docs/Thesis/imagens/esp32_devkitC_v4_pinlayout.png`](../../Docs/Thesis/imagens/esp32_devkitC_v4_pinlayout.png)

**Coexistência JTAG + ZCD:** JTAG usa GPIO **12–15** (livres de nets ESC). ZCD usa GPIO **16, 17, 5** (U3 **RX2, TX2, D5**).

> **Nota:** “Pino U3” = designador de pino no símbolo Altium (≠ número GPIO). Ex.: U3 pino **17** = **VP = GPIO36**, não GPIO17.

---

## Tabela mestra U3 ↔ GPIO ↔ função ESC / JTAG

| Pino U3 | Rótulo Altium | GPIO | `#define` / função | Net / destino |
|---------|---------------|------|-------------------|---------------|
| 30 | VIN | — | Alimentação | `Vcc 2 (+)` |
| 16 | EN | — | Reset | NC (botão EN DevKit) |
| 17 | VP | **36** | `PIN_ADC_IC` | `Isense C Out` → INA240 |
| 18 | VN | **39** | `PIN_ADC_VBAT` | `Vcc Supply Sense` |
| 4 | D2 | 2 | Livre | LED DevKit — **não usar ESC** |
| 5 | D4 | **4** | `PIN_SD_C` | `Shutdown C` → IR2110 SD |
| 8 | D5 | **5** | `PIN_ZCD_C` | `ZCD C` → LM339 BEMF (pull-up 10k) |
| 27 | D12 | **12** | **JTAG MTDI** | Header ESP-Prog — **sem ESC** |
| 28 | D13 | **13** | **JTAG MTCK** | Header ESP-Prog |
| 26 | D14 | **14** | **JTAG TMS** | Header ESP-Prog |
| 3 | D15 | **15** | **JTAG MTDO** | Header ESP-Prog |
| 9 | D18 | **18** | `PIN_PWM_CH` | `CH` → IR2110 |
| 10 | D19 | **19** | `PIN_PWM_CL` | `CL` → IR2110 |
| 11 | D21 | **21** | `PIN_PWM_AH` | `AH` → IR2110 |
| 1 | 3V3 | — | Alimentação | `Vmicro Ref` / 3,3 V |
| 12 | RX0 | 3 | Livre | UART0 programação — **não usar** |
| 13 | TX0 | 1 | Livre | UART0 programação — **não usar** |
| 6 | RX2 | **16** | `PIN_ZCD_A` | `ZCD A` → LM339 BEMF |
| 7 | TX2 | **17** | `PIN_ZCD_B` | `ZCD B` → LM339 BEMF |
| 14 | D22 | **22** | `PIN_PWM_AL` | `AL` → IR2110 |
| 15 | D23 | **23** | `PIN_PWM_BL` | `BL` → IR2110 |
| 23 | D25 | **25** | `PIN_VDAC_REF` | `Vdac Ref` → LM339 OCP (+) DAC1 |
| 24 | D26 | **26** | `PIN_OC_TRIP` | `OC Trip` ← LM339 wired-OR |
| 25 | D27 | **27** | `PIN_PWM_BH` | `BH` → IR2110 |
| 21 | D32 | **32** | `PIN_SD_A` | `Shutdown A` → IR2110 SD |
| 22 | D33 | **33** | `PIN_SD_B` | `Shutdown B` → IR2110 SD |
| 20 | D35 | **35** | `PIN_ADC_IB` | `Isense B Out` → INA240 |
| 19 | D34 | **34** | `PIN_ADC_IA` | `Isense A Out` → INA240 |
| 2, 29 | GND | — | Terra | `SGND` |

### Resumo por categoria

| Categoria | GPIO | Rótulos U3 |
|-----------|------|------------|
| PWM MCPWM | 21, 22, 27, 23, 18, 19 | D21, D22, D27, D23, D18, D19 |
| Shutdown IR2110 | 32, 33, 4 | D32, D33, D4 |
| ADC1 (Wi-Fi OK) | 34, 35, 36, 39 | D34, D35, VP, VN |
| OCP LM339 | 25 (DAC), 26 (in) | D25, D26 |
| ZCD BEMF (reserva) | 16, 17, 5 | RX2, TX2, D5 |
| JTAG / ICE | 12, 13, 14, 15 | D12, D13, D14, D15 |
| Livres | 1, 2, 3 | TX0, D2, RX0 |

---

## Bloco JTAG (ESP-Prog)

| Sinal JTAG | GPIO | Rótulo U3 | Pino U3 |
|------------|------|-----------|---------|
| TDI / MTDI | 12 | D12 | 27 |
| TCK / MTCK | 13 | D13 | 28 |
| TMS / MTMS | 14 | D14 | 26 |
| TDO / MTDO | 15 | D15 | 3 |
| GND | — | GND | 2 ou 29 |

Recomendação PCB: header dedicado (ex. 2×5); trilhas curtas; **nenhuma** net ESC nestes pinos.

---

## Bloco ZCD BEMF (LM339)

| Net | GPIO | Rótulo U3 | Pino U3 |
|-----|------|-----------|---------|
| ZCD A | 16 | RX2 | 6 |
| ZCD B | 17 | TX2 | 7 |
| ZCD C | 5 | D5 | 8 |

Regras de hardware:

- Saída LM339 **open-collector** + **pull-up 10 kΩ → 3,3 V** em cada fase.
- **GPIO5 (D5):** pino de strapping (VSPI CS) — pull-up externo obrigatório no comparador.
- Firmware: `BOARD_ENABLE_BEMF_ZCD 0` até hardware soldado; depois `1`.

---

## Não usar

GPIO **6–11** (flash SPI interna do módulo WROOM-32).

---

## Histórico de pinagem ZCD

| Versão | ZCD A/B/C | JTAG 12–15 |
|--------|-------------|------------|
| Inicial | 16 / 17 / 5 | Livre |
| Otimizado (intermediário) | 12 / 13 / 15 | Conflito |
| **Atual** | **16 / 17 / 5** | **Livre** |

---

## Ação Altium (`Control.SchDoc`)

1. Rotear **ZCD A/B/C** para **RX2, TX2, D5** (U3 pinos 6, 7, 8) — **não** D12/D13/D15.
2. Deixar **D12–D15** apenas para header **ESP-Prog** (No Connect no resto do ESC).
3. Confirmar demais nets conforme tabela mestra (BL em D23, Shutdown C em D4, etc.).
4. Re-exportar [`Docs/Thesis/imagens/esp32_schematic.png`](../../Docs/Thesis/imagens/esp32_schematic.png).

---

## Validação GPIO (sem overlap)

- **ESC ativo:** 4, 5, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39 (18 pinos)
- **JTAG exclusivo:** 12, 13, 14, 15
- **ZCD** compartilha 16, 17, 5 com ESC reserva — mesmos pinos, nets ZCD quando BEMF existir
