#include "mqtt_client.h"
#include "esp_http_client.h"


void initialize_wifi(const char *ssid, const char *pass, esp_event_handler_t wifi_event_handler);


esp_mqtt_client_handle_t initialize_mqtt(const char *uri, const char *user, const char *password, esp_event_handler_t mqtt_event_handler);


void initialize_sntp(void);


void wait_for_time_sync(void);


void print_current_time();