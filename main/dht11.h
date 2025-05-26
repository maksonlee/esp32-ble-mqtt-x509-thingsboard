#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct
{
    int temperature;
    int humidity;
} dht11_reading_t;

esp_err_t dht11_read(dht11_reading_t *result);