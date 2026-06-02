#ifndef BEMF_ZCD_H
#define BEMF_ZCD_H

#include "ina240_current_sensors.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool bemf_zcd_init(void);
bool bemf_zcd_is_ready(void);

/** Fase flutuante esperada para o passo de comutação 0…5. */
ina240_phase_t bemf_zcd_floating_phase_for_step(uint8_t comm_step);

/**
 * Consome um flanco EXTI na fase esperada (desde a última leitura).
 * Retorna false se não houve evento ou a fase não coincide.
 */
bool bemf_zcd_consume_edge(ina240_phase_t expected_phase);

/** Nível atual do comparador da fase (true = saída ativa / nível baixo). */
bool bemf_zcd_phase_asserted(ina240_phase_t phase);

#ifdef __cplusplus
}
#endif

#endif // BEMF_ZCD_H
