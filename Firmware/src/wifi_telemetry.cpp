/*
 * wifi_telemetry.cpp — Dashboard Wi-Fi via HTTP polling para o ESC BLDC.
 *
 * Estratégia: HTTP polling (GET /data a cada 1 s pelo browser).
 * Sem WebSocket persistente → sem sobrecarga de ~19 KB de heap por conexão.
 *
 * Modo defer (WIFI_TELEMETRY_DEFER_IN_RUNNING=1): em RUNNING grava amostras
 * compactas em ring buffer; GET /data/batch expõe o lote após IDLE/FAULT.
 *
 * Upload do filesystem: pio run -t uploadfs
 * Acesso:               http://192.168.4.1  (rede "ESC-Dashboard")
 */

#include "wifi_telemetry.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

#include <stdio.h>
#include <string.h>

static AsyncWebServer s_server(WIFI_TELEMETRY_PORT);
static bool          s_initialized = false;
static bool          s_littlefs_ok = false;

static char s_last_json[512] = "{\"state\":0,\"t\":0}";

#if WIFI_TELEMETRY_DEFER_IN_RUNNING
static wifi_telem_run_sample_t s_run_buf[WIFI_TELEM_RUN_BUF_SAMPLES];
static uint16_t s_run_head   = 0;
static uint16_t s_run_count  = 0;
static bool     s_run_wrapped = false;
static uint32_t s_run_t0_ms  = 0;
static bool     s_batch_ready = false;

static char s_batch_json[4096];

static void build_batch_json(void)
{
    if (!s_batch_ready || s_run_count == 0) {
        snprintf(s_batch_json, sizeof(s_batch_json),
                 "{\"ready\":false,\"n\":0,\"wrapped\":false,\"dt_ms\":%u,"
                 "\"t0\":0,\"samples\":[]}",
                 static_cast<unsigned>(WIFI_TELEM_RUN_SAMPLE_MS));
        return;
    }

    char *p   = s_batch_json;
    size_t rem = sizeof(s_batch_json);

    const int hdr = snprintf(
        p, rem,
        "{\"ready\":true,\"n\":%u,\"wrapped\":%s,\"dt_ms\":%u,\"t0\":%lu,\"samples\":[",
        static_cast<unsigned>(s_run_count),
        s_run_wrapped ? "true" : "false",
        static_cast<unsigned>(WIFI_TELEM_RUN_SAMPLE_MS),
        static_cast<unsigned long>(s_run_t0_ms));
    if (hdr < 0 || static_cast<size_t>(hdr) >= rem) {
        snprintf(s_batch_json, sizeof(s_batch_json),
                 "{\"ready\":false,\"n\":0,\"error\":\"batch_overflow\"}");
        return;
    }
    p += hdr;
    rem -= static_cast<size_t>(hdr);

    const uint16_t start = s_run_wrapped ? s_run_head : 0;

    for (uint16_t i = 0; i < s_run_count; i++) {
        const uint16_t idx =
            static_cast<uint16_t>((start + i) % WIFI_TELEM_RUN_BUF_SAMPLES);
        const wifi_telem_run_sample_t *s = &s_run_buf[idx];
        const float im_a = static_cast<float>(s->im_cA) / 100.0f;

        const int n = snprintf(
            p, rem, "%s[%lu,%u,%.2f,%u,%u]",
            (i == 0) ? "" : ",",
            static_cast<unsigned long>(s->t_ms),
            static_cast<unsigned>(s->rpm),
            static_cast<double>(im_a),
            static_cast<unsigned>(s->duty),
            static_cast<unsigned>(s->fel));
        if (n < 0 || static_cast<size_t>(n) >= rem) {
            snprintf(s_batch_json, sizeof(s_batch_json),
                     "{\"ready\":false,\"n\":%u,\"error\":\"batch_overflow\"}",
                     static_cast<unsigned>(s_run_count));
            return;
        }
        p += n;
        rem -= static_cast<size_t>(n);
    }

    if (rem < 2) {
        snprintf(s_batch_json, sizeof(s_batch_json),
                 "{\"ready\":false,\"n\":%u,\"error\":\"batch_overflow\"}",
                 static_cast<unsigned>(s_run_count));
        return;
    }
    snprintf(p, rem, "]}");
}

void wifi_telemetry_record_running_sample(uint32_t t_ms, float rpm, float im,
                                          float duty, float fel)
{
    if (s_run_count == 0) {
        s_run_t0_ms = t_ms;
    }

    wifi_telem_run_sample_t *s = &s_run_buf[s_run_head];

    s->t_ms = t_ms;

    if (rpm < 0.0f) {
        rpm = 0.0f;
    } else if (rpm > 65535.0f) {
        rpm = 65535.0f;
    }
    s->rpm = static_cast<uint16_t>(rpm + 0.5f);

    int32_t cA = static_cast<int32_t>(im * 100.0f);
    if (cA > 32767) {
        cA = 32767;
    } else if (cA < -32768) {
        cA = -32768;
    }
    s->im_cA = static_cast<int16_t>(cA);

    if (duty < 0.0f) {
        duty = 0.0f;
    } else if (duty > 100.0f) {
        duty = 100.0f;
    }
    s->duty = static_cast<uint8_t>(duty + 0.5f);

    if (fel < 0.0f) {
        fel = 0.0f;
    } else if (fel > 255.0f) {
        fel = 255.0f;
    }
    s->fel = static_cast<uint8_t>(fel + 0.5f);

    s_run_head = static_cast<uint16_t>((s_run_head + 1U) % WIFI_TELEM_RUN_BUF_SAMPLES);
    if (s_run_count < WIFI_TELEM_RUN_BUF_SAMPLES) {
        s_run_count++;
    } else {
        s_run_wrapped = true;
    }
    s_batch_ready = false;
}

void wifi_telemetry_finalize_running_batch(void)
{
    if (s_run_count > 0) {
        s_batch_ready = true;
        build_batch_json();
    }
}

void wifi_telemetry_clear_running_batch(void)
{
    s_run_head    = 0;
    s_run_count   = 0;
    s_run_wrapped = false;
    s_run_t0_ms   = 0;
    s_batch_ready = false;
    s_batch_json[0] = '\0';
}

bool wifi_telemetry_running_batch_ready(void)
{
    return s_batch_ready;
}

uint16_t wifi_telemetry_running_buf_count(void)
{
    return s_run_count;
}

void wifi_telemetry_push_light_running_status(uint32_t t_ms, uint16_t buf_n,
                                              bool ps4_connected, uint8_t r2_pct)
{
    char json[128];
    snprintf(json, sizeof(json),
             "{\"state\":2,\"buffering\":true,\"buf_n\":%u,\"t\":%lu,"
             "\"ps4c\":%s,\"r2\":%u}",
             static_cast<unsigned>(buf_n),
             static_cast<unsigned long>(t_ms),
             ps4_connected ? "true" : "false",
             static_cast<unsigned>(r2_pct));
    wifi_telemetry_push(json);
}
#endif /* WIFI_TELEMETRY_DEFER_IN_RUNNING */

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
        AsyncWebServerResponse *r =
            req->beginResponse(200, "application/json", s_last_json);
        r->addHeader("Cache-Control", "no-cache, no-store");
        r->addHeader("Access-Control-Allow-Origin", "*");
        req->send(r);
    });

#if WIFI_TELEMETRY_DEFER_IN_RUNNING
    s_server.on("/data/batch", HTTP_GET, [](AsyncWebServerRequest *req) {
        build_batch_json();
        AsyncWebServerResponse *r =
            req->beginResponse(200, "application/json", s_batch_json);
        r->addHeader("Cache-Control", "no-cache, no-store");
        r->addHeader("Access-Control-Allow-Origin", "*");
        req->send(r);
    });
#endif

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
#if WIFI_TELEMETRY_DEFER_IN_RUNNING
    Serial.println("[WiFi] Modo defer RUNNING: GET /data/batch apos corrida.");
#endif
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
