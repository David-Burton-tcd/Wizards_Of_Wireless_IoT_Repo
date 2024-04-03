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
#include "freertos/task.h"
#include "driver/mcpwm_prelude.h"
#include <semaphore.h> 
#include "esp_timer.h"


#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        0   // Minimum angle
#define SERVO_MAX_DEGREE        180    // Maximum angle

#define SERVO_PULSE_GPIO             26        // GPIO connects to the PWM signal line
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms
#define DEPLOYMENT_STATE_CHANGE_MIN_DELAY 5
static mcpwm_cmpr_handle_t comparator = NULL;
//sem_t mutex_motor; 


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

static inline uint32_t example_angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

static bool is_deployed = false;
static int last_bump_change = 0;

static void deactivate_speed_bump(bool isBySignal) 
{
    int current_tick_count = xTaskGetTickCount();
    if(is_deployed && current_tick_count - last_bump_change > DEPLOYMENT_STATE_CHANGE_MIN_DELAY){
        int angle = 180;
        int step = -1;
        while (!((angle + step) < 0)) {
            ESP_LOGI(DEMO_TAG, "Angle of rotation: %d", angle);
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(angle)));
            //Add delay, since it takes time for servo to rotate, usually 200ms/60degree rotation under 5V power supply
            vTaskDelay(pdMS_TO_TICKS(1));
            angle += step;
        }
        is_deployed = false;
    }
    if(isBySignal) {
        last_bump_change = xTaskGetTickCount();
    }
}

static void deactivate_speed_bump_automatic() {
    vTaskDelay(pdMS_TO_TICKS(10000));
    deactivate_speed_bump(false);
}

static void activate_speed_bump()
{
    int current_tick_count = xTaskGetTickCount();
    if(!is_deployed && current_tick_count - last_bump_change > DEPLOYMENT_STATE_CHANGE_MIN_DELAY) {
        int angle = 0;
        int step = 1;
        while (!((angle + step) > 180)) {
            ESP_LOGI(DEMO_TAG, "Angle of rotation: %d", angle);
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(angle)));
            //Add delay, since it takes time for servo to rotate, usually 200ms/60degree rotation under 5V power supply
            vTaskDelay(pdMS_TO_TICKS(1));
            angle += step;
        }
        is_deployed = true;
        xTaskCreate(&deactivate_speed_bump_automatic, "undeploy_speed_bump_task", 2048, NULL, 5, NULL);
    }
    last_bump_change = xTaskGetTickCount();
}

static void setup_servo() 
{
        ESP_LOGI(DEMO_TAG, "Create timer and operator");
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

    ESP_LOGI(DEMO_TAG, "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    ESP_LOGI(DEMO_TAG, "Create comparator and generator from the operator");

    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = SERVO_PULSE_GPIO,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

    // set the initial compare value, so that the servo will spin to the center position
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(0)));

    ESP_LOGI(DEMO_TAG, "Set generator action on timer and compare event");
    // go high on counter empty
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    // go low on compare threshold
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

    ESP_LOGI(DEMO_TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    deactivate_speed_bump(false);
}

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
                        //ESP_LOGI(DEMO_TAG, "Device length match");
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
                            if(adv_data[17] == 0x01) {
                                activate_speed_bump();
                            } else if (adv_data[17] == 0x02) {
                                deactivate_speed_bump(true);
                            }
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

    setup_servo();
    ble_setup();
    esp_ble_gap_set_scan_params(&ble_scan_params);
}