#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef const char *esp_event_base_t;
typedef void *esp_mqtt_client_handle_t;
typedef int esp_mqtt_event_id_t;
#define ESP_EVENT_ANY_ID -1
enum { MQTT_EVENT_CONNECTED, MQTT_EVENT_DISCONNECTED, MQTT_EVENT_ERROR, MQTT_EVENT_PUBLISHED };
typedef struct {
    int error_type, esp_tls_last_esp_err, esp_tls_stack_err, esp_tls_cert_verify_flags;
    int esp_transport_sock_errno, connect_return_code;
} esp_mqtt_error_codes_t;
typedef struct { int msg_id; esp_mqtt_error_codes_t *error_handle; } esp_mqtt_event_t;
typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;
typedef struct {
    struct { struct { const char *uri; } address; struct { const char *certificate; } verification; } broker;
    struct { struct { const char *certificate, *key; } authentication; } credentials;
    struct { uint64_t limit; } outbox;
} esp_mqtt_client_config_t;
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config);
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, int event, void (*handler)(void *, esp_event_base_t, int32_t, void *), void *arg);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client);
int esp_mqtt_client_enqueue(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain, bool store);
