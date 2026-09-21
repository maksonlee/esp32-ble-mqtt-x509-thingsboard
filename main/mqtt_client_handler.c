#include "dht11.h"
#include "driver/gpio.h"
#include "mqtt_client_handler.h"
#include "mqtt_client.h"
#include "cert_manager.h"
#include "esp_log.h"
#include "inttypes.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include <stdio.h>

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
        if (msg_id < 0) {
            ESP_LOGW(TAG, "Telemetry submission failed (%s)",
                     msg_id == -2 ? "outbox full" : "transport or allocation failure");
        } else {
            ESP_LOGI(TAG, "Submitted telemetry, msg_id=%d: %s", msg_id, payload);
        }
    }
    else
    {
        ESP_LOGW(TAG, "DHT11 read failed: %s", esp_err_to_name(err));
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
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
        if (event && event->error_handle) {
            const esp_mqtt_error_codes_t *error = event->error_handle;
            ESP_LOGW(TAG, "MQTT error type=%d tls=0x%x stack=0x%x verify=0x%x socket=%d connack=%d",
                     error->error_type, error->esp_tls_last_esp_err,
                     error->esp_tls_stack_err, error->esp_tls_cert_verify_flags,
                     error->esp_transport_sock_errno, error->connect_return_code);
        } else {
            ESP_LOGW(TAG, "MQTT error without transport details");
        }
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Broker acknowledged telemetry, msg_id=%d", event->msg_id);
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
