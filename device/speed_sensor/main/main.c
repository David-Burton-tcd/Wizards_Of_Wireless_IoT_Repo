#include <stdio.h>
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


const char *ssid = "<>";
const char *pass = "<>";


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


void app_main(void)
{
	nvs_flash_init();
	wifi_connection();
}
