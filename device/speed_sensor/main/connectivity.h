#include "mqtt_client.h"
#include "esp_http_client.h"
#include "esp_gap_ble_api.h"


static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};


void initialize_wifi(const char *ssid, const char *pass, esp_event_handler_t wifi_event_handler);


esp_mqtt_client_handle_t initialize_mqtt(const char *uri, const char *user, const char *password, esp_event_handler_t mqtt_event_handler);


void initialize_sntp(void);


void wait_for_time_sync(void);


void print_current_time();


void initialize_ble(esp_gap_ble_cb_t esp_gap_cb);


void advertise_idle();


void advertise_deploy_speed_bump();


void advertise_retract_speed_bump();