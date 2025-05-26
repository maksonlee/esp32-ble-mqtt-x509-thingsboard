#include "wifi_provisioning.h"
#include "mqtt_client_handler.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "app_event.h"

static const char *TAG = "app_main";
static void app_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base == APP_EVENT && id == APP_EVENT_WIFI_CONNECTED)
    {
        ESP_LOGI(TAG, "Wi-Fi connected, starting MQTT");
        mqtt_app_start();
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "System init");

    ESP_ERROR_CHECK(esp_event_loop_create_default()); // ← REQUIRED!

    ESP_ERROR_CHECK(esp_event_handler_register(APP_EVENT, APP_EVENT_WIFI_CONNECTED, &app_event_handler, NULL));
    wifi_provisioning_start();
}