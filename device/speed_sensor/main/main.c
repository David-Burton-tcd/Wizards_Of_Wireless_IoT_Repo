#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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

#include "esp_timer.h"

#include <ultrasonic.h>
#include <esp_err.h>

#define MAX_SAMPLES 100

char *device_firmware_version = CONFIG_DEVICE_FIRMWARE_VERSION;

char *MQTT_DEVICE_UPGRADE_TOPIC = "/device/upgrade";
char *MQTT_BUMP_CONTROLLER_TOPIC = "/device/bump";
const char *wifi_ssid = CONFIG_WIFI_SSID;
const char *wifi_pass = CONFIG_WIFI_PASSWORD;
const char *firmware_url = CONFIG_FIRMWARE_UPGRADE_URL;
char* firmware_binary = NULL;
const char *mqtt_broker_uri = CONFIG_MQTT_BROKER_URI;
const char *mqtt_broker_user = CONFIG_MQTT_BROKER_USER;
const char *mqtt_broker_pass = CONFIG_MQTT_BROKER_PASSWORD;
static const char *MQTT_TAG = "MQTT";
static const char *HTTP_TAG = "HTTP";

#define MAX_DISTANCE_CM 60 // 5m max
#define SENSOR_DISTANCE_CM 10 // Distance between sensors in cm

#define TRIGGER_GPIO_1 5
#define ECHO_GPIO_1 18

#define TRIGGER_GPIO_2 19 // Example GPIO for second sensor
#define ECHO_GPIO_2 21   // Example GPIO for second sensor

// Global MQTT client handle
esp_mqtt_client_handle_t mqtt_client = NULL;

float speed_samples[MAX_SAMPLES];
int sample_count = 0;
bool sensor_1_up = true;
bool sensor_2_up = true;


void add_speed_sample(float speed) {
    if (sample_count < MAX_SAMPLES) {
        speed_samples[sample_count++] = speed;
    }
}


float generate_random_float(float min, float max) {
    return min + ((float)rand() / RAND_MAX) * (max - min);
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

    char* firmware_complete_url = malloc(150 * sizeof(char));
    snprintf(firmware_complete_url, 100, "%s/%s", firmware_url, firmware_binary);

    esp_http_client_config_t config = {
        .url = firmware_complete_url,
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
        free(firmware_complete_url);
        esp_restart();
    } else {
        ESP_LOGE("UPGRADE", "Firmware upgrade failed");
        free(firmware_complete_url);
        free(firmware_binary);
        firmware_binary = NULL;
		vTaskDelete(NULL);
    }
}


void analyze_samples_send_over_mqtt() {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Wait for 5 seconds

        float max_speed = 0;
        float min_speed = 0;
        float sum_speed = 0;
        float average_speed = 0;

        if (sample_count > 0) {
            max_speed = speed_samples[0];
            min_speed = speed_samples[0];
            sum_speed = 0;
            for (int i = 0; i < sample_count; i++) {
                if (speed_samples[i] > max_speed) max_speed = speed_samples[i];
                if (speed_samples[i] < min_speed) min_speed = speed_samples[i];
                sum_speed += speed_samples[i];
            }
            average_speed = sum_speed / sample_count;
        }

        char* mqtt_message = malloc(250 * sizeof(char));
        sprintf(mqtt_message, "{\"device\": \"sensor_1\", \"version\": \"%s\", \"data\": {\"avg_speed\": \"%.2f\", \"max_speed\": \"%.2f\", \"min_speed\": \"%.2f\", \"num_cars\": \"%d\", \"sensor_1_up\": \"%d\", \"sensor_2_up\": \"%d\"}}", device_firmware_version, average_speed, max_speed, min_speed, sample_count, sensor_1_up, sensor_2_up);
        esp_mqtt_client_publish(mqtt_client, "/device/data", mqtt_message, 0, 0, true);
        // ESP_LOGI("ANALYZE", "Max Speed: %0.02f cm/s, Min Speed: %0.02f cm/s, Average Speed: %0.02f cm/s, Total Cars: %d", max_speed, min_speed, average_speed, sample_count);
        free(mqtt_message);

        // Reset the list
        sample_count = 0;
        sensor_1_up = true;
        sensor_2_up = true;
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
        
        // handle device bump deploy topic
        if (strncmp(event->topic, MQTT_BUMP_CONTROLLER_TOPIC, event->topic_len) == 0) {
            if (strncmp(event->data, "deploy", event->data_len) == 0){
                advertise_deploy_speed_bump();
            }
            else if (strncmp(event->data, "retract", event->data_len) == 0){
                advertise_retract_speed_bump();
            }
        }

		// handle device upgrade topic
		if (strncmp(event->topic, MQTT_DEVICE_UPGRADE_TOPIC, event->topic_len) == 0) {
            firmware_binary = malloc(event->data_len + 1);
            memcpy(firmware_binary, event->data, event->data_len);
            firmware_binary[event->data_len] = '\0';
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


void ultrasonic_sensor_data()
{
    ultrasonic_sensor_t sensor1 = {
        .trigger_pin = TRIGGER_GPIO_1,
        .echo_pin = ECHO_GPIO_1
    };

    ultrasonic_sensor_t sensor2 = {
        .trigger_pin = TRIGGER_GPIO_2,
        .echo_pin = ECHO_GPIO_2
    };

    ultrasonic_init(&sensor1);
    ultrasonic_init(&sensor2);

    TickType_t start_time = 0;

    while (true)
    {
        float distance1, distance2;
        esp_err_t res1 = ultrasonic_measure(&sensor1, MAX_DISTANCE_CM, &distance1);
        esp_err_t res2 = ultrasonic_measure(&sensor2, MAX_DISTANCE_CM, &distance2);

        if (res1 != ESP_OK)
        {
            // printf("Error on Sensor 1: %d: ", res1);
            sensor_1_up = false;
            // Handle errors for sensor 1
        }
        else if (distance1 * 100 < MAX_DISTANCE_CM)
        {
            start_time = xTaskGetTickCount();
            printf("Distance from Sensor 1: %0.04f cm\n", distance1 * 100);
        }

        if (res2 != ESP_OK)
        {
            // printf("Error on Sensor 2: %d: ", res2);
            sensor_2_up = false;
            // Handle errors for sensor 2
            // Dummy data
            float chance = generate_random_float(0, 100);
            if (chance < 10) {
                add_speed_sample(generate_random_float(0.0, 200.0));
            }
        }
        else if (distance2 * 100 < MAX_DISTANCE_CM)
        {
            printf("Distance from Sensor 2: %0.04f cm\n", distance2 * 100);
            if (start_time != 0) // If the timer was started
            {
                TickType_t end_time = xTaskGetTickCount(); // Get the current time
                float time_taken = ((float)(end_time - start_time)) * portTICK_PERIOD_MS / 1000; // Calculate time taken in seconds
                float speed = SENSOR_DISTANCE_CM / time_taken; // Calculate speed of passing car
                printf("Speed of passing car: %0.02f cm/s\n", speed);
                start_time = 0; // Reset the timer
                add_speed_sample(speed);
                if (speed > 50){
                    ESP_LOGI("TAG", "%s", "Too fast");
                    advertise_deploy_speed_bump();
                }
                else {  
                    ESP_LOGI("TAG", "%s", "Too slow");
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
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
    advertise_idle();

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

    xTaskCreate(&ultrasonic_sensor_data, "ultrasonic_sensor_data", 2048, NULL, 5, NULL);
    xTaskCreate(&analyze_samples_send_over_mqtt, "analyze_samples_send_over_mqtt", 2048, NULL, 5, NULL);    
}
