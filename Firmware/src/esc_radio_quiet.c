/*
 * esc_radio_quiet.c — Libera rádio BT quando PS4 está desligado (bancada Serial HMI).
 *
 * O core Bluepad32 inicializa BTstack antes de setup(); pinos ADC2 (GPIO 4/32/33)
 * podem falhar como saída SD enquanto o controlador BT estiver ativo.
 */

#include "esc_radio_quiet.h"

#include "board_config.h"

#if !BOARD_ENABLE_PS4_BT

#include "esp_bt.h"
#include "esp_err.h"

#if __has_include("esp_bt_main.h")
#include "esp_bt_main.h"
#define ESC_HAS_BLUEDROID 1
#else
#define ESC_HAS_BLUEDROID 0
#endif

void esc_radio_quiet_init(void)
{
    esp_bt_controller_status_t st = esp_bt_controller_get_status();

    if (st == ESP_BT_CONTROLLER_STATUS_ENABLED) {
#if ESC_HAS_BLUEDROID
        if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
            esp_bluedroid_disable();
        }
        if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
            esp_bluedroid_deinit();
        }
#endif
        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        st = esp_bt_controller_get_status();
    }

    if (st == ESP_BT_CONTROLLER_STATUS_IDLE) {
        esp_bt_controller_mem_release(ESP_BT_MODE_BLE | ESP_BT_MODE_CLASSIC_BT);
    }
}

#else

void esc_radio_quiet_init(void)
{
}

#endif
