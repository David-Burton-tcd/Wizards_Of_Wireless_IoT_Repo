#include <time.h>

#include "esp_wifi.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "lwip/apps/sntp.h"

#include "freertos/FreeRTOS.h"


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
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // Set the server by name
    // The server can be "pool.ntp.org", "time.nist.gov", or any other NTP server
    sntp_setservername(0, "pool.ntp.org");

    // This function initializes the SNTP service.
    sntp_init();
}


void wait_for_time_sync(void) {
    // Wait for time to be set
    int retry = 0;
    const int retry_count = 10; // Maximum retries
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