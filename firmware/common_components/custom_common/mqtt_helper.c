#include "mqtt_helper.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"

static const char *TAG = "common_mqtt";

static esp_mqtt_client_handle_t s_client = NULL;
static volatile bool s_mqtt_connected = false;

static void on_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (s_client && !s_mqtt_connected) {
        ESP_LOGI(TAG, "WiFi reconnected – triggering MQTT reconnect");
        esp_mqtt_client_reconnect(s_client);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            s_mqtt_connected = true;
            ESP_LOGI(TAG, "MQTT connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_mqtt_connected = false;
            ESP_LOGW(TAG, "MQTT disconnected, auto-reconnect active");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Received topic: %.*s | data: %.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error type=%d", event->error_handle->error_type);
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "  TCP: esp_err=0x%x errno=%d",
                         event->error_handle->esp_tls_last_esp_err,
                         event->error_handle->esp_transport_sock_errno);
            }
            break;
        default:
            ESP_LOGI(TAG, "unhandled mqtt event %ld received", event_id);
            break;
    }
}


esp_mqtt_client_handle_t common_mqtt_start(const char *broker_uri) {
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = broker_uri,
        .session.keepalive = 15,        // send PINGREQ every 15s; detect dead connection fast
        .network.reconnect_timeout_ms = 5000,
        .network.timeout_ms = 10000,    // socket ops timeout; prevents task blocking forever on dead TCP
    };

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    // When WiFi reconnects and we have an IP, trigger an immediate MQTT reconnect
    // instead of waiting for the reconnect timer to fire.
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL);

    esp_mqtt_client_start(s_client);
    return s_client;
}
