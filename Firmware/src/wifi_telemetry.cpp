/*
 * wifi_telemetry.cpp — Dashboard Wi-Fi via HTTP polling para o ESC BLDC.
 *
 * Estratégia: HTTP polling (GET /data a cada 1 s pelo browser).
 * Sem WebSocket persistente → sem sobrecarga de ~19 KB de heap por conexão.
 * O browser requisita /data, o ESP32 responde com o JSON mais recente e fecha.
 *
 * Uso de memória por request: ~2 KB temporários, liberados imediatamente.
 * Comparado ao WebSocket: não há conexão persistente nem buffers alocados.
 *
 * Upload do filesystem: pio run -t uploadfs
 * Acesso:               http://192.168.4.1  (rede "ESC-Dashboard")
 */

#include "wifi_telemetry.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "board_config.h"

static AsyncWebServer s_server(WIFI_TELEMETRY_PORT);
static bool          s_initialized = false;

/* Buffer com o snapshot JSON mais recente.
 * Pré-inicializado com JSON válido mínimo para que a primeira requisição GET /data
 * retorne algo parsável mesmo antes de push_wifi_telemetry() ser chamado.
 * Atualizado pelo loop() via wifi_telemetry_push(). */
static char s_last_json[320] = "{\"state\":0,\"t\":0}";

/* --- API pública -------------------------------------------------------- */

bool wifi_telemetry_init(void)
{
    /* 1. Monta LittleFS (contém index.html gravado com 'pio run -t uploadfs') */
    if (!LittleFS.begin(false)) {
        Serial.println("[WiFi] ERRO: LittleFS nao montou. Execute 'pio run -t uploadfs'.");
        return false;
    }
    Serial.printf("[WiFi] LittleFS OK  %u KB livres\n",
                  LittleFS.totalBytes() / 1024U - LittleFS.usedBytes() / 1024U);

    /* 2. Sobe o Access Point */
    WiFi.mode(WIFI_AP);
    const bool ap_ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL);
    if (!ap_ok) {
        Serial.println("[WiFi] ERRO: softAP falhou.");
        return false;
    }
    Serial.printf("[WiFi] AP '%s'  IP=%s\n",
                  WIFI_AP_SSID,
                  WiFi.softAPIP().toString().c_str());

    /* 3. Endpoint de telemetria: GET /data → JSON atual
     * Cache desabilitado para garantir que o browser sempre busque dados frescos. */
    s_server.on("/data", HTTP_GET, [](AsyncWebServerRequest *req) {
        AsyncWebServerResponse *r =
            req->beginResponse(200, "application/json", s_last_json);
        r->addHeader("Cache-Control", "no-cache, no-store");
        r->addHeader("Access-Control-Allow-Origin", "*");
        req->send(r);
    });

    /* 4. Rota principal: serve index.html */
    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, "/index.html", "text/html", false);
    });

    /* 5. Serve qualquer outro arquivo estático do LittleFS */
    s_server.serveStatic("/", LittleFS, "/").setCacheControl("max-age=300");

    /* 6. Fallback 404 */
    s_server.onNotFound([](AsyncWebServerRequest *req) {
        req->send(404, "text/plain", "Not found");
    });

    s_server.begin();
    s_initialized = true;
    Serial.printf("[WiFi] HTTP server iniciado  porta=%u  GET http://%s/data\n",
                  WIFI_TELEMETRY_PORT,
                  WiFi.softAPIP().toString().c_str());
    return true;
}

void wifi_telemetry_push(const char *json)
{
    if (!s_initialized || json == nullptr) {
        return;
    }
    /* Atualiza o buffer — será entregue na próxima requisição GET /data */
    strncpy(s_last_json, json, sizeof(s_last_json) - 1);
    s_last_json[sizeof(s_last_json) - 1] = '\0';
}

int wifi_telemetry_client_count(void)
{
    /* Com polling HTTP não existe conexão persistente para contar.
     * Retorna 1 quando o servidor está disponível para manter o intervalo
     * de telemetria de 100 ms no loop() (melhor resolução no buffer JSON). */
    return s_initialized ? 1 : 0;
}
