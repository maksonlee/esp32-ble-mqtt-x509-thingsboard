#include <assert.h>
#include <string.h>
#include "mqtt_client.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "../../main/dht11.h"

static unsigned bits, tasks, loads, frees, creates, destroys, queued;
static int fail_init, fail_register, fail_start, sample_error, disconnect_during_sample;
static bool synced = true, load_ok = true;
char *cert_ca = "test", *cert_client = "test", *key_client = "test";
bool cert_manager_load(void) { loads++; return load_ok; }
void cert_manager_free(void) { frees++; }
bool time_sync_wait(void) { return synced; }
EventGroupHandle_t xEventGroupCreate(void) { return &bits; }
EventBits_t xEventGroupGetBits(EventGroupHandle_t g) { return *g; }
EventBits_t xEventGroupSetBits(EventGroupHandle_t g, EventBits_t b) { return *g |= b; }
EventBits_t xEventGroupClearBits(EventGroupHandle_t g, EventBits_t b) { return *g &= ~b; }
EventBits_t xEventGroupWaitBits(EventGroupHandle_t g, EventBits_t b, BaseType_t clear, BaseType_t all, TickType_t timeout) { return *g; }
BaseType_t xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned priority, TaskHandle_t *handle) { tasks++; *handle = (void *)1; return pdPASS; }
unsigned ulTaskNotifyTake(BaseType_t clear, TickType_t timeout) { return 0; }
void xTaskNotifyGive(TaskHandle_t task) { assert(task); }
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config) {
    creates++; assert(config->outbox.limit == 16384);
    return fail_init ? NULL : (void *)2;
}
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, int event, void (*handler)(void *, esp_event_base_t, int32_t, void *), void *arg) { return fail_register ? ESP_FAIL : ESP_OK; }
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client) { return fail_start ? ESP_FAIL : ESP_OK; }
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client) { destroys++; return ESP_OK; }
int esp_mqtt_client_enqueue(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain, bool store) {
    assert(qos == 1 && !retain && store);
    assert(strcmp(data, "{\"temperature\":23,\"humidity\":49}") == 0);
    queued++; return 7;
}
esp_err_t dht11_read(dht11_reading_t *result) {
    if (disconnect_during_sample) { bits = 0; }
    *result = (dht11_reading_t){23, 49};
    return sample_error ? ESP_ERR_INVALID_CRC : ESP_OK;
}
#include "../../main/mqtt_client_handler.c"

int main(void) {
    mqtt_app_start(); mqtt_app_start(); assert(tasks == 1);
    mqtt_app_set_network(true);
    synced = false; assert(!start_client()); assert(creates == 0 && loads == 0);
    synced = true; load_ok = false; assert(!start_client()); assert(creates == 0);
    load_ok = true; fail_init = 1; assert(!start_client()); assert(frees == 1 && mqtt_client == NULL);
    fail_init = 0; fail_register = 1; assert(!start_client()); assert(destroys == 1 && mqtt_client == NULL);
    fail_register = 0; fail_start = 1; assert(!start_client()); assert(destroys == 2 && mqtt_client == NULL);
    fail_start = 0; assert(start_client()); assert(mqtt_client);
    esp_mqtt_event_t event = {0};
    mqtt_event_handler(NULL, NULL, MQTT_EVENT_CONNECTED, &event);
    send_telemetry(NULL); assert(queued == 1);
    sample_error = 1; send_telemetry(NULL); assert(queued == 1);
    sample_error = 0; disconnect_during_sample = 1; send_telemetry(NULL); assert(queued == 1);
    disconnect_during_sample = 0;
    bits = MQTT_CONNECTED_BIT; send_telemetry(NULL); assert(queued == 1);
    mqtt_event_handler(NULL, NULL, MQTT_EVENT_DISCONNECTED, &event);
    assert(!(bits & MQTT_CONNECTED_BIT));
    mqtt_app_set_network(false); assert(!(bits & NETWORK_READY_BIT));
    return 0;
}
