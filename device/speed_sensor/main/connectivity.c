#include <time.h>

#include "esp_wifi.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"

#include "esp_sntp.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"

#include "freertos/FreeRTOS.h"

#include "esp_bt_main.h"



void initialize_wifi(const char *ssid, const char *pass, esp_event_handler_t wifi_event_handler)
{
	esp_netif_init();
	esp_event_loop_create_default();
	esp_netif_create_default_wifi_sta();
	wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&wifi_initiation);

	// register event handler
	esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
	esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

	wifi_config_t wifi_configuration = {
		.sta= {
			.ssid = "",
			.password= ""
		}
	};
	strcpy((char*)wifi_configuration.sta.ssid, ssid);
	strcpy((char*)wifi_configuration.sta.password, pass);
	esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
	esp_wifi_start();
	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_connect();
	printf( "wifi_init_softap finished. SSID:%s  password:%s\n", ssid, pass);
}


esp_mqtt_client_handle_t initialize_mqtt(const char *uri, const char *user, const char *password, esp_event_handler_t mqtt_event_handler)
{
	esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
		.broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
		.credentials.username = user,
		.credentials.authentication.password = password,
		.broker.verification.skip_cert_common_name_check = true
	};

	esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg); //sending struct as a parameter in init client function
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

	return client;
}


void initialize_sntp(void) {
    ESP_LOGI("SNTP", "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // Set the server by name
    // The server can be "pool.ntp.org", "time.nist.gov", or any other NTP server
    esp_sntp_setservername(0, "pool.ntp.org");

    // This function initializes the SNTP service.
    esp_sntp_init();
}


void wait_for_time_sync(void) {
    // Wait for time to be set
    int retry = 0;
    const int retry_count = 30; // Maximum retries
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI("SNTP", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    if (retry == retry_count) {
        ESP_LOGW("SNTP", "Could not synchronize time");
    } else {
        ESP_LOGI("SNTP", "Time synchronized");
        // Now you can call functions like 'localtime()' to get the current time
    }
}


void print_current_time() {
	// Retrieve current time
	time_t now = time(NULL);

	// Convert to local time format
	struct tm *local_time = localtime(&now);

	// Buffer to store the formatted date and time
	char time_str[100];

	// Format time into a string: e.g., "2023-03-26 14:55:02"
	strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);

	// Print the formatted time
	printf("Current time: %s\n", time_str);
	vTaskDelay(5000 /portTICK_PERIOD_MS);
}


void initialize_ble(esp_gap_ble_cb_t esp_gap_cb) {
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    esp_bluedroid_init();
    esp_bluedroid_enable();
    
    esp_err_t status;
    ESP_LOGI("BLE", "register callback");
    //register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE("BLE", "gap register error: %s", esp_err_to_name(status));
        return;
    }

}


void advertise_idle()
{
    uint8_t idle_message[] = {
        /*-- device name --*/
        0x0b, // length of type and device name(11)
        0x09, // device name type (1)
        'S','p','e','e','d',' ','B','u','m','p', // Device name (10)
        0x05, // length of custom data
        0xff, // custom type
        0x00,0x00,0x00,0x00
    };
    esp_ble_gap_config_adv_data_raw(idle_message, sizeof(idle_message));
    ESP_LOGI("BLE", "Advertise idle");
}


void advertise_deploy_speed_bump()
{
    uint8_t deploy_message[] = {
    /*-- device name --*/
    0x0b, // length of type and device name(11)
    0x09, // device name type (1)
    'S','p','e','e','d',' ','B','u','m','p', // Device name (10)
    0x05, // length of custom data
    0xff, // custom type
    0x00,0x00,0x00,0x01
    };
    esp_ble_gap_config_adv_data_raw(deploy_message, sizeof(deploy_message));
    ESP_LOGI("BLE", "Advertise deploy");
    vTaskDelay(2000 /portTICK_PERIOD_MS);
    advertise_idle();
}


void advertise_retract_speed_bump()
{
    uint8_t retract_message[] = {
    /*-- device name --*/
    0x0b, // length of type and device name(11)
    0x09, // device name type (1)
    'S','p','e','e','d',' ','B','u','m','p', // Device name (10)
    0x05, // length of custom data
    0xff, // custom type
    0x00,0x00,0x00,0x02
    };
    esp_ble_gap_config_adv_data_raw(retract_message, sizeof(retract_message));
    ESP_LOGI("BLE", "Advertise retract");
    vTaskDelay(2000 /portTICK_PERIOD_MS);
    advertise_idle();
}