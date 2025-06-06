#pragma once
#include "driver/gpio.h"

// Shared sensor config struct for a single PZEM-004T
typedef struct {
    const char *name;               // Human-readable sensor name (for logs)
    uint8_t modbus_addr;            // Modbus slave ID
    gpio_num_t tx_pin;              // UART TX (may be shared)
    gpio_num_t rx_pin;              // UART RX (unique per sensor)
    bool use_rs485;                 // Enable RS485 mode instead of normal TTL
    gpio_num_t rs485_dir_pin;             // RS485 DIR pin to control half-duplex
    const char *mqtt_topic_prefix;  // MQTT topic prefix to publish data under
    int publish_interval_ms;        // How often to read + publish
} ModbusSensor;
