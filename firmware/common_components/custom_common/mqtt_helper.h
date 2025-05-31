#pragma once
#include "mqtt_client.h"

// Initializes and starts MQTT client, returns mqtt client handle
esp_mqtt_client_handle_t common_mqtt_start(const char *broker_uri);
