#include "esp_wifi.h"
#include "mqtt_client.h"


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
	printf( "wifi_init_softap finished. SSID:%s  password:%s", ssid, pass);
}


esp_mqtt_client_handle_t initialize_mqtt(const char *uri, const char *user, const char *password, esp_event_handler_t mqtt_event_handler)
{
	esp_mqtt_client_config_t mqtt_cfg = {
        // .broker.address.uri = "mqtt://172.20.10.10",
		// .credentials.username = "tclient",
		// .credentials.authentication.password = "mqtttest"
        .broker.address.uri = uri,
		.credentials.username = user,
		.credentials.authentication.password = password
	};
	esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg); //sending struct as a parameter in init client function
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

	return client;
}
