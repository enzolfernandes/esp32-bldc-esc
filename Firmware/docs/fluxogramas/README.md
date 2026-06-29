# Fluxogramas Mermaid — firmware ESC BLDC

Fontes em Mermaid (`.mmd`) para fluxogramas do firmware, alinhados a [`DOCUMENTACAO_PROGRAMACAO.md`](../../DOCUMENTACAO_PROGRAMACAO.md) Seção 5.

## Índice de arquivos

| Arquivo | Descrição | Uso na tese |
|---------|-----------|-------------|
| [`fluxograma1_documentacao_fluxo_Completo.mmd`](fluxograma1_documentacao_fluxo_Completo.mmd) | Mapa completo: Fluxos A, B e C + acoplamentos ①–⑨ | Apêndice ou figura principal (A2/A3 paisagem) |
| [`fluxograma1_documentacao_fluxo_A.mmd`](fluxograma1_documentacao_fluxo_A.mmd) | Fluxo A isolado — `loop()` ~20 ms, FSM, PS4 | Cap. implementação |
| [`fluxograma1_documentacao_fluxo_B.mmd`](fluxograma1_documentacao_fluxo_B.mmd) | Fluxo B isolado — `esp_timer` 1 kHz, PI, 6-step | Cap. controle |
| [`fluxograma1_documentacao_fluxo_C.mmd`](fluxograma1_documentacao_fluxo_C.mmd) | Fluxo C isolado — ISR OC Trip (µs) | Cap. proteções |
| [`fluxograma2_processo.mmd`](fluxograma2_processo.mmd) | Visão operacional resumida (1× A4) | Metodologia / visão geral |
| [`fluxograma1_legenda.md`](fluxograma1_legenda.md) | Legenda, tabela ①–⑨, `\caption` LaTeX | Texto da figura (não exportar) |

## Convenções

- **INÍCIO** no topo; loops com setas de retorno explícitas (sem conectores ○A/○B).
- **Cores dos painéis** (LED PS4): azul = Fluxo A / IDLE, verde = Fluxo B / RUNNING, vermelho = Fluxo C / FAULT.
- **Setas sólidas** = sequência interna; **tracejadas numeradas** = acoplamento entre fluxos (somente no mapa completo).
- **ZCD:** padrão `BOARD_ENABLE_BEMF_ZCD=0` (malha aberta).

## Exportação

### Mermaid Live (rápido)

1. [https://mermaid.live](https://mermaid.live) → colar conteúdo do `.mmd`
2. **Actions → Export SVG** (recomendado para LaTeX)

### mermaid-cli

```bash
cd Firmware/docs/fluxogramas
npx @mermaid-js/mermaid-cli -i fluxograma1_documentacao_fluxo_Completo.mmd -o fluxograma1_documentacao_fluxo_Completo.svg -b transparent
npx @mermaid-js/mermaid-cli -i fluxograma1_documentacao_fluxo_A.mmd -o fluxograma1_documentacao_fluxo_A.svg -b transparent
npx @mermaid-js/mermaid-cli -i fluxograma1_documentacao_fluxo_B.mmd -o fluxograma1_documentacao_fluxo_B.svg -b transparent
npx @mermaid-js/mermaid-cli -i fluxograma1_documentacao_fluxo_C.mmd -o fluxograma1_documentacao_fluxo_C.svg -b transparent
npx @mermaid-js/mermaid-cli -i fluxograma2_processo.mmd -o fluxograma2_processo.svg -b transparent
```

Copiar SVGs exportados para `Docs/Thesis/imagens/` antes de `\includesvg` no LaTeX.

**Após editar `.mmd`:** reexportar SVGs e substituir os arquivos em `Docs/Thesis/imagens/` — os SVGs não são atualizados automaticamente pelo repositório.

Detalhes, tabela de acoplamentos e texto de `\caption` pronto: [`fluxograma1_legenda.md`](fluxograma1_legenda.md).

## Manutenção

Ao alterar `initVariant`, sequência de boot INA240 (`esc_boot_sensors`, recals pós-Wi-Fi/PS4), `setup`/`loop`, FSM, `motor_control_tick` ou ISR de OC, atualizar os `.mmd` correspondentes e a [Seção 7.3](../../DOCUMENTACAO_PROGRAMACAO.md#73-fluxogramas-mermaid) da documentação.
