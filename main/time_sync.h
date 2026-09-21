#pragma once
#include <stdbool.h>
/* Called only from the MQTT worker. Never disables TLS checks on failure. */
bool time_sync_wait(void);
