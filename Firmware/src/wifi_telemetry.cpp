/*
 * wifi_telemetry.cpp — Dashboard Wi-Fi via HTTP polling para o ESC BLDC.
 *
 * Estratégia: HTTP polling (GET /data a cada 1 s pelo browser).
 * Sem WebSocket persistente → sem sobrecarga de ~19 KB de heap por conexão.
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
static bool          s_littlefs_ok = false;

static char s_last_json[512] = "{\"state\":0,\"t\":0}";

bool wifi_telemetry_init(void)
{
    s_littlefs_ok = LittleFS.begin(false);
    if (!s_littlefs_ok) {
        Serial.println("[WiFi] AVISO: LittleFS nao montou. Execute 'pio run -t uploadfs'.");
        Serial.println("[WiFi] AP e GET /data serao iniciados mesmo assim.");
    } else {
        Serial.printf("[WiFi] LittleFS OK  %u KB livres\n",
                      LittleFS.totalBytes() / 1024U - LittleFS.usedBytes() / 1024U);
        if (!LittleFS.exists("/index.html")) {
            Serial.println("[WiFi] AVISO: /index.html ausente no LittleFS.");
        }
    }

    WiFi.mode(WIFI_AP);
    const bool ap_ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL);
    if (!ap_ok) {
        Serial.println("[WiFi] ERRO: softAP falhou.");
        return false;
    }
    Serial.printf("[WiFi] AP '%s'  IP=%s\n",
                  WIFI_AP_SSID,
                  WiFi.softAPIP().toString().c_str());

    s_server.on("/data", HTTP_GET, [](AsyncWebServerRequest *req) {
        // Sem printf/Serial aqui — callback roda em task async; competição com loop trava UART.

        AsyncWebServerResponse *r =
            req->beginResponse(200, "application/json", s_last_json);
        r->addHeader("Cache-Control", "no-cache, no-store");
        r->addHeader("Access-Control-Allow-Origin", "*");
        req->send(r);
    });

    if (s_littlefs_ok && LittleFS.exists("/index.html")) {
        s_server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
            req->send(LittleFS, "/index.html", "text/html", false);
        });
        s_server.serveStatic("/", LittleFS, "/").setCacheControl("no-cache");
    } else {
        s_server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
            req->send(200, "text/html",
                      "<!DOCTYPE html><html lang=\"pt-BR\"><head><meta charset=\"UTF-8\">"
                      "<title>ESC Dashboard</title></head><body>"
                      "<h1>ESC BLDC</h1>"
                      "<p>LittleFS ausente ou <code>index.html</code> nao encontrado.</p>"
                      "<p>Execute <code>pio run -t uploadfs</code> e reinicie o ESP32.</p>"
                      "<p>Telemetria JSON: <a href=\"/data\">/data</a></p>"
                      "</body></html>");
        });
    }

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
    strncpy(s_last_json, json, sizeof(s_last_json) - 1);
    s_last_json[sizeof(s_last_json) - 1] = '\0';
}

int wifi_telemetry_client_count(void)
{
    if (!s_initialized) {
        return 0;
    }

    return static_cast<int>(WiFi.softAPgetStationNum());
}
