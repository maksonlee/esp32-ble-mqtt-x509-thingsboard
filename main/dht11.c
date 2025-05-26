#include "dht11.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp32/rom/ets_sys.h"

#define DHT11_GPIO GPIO_NUM_23
#define DHT_OK 0
#define DHT_TIMEOUT_ERROR -1
#define DHT_CHECKSUM_ERROR -2

static const char *TAG = "dht11";

static int wait_level(gpio_num_t pin, int level, int timeout_us)
{
    int t = 0;
    while (gpio_get_level(pin) == level)
    {
        if (t++ > timeout_us)
            return -1;
        ets_delay_us(1);
    }
    return t;
}

esp_err_t dht11_read(dht11_reading_t *result)
{
    gpio_num_t pin = DHT11_GPIO;
    uint8_t data[5] = {0};

    // Configure GPIO
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);
    vTaskDelay(pdMS_TO_TICKS(20)); // 20ms
    gpio_set_level(pin, 1);
    ets_delay_us(30);
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    if (wait_level(pin, 0, 80) < 0)
        return ESP_FAIL;
    if (wait_level(pin, 1, 80) < 0)
        return ESP_FAIL;

    for (int i = 0; i < 40; i++)
    {
        if (wait_level(pin, 0, 50) < 0)
            return ESP_FAIL;
        int len = wait_level(pin, 1, 70);
        if (len < 0)
            return ESP_FAIL;

        data[i / 8] <<= 1;
        if (len > 40)
            data[i / 8] |= 1;
    }

    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (data[4] != checksum)
    {
        ESP_LOGW(TAG, "Checksum mismatch: expected %02x, got %02x", checksum, data[4]);
        return ESP_ERR_INVALID_CRC;
    }

    result->humidity = data[0];
    result->temperature = data[2];
    return ESP_OK;
}