#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char* DEMO_TAG = "IBEACON_DEMO";
uint8_t expected_device_name[] = {0x0b,0x09,'S','p','e','e','d',' ','B','u','m','p'};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
            uint32_t duration = 0;
            esp_ble_gap_start_scanning(duration);
        }
        break;
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            //scan start complete event to indicate scan start successfully or failed
            if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(DEMO_TAG, "Scan start failed: %s", esp_err_to_name(err));
            }
        break;
        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
            switch (scan_result->scan_rst.search_evt) {
                case ESP_GAP_SEARCH_INQ_RES_EVT:
                    uint8_t *adv_data = scan_result->scan_rst.ble_adv;
                    uint8_t adv_data_len = scan_result->scan_rst.adv_data_len;

                    // printf("\n[");
                    // printf("Advertising length: %d, ", adv_data_len);
                    // printf("Advertising data: ");
                    // for (int i = 0; i < adv_data_len; i++) {
                    //     printf("%02x ", adv_data[i]);
                    // }
                    // printf("]\n");

                    // match advertisement length with expected length 18
                    if (adv_data_len == 18) {
                        ESP_LOGI(DEMO_TAG, "Device length match");
                        // match name of the device
                        bool device_name_check = true;
                        for (int i = 0; i < 12; i++) {
                            if (expected_device_name[i] != adv_data[i]) {
                                device_name_check = false;
                                break;
                            }
                        }
                        // if device matched, example for how to get the 4 bytes of data we have reserved
                        if (device_name_check) {
                            ESP_LOGI(DEMO_TAG, "Found speed bump controller");
                            ESP_LOGI(DEMO_TAG, "%02x %02x %02x %02x", adv_data[14], adv_data[15], adv_data[16], adv_data[17]);
                        }
                    }
                break;
                default:
                break;
            }
        }
        break;
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        break;
        default:
        break;
    }
}

void ble_setup() {
    esp_bluedroid_init();
    esp_bluedroid_enable();
    
    esp_err_t status;
    ESP_LOGI(DEMO_TAG, "register callback");
    //register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(DEMO_TAG, "gap register error: %s", esp_err_to_name(status));
        return;
    }

}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    ble_setup();
    esp_ble_gap_set_scan_params(&ble_scan_params);
}