#include "dht11.h"
#include "driver/gpio.h"
#include "mqtt_client_handler.h"
#include "mqtt_client.h"
#include "cert_manager.h"
#include "esp_log.h"
#include "inttypes.h"
#include "esp_timer.h"

static const char *TAG = "mqtt_client";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static esp_timer_handle_t telemetry_timer = NULL;

static void send_telemetry(void *arg)
{
    dht11_reading_t reading;
    esp_err_t err = dht11_read(&reading);

    if (err == ESP_OK)
    {
        char payload[64];
        snprintf(payload, sizeof(payload),
                 "{\"temperature\":%d,\"humidity\":%d}",
                 reading.temperature, reading.humidity);

        int msg_id = esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", payload, 0, 1, 0);
        ESP_LOGI(TAG, "Published telemetry, msg_id=%d: %s", msg_id, payload);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to read from DHT11 sensor");
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        // Restart telemetry timer
        if (telemetry_timer)
        {
            esp_timer_stop(telemetry_timer);
            esp_timer_delete(telemetry_timer);
            telemetry_timer = NULL;
        }

        const esp_timer_create_args_t timer_args = {
            .callback = &send_telemetry,
            .name = "telemetry_timer"};
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &telemetry_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic(telemetry_timer, 1000000)); // every 1 second
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");

        if (telemetry_timer)
        {
            esp_timer_stop(telemetry_timer);
            esp_timer_delete(telemetry_timer);
            telemetry_timer = NULL;
        }

        // Do NOT destroy the client — let it auto-reconnect
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        break;
    }
}

void mqtt_app_start(void)
{
    ESP_LOGI(TAG, "Starting MQTT client...");

    if (mqtt_client != NULL)
    {
        ESP_LOGW(TAG, "MQTT client already initialized, skipping");
        return;
    }

    if (!cert_manager_load())
    {
        ESP_LOGE(TAG, "Certificate manager failed");
        return;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = CONFIG_BROKER_URI,
            .verification.certificate = cert_ca,
        },
        .credentials = {
            .authentication = {
                .certificate = cert_client,
                .key = key_client,
            },
        },
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client)
    {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        cert_manager_free();
        return;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT client failed to start: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        cert_manager_free();
        return;
    }
}