#pragma once
#include "config_types.h"
#include "mqtt_client.h"
#include "driver/uart.h"


// Struct to pass to the task
typedef struct {
    const ModbusSensor *sensors;
    const int sensor_count;
    const uart_port_t uart_port;
    const esp_mqtt_client_handle_t mqtt_client;
    const int retry_interval_on_fail_ms;
} PMonTaskConfig_t;



// Starts the background task that periodically reads + publishes sensor data
// repeatedly read and publish all data of multiple sensors
// where the UART interface is re-initialized for each sensor to 
// allow individual uart pin configuration for each sensor
void common_PMonTask(void * PMonTaskConfig_t);
