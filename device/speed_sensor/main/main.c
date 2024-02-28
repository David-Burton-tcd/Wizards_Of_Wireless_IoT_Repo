#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

const char *ssid = "";
const char *pass = "";
static const char *TAG = "mqtt_wow";

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
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
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

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void wifi_connection(){
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
	printf( "wifi_init_softap finished. SSID:%s  password:%s", ssid, pass);
}

static esp_mqtt_client_handle_t mqtt_initialize(void)
{
	esp_mqtt_client_config_t mqtt_cfg = {
		.broker.address.uri = "mqtt://172.20.10.10",
		.credentials.username = "tclient",
		.credentials.authentication.password = "mqtttest"
	};
	esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg); //sending struct as a parameter in init client function
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

	return client;
}

void app_main(void)
{
	nvs_flash_init();
	wifi_connection();
	ESP_LOGI(TAG, "20s delay to connect to Wifi.");
	vTaskDelay(20000 /portTICK_PERIOD_MS);
	esp_mqtt_client_handle_t mqtt_client = mqtt_initialize();

	while (true) {
		car_readings current_sample = random_sensor_readings();
		char* mqtt_message = malloc(100 * sizeof(char));

		sprintf(mqtt_message, "{\"device\": \"sensor_1\", \"data\": {\"speed\": \"%.2f\", \"length\": \"%.2f\"}}", current_sample.speed, current_sample.length);

		esp_mqtt_client_publish(mqtt_client, "test", mqtt_message, 0, 0, true);
		vTaskDelay(5000 /portTICK_PERIOD_MS);
	}
}
