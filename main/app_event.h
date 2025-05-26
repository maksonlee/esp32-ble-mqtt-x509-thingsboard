#pragma once

#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(APP_EVENT);

typedef enum
{
    APP_EVENT_WIFI_CONNECTED,
    // add more events here
} app_event_id_t;