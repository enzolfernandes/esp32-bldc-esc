/*
 * wifi_telemetry.h — Dashboard Wi-Fi em tempo real para o ESC BLDC.
 *
 * Camada: aplicação. Inicializado em setup(); push chamado no loop() a cada 100 ms.
 *
 * O ESP32 opera em modo Access Point (AP):
 *   SSID: WIFI_AP_SSID  (board_config.h)
 *   Pass: WIFI_AP_PASSWORD
 *   URL:  http://192.168.4.1
 *
 * Coexistência BT Classic (Bluepad32/DualShock 4) + Wi-Fi é gerenciada pelo
 * ESP-IDF via time-sharing de rádio. ADC1 (pinos 34/35/36/39) não é afetado.
 */

#ifndef WIFI_TELEMETRY_H
#define WIFI_TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o Access Point Wi-Fi, monta o LittleFS e sobe o servidor HTTP.
 * @return true se AP e servidor HTTP subiram; false apenas se softAP falhar.
 */
bool wifi_telemetry_init(void);

/**
 * @brief Atualiza o snapshot JSON servido em GET /data.
 * @param json  String JSON terminada em '\0', tipicamente < 400 bytes.
 *
 * Deve ser chamado no loop() (contexto Arduino), nunca em ISR ou motor_control_tick.
 */
void wifi_telemetry_push(const char *json);

/**
 * @brief Estações conectadas ao softAP (0 = nenhum browser ativo).
 */
int wifi_telemetry_client_count(void);

#if WIFI_TELEMETRY_DEFER_IN_RUNNING

/** Amostra compacta gravada em RUNNING (~12 B). */
typedef struct __attribute__((packed)) {
    uint32_t t_ms;
    uint16_t rpm;
    int16_t  im_cA;
    uint8_t  duty;
    uint8_t  fel;
} wifi_telem_run_sample_t;

void wifi_telemetry_record_running_sample(uint32_t t_ms, float rpm, float im,
                                          float duty, float fel);
void wifi_telemetry_finalize_running_batch(void);
void wifi_telemetry_clear_running_batch(void);
bool wifi_telemetry_running_batch_ready(void);
uint16_t wifi_telemetry_running_buf_count(void);

/** Snapshot leve em RUNNING (≤ 1 Hz) — mantém dashboard vivo sem JSON denso. */
void wifi_telemetry_push_light_running_status(uint32_t t_ms, uint16_t buf_n,
                                              bool ps4_connected, uint8_t r2_pct);

#endif /* WIFI_TELEMETRY_DEFER_IN_RUNNING */

#ifdef __cplusplus
}
#endif

#endif // WIFI_TELEMETRY_H
