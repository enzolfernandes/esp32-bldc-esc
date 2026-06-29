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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa o Access Point Wi-Fi, monta o LittleFS e sobe o servidor HTTP/WebSocket.
 * @return true se AP e servidor HTTP subiram; false apenas se softAP falhar.
 */
bool wifi_telemetry_init(void);

/**
 * @brief Envia um snapshot JSON para todos os clientes WebSocket conectados.
 * @param json  String JSON terminada em '\0', tipicamente < 300 bytes.
 *
 * Deve ser chamado no loop() (contexto Arduino), nunca em ISR ou motor_control_tick.
 */
void wifi_telemetry_push(const char *json);

/**
 * @brief Estações conectadas ao softAP (0 = nenhum browser ativo).
 */
int wifi_telemetry_client_count(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_TELEMETRY_H
