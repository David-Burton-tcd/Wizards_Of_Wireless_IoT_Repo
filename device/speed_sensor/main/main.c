#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

// for multiple protocol interoperability
#include "protocol_examples_common.h"

#include "mqtt_client.h"
#include "connectivity.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "sdkconfig.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"


char *MQTT_DEVICE_UPGRADE_TOPIC = "/device/upgrade";
char *MQTT_BUMP_CONTROLLER_TOPIC = "/device/bump";
const char *wifi_ssid = CONFIG_WIFI_SSID;
const char *wifi_pass = CONFIG_WIFI_PASSWORD;
const char *firmware_url = CONFIG_FIRMWARE_UPGRADE_URL;
const char *mqtt_broker_uri = CONFIG_MQTT_BROKER_URI;
const char *mqtt_broker_user = CONFIG_MQTT_BROKER_USER;
const char *mqtt_broker_pass = CONFIG_MQTT_BROKER_PASSWORD;
static const char *MQTT_TAG = "MQTT";
static const char *HTTP_TAG = "HTTP";

// Global MQTT client handle
esp_mqtt_client_handle_t mqtt_client = NULL;

typedef struct {
    float speed; // Speed in km/h
    float length; // Length in meters
} car_readings;


float generate_random_float(float min, float max) {
    return min + ((float)rand() / RAND_MAX) * (max - min);
}


static car_readings random_sensor_readings() {
	car_readings sample;

	sample.speed = generate_random_float(0.0, 200.0);
    sample.length = generate_random_float(3.0, 5.0);

    return sample;
}


static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(MQTT_TAG, "Last error %s: 0x%x", message, error_code);
    }
}


static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data){
	if(event_id == WIFI_EVENT_STA_START)
	{
		printf("WIFI CONNECTING....\n");
	}
	else if (event_id == WIFI_EVENT_STA_CONNECTED)
	{
		printf("WiFi CONNECTED\n");
	}
	else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		printf("WiFi lost connection\n");
		int retry_num = 0;

		if (retry_num < 5)
		{
			esp_wifi_connect();
			retry_num++;
			printf("Retrying to Connect...\n");
		}
	}
	else if (event_id == IP_EVENT_STA_GOT_IP)
	{
		printf("Wifi got IP...\n\n");
	}
}


esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(HTTP_TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(HTTP_TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(HTTP_TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(HTTP_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(HTTP_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(HTTP_TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(HTTP_TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(HTTP_TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}


void upgrade_firmware_task(void *pvParameters) {
    ESP_LOGI("UPGRADE", "Starting OTA Upgrade task");

    esp_http_client_config_t config = {
        .url = firmware_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
        .keep_alive_enable = true,
    };
    config.skip_cert_common_name_check = true;

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    ESP_LOGI("UPGRADE", "Attempting to download update from %s", config.url);
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI("UPGRADE", "OTA Succeed, Rebooting...");
        esp_restart();
    } else {
        ESP_LOGE("UPGRADE", "Firmware upgrade failed");
		vTaskDelete(NULL);
    }
}


void sensor_data_send_task(void *pvParameters) {
    // sends dummy sensor data every 5 seconds
    while (true) {
        // print_current_time();
		car_readings current_sample = random_sensor_readings();
		char* mqtt_message = malloc(100 * sizeof(char));

		sprintf(mqtt_message, "{\"device\": \"sensor_1\", \"data\": {\"speed\": \"%.2f\", \"length\": \"%.2f\"}}", current_sample.speed, current_sample.length);

		esp_mqtt_client_publish(mqtt_client, "/device/data", mqtt_message, 0, 0, true);
		vTaskDelay(5000 /portTICK_PERIOD_MS);
	}
}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(MQTT_TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, MQTT_DEVICE_UPGRADE_TOPIC, 0);
        ESP_LOGI(MQTT_TAG, "sent subscribe successful, msg_id=%d", msg_id);

		msg_id = esp_mqtt_client_subscribe(client, MQTT_BUMP_CONTROLLER_TOPIC, 0);
        ESP_LOGI(MQTT_TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

		// handle device upgrade topic
		if (strcmp(event->topic, MQTT_DEVICE_UPGRADE_TOPIC)) {
			xTaskCreate(&upgrade_firmware_task, "upgrade_firmware", 8192, NULL, 5, NULL);
		}

        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(MQTT_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }
}


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
                ESP_LOGE("BLE", "Adv start failed: %s", esp_err_to_name(err));
            }
        break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
                ESP_LOGE("BLE", "Adv stop failed: %s", esp_err_to_name(err));
            }
            else {
                ESP_LOGI("BLE", "Stop adv successfully");
            }
        break;

        default:
        break;
    }
}


void app_main(void)
{
	esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    initialize_ble(esp_gap_cb);
    ESP_LOGI("BLE", "Configuring payload");
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

	initialize_wifi(wifi_ssid, wifi_pass, wifi_event_handler);
	
	ESP_LOGI("MAIN", "5s delay to connect to Wifi.");
	vTaskDelay(5000 /portTICK_PERIOD_MS);

    initialize_sntp();
    wait_for_time_sync();
	
	mqtt_client = initialize_mqtt(
		mqtt_broker_uri,
		mqtt_broker_user,
		mqtt_broker_pass,
		mqtt_event_handler
	);

    xTaskCreate(&sensor_data_send_task, "sensor_data_task", 2048, NULL, 5, NULL);
}
