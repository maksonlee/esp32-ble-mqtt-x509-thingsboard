#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void mqtt_app_start(void);
    void mqtt_app_set_network(bool available);

#ifdef __cplusplus
}
#endif
