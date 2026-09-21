#include "wifi_provisioning.h"
#include "mqtt_client_handler.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "System init");

    ESP_ERROR_CHECK(esp_event_loop_create_default()); // ← REQUIRED!

    mqtt_app_start();
    wifi_provisioning_start();
}
