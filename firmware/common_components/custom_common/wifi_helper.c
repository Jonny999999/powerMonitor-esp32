#include "wifi_helper.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include <string.h>
#include <arpa/inet.h>

#define TAG "common_wifi"

// Delay between reconnect attempts — prevents a tight busy-loop when the AP is down.
#define RECONNECT_DELAY_US (5 * 1000000ULL)

static esp_timer_handle_t s_reconnect_timer = NULL;

static void reconnect_timer_cb(void *arg) {
    ESP_LOGW(TAG, "Attempting WiFi reconnect...");
    esp_wifi_connect();
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGW(TAG, "Connecting to WiFi...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Associated to AP");
                // cancel any pending reconnect timer
                if (s_reconnect_timer && esp_timer_is_active(s_reconnect_timer)) {
                    esp_timer_stop(s_reconnect_timer);
                }
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGE(TAG, "Disconnected. Reconnecting in 5s...");
                // use a one-shot timer so we don't hammer the driver if the AP is gone
                if (s_reconnect_timer) {
                    esp_timer_stop(s_reconnect_timer); // safe to call even when inactive
                    esp_timer_start_once(s_reconnect_timer, RECONNECT_DELAY_US);
                } else {
                    esp_wifi_connect(); // fallback (timer not created yet)
                }
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
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

    // Create the reconnect timer before registering the event handler
    esp_timer_create_args_t timer_args = {
        .callback = reconnect_timer_cb,
        .name = "wifi_reconnect"
    };
    esp_timer_create(&timer_args, &s_reconnect_timer);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, settings->ssid, sizeof(wifi_config.sta.ssid));

    if (settings->password) {
        strncpy((char *)wifi_config.sta.password, settings->password, sizeof(wifi_config.sta.password));
    }

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    // Register handlers before starting so no events are missed
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    // WIFI_EVENT_STA_START fires after this and the handler calls esp_wifi_connect()
    esp_wifi_start();
}
