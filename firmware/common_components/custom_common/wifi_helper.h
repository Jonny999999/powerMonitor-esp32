#pragma once

// WiFi connection settings
typedef struct {
    const char *ssid;
    const char *password; // NULL = open network
    int use_static_ip;
    const char *ip;
    const char *netmask;
    const char *gateway;
} wifi_settings_t;

// Connects to WiFi (DHCP or static) using provided settings
void common_wifi_start(wifi_settings_t *settings);
