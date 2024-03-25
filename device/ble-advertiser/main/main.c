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

static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
            esp_ble_gap_start_advertising(&ble_adv_params);
        }
        break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            //adv start complete event to indicate adv start successfully or failed
            if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(DEMO_TAG, "Adv start failed: %s", esp_err_to_name(err));
            }
        break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
                ESP_LOGE(DEMO_TAG, "Adv stop failed: %s", esp_err_to_name(err));
            }
            else {
                ESP_LOGI(DEMO_TAG, "Stop adv successfully");
            }
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

    ESP_LOGI(DEMO_TAG, "Configuring payload");
    uint8_t hello_message[] = {
        /*-- device name --*/
        0x0b, // length of type and device name(11)
        0x09, // device name type (1)
        'S','p','e','e','d',' ','B','u','m','p', // Device name (10)
        0x05, // length of custom data
        0xff, // custom type
        0x00,0x00,0x00,0x00
    };

    esp_ble_gap_config_adv_data_raw(hello_message, sizeof(hello_message));

    /* Changing payload mid-way in some callback function */
    //
    // 1. modify the payload
    // uint8_t some_new_message[] = {
    //     /*-- device name --*/
    //     0x0b, // length of type and device name(11)
    //     0x09, // device name type (1)
    //     'S','p','e','e','d',' ','B','u','m','p', // Device name (10)
    //     0x05, // length of custom data
    //     0xff, // custom type
    //     0x01,0x02,0x03,0x04   // <--------------- new message
    // };
    // esp_ble_gap_config_adv_data_raw(some_new_message, sizeof(some_new_message));
    //
    // 2. start advertising again
    // esp_ble_gap_start_advertising(&ble_adv_params);
    //
    /*****/
}