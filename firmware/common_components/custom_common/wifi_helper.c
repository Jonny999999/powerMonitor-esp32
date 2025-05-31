#include "wifi_helper.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>
#include <arpa/inet.h>

#define TAG "common_wifi"



static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGW("WiFi", "Connecting to WiFi...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGE("WiFi", "Disconnected. Reconnecting...");
                esp_wifi_connect();
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WiFi", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}



void common_wifi_start(wifi_settings_t *settings) {
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    if (settings->use_static_ip) {
        ESP_LOGI(TAG, "Setting static IP: %s", settings->ip);
        esp_netif_dhcpc_stop(netif);

        esp_netif_ip_info_t ip = {
            .ip.addr = ipaddr_addr(settings->ip),
            .netmask.addr = ipaddr_addr(settings->netmask),
            .gw.addr = ipaddr_addr(settings->gateway)
        };

        esp_netif_set_ip_info(netif, &ip);
    }
    else {
        ESP_LOGI(TAG, "Using DHCP");
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, settings->ssid, sizeof(wifi_config.sta.ssid));

    if (settings->password) {
        strncpy((char *)wifi_config.sta.password, settings->password, sizeof(wifi_config.sta.password));
    }

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

    // register event handler to automatically reconnect when connection lost
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
}
