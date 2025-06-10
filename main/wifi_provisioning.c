#include "wifi_provisioning.h"
#include "mqtt_client_handler.h"
#include "app_event.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

static const char *TAG = "wifi_prov";

static int retry_count = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Wi-Fi STA Started");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "Connected to Wi-Fi");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        retry_count = 0;
        ESP_LOGI(TAG, "Got IP address, posting APP_EVENT_WIFI_CONNECTED...");
        esp_event_post(APP_EVENT, APP_EVENT_WIFI_CONNECTED, NULL, 0, portMAX_DELAY);
        ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler));
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Wi-Fi disconnected");

        ESP_LOGI(TAG, "Retrying Wi-Fi... attempt %d", ++retry_count);

        // Optional: short delay to avoid spamming connection attempts
        vTaskDelay(pdMS_TO_TICKS(5000));

        esp_wifi_connect();
    }
}

void wifi_provisioning_start(void)
{
    // Initialize NVS with fallback logic
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    else
    {
        ESP_ERROR_CHECK(ret);
    }

    // Initialize network interfaces
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    // Register Wi-Fi and IP event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Initialize Wi-Fi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Check if device is already provisioned
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned)
    {
        ESP_LOGI(TAG, "Starting Wi-Fi provisioning via BLE");

        // Generate BLE device name using MAC
        char service_name[13] = {0}; // PROV_ + 6 hex digits + null terminator
        uint8_t mac[6];
        ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
        snprintf(service_name, sizeof(service_name), "PROV_%02X%02X%02X", mac[3], mac[4], mac[5]);
        ESP_LOGI(TAG, "BLE Device Name: %s", service_name);

        // Set up BLE provisioning configuration
        wifi_prov_mgr_config_t config = {
            .scheme = wifi_prov_scheme_ble,
            .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE};

        ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, NULL, service_name, NULL));
    }
    else
    {
        ESP_LOGI(TAG, "Already provisioned, connecting to saved Wi-Fi");
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
}