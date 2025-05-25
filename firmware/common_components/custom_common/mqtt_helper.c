#include "mqtt_helper.h"
#include "esp_log.h"

static const char *TAG = "common_mqtt";


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            //ESP_LOGI(TAG, "MQTT connected, subscribing to 'button'");
            //esp_mqtt_client_subscribe(event->client, "button", mqtt_current_qos_level);
            //esp_mqtt_client_subscribe(event->client, "qos-level", 2);
            break;
        case MQTT_EVENT_DISCONNECTED:
            //TODO need to handle reconnect manually?
            ESP_LOGW(TAG, "MQTT disconnected");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Received topic: %.*s | data: %.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);
            //if (strncmp(event->topic, "button", event->topic_len) == 0) {
            //    buzzer_beep();
            //    ESP_LOGI(TAG, "button topic received!");
            //}
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
            ESP_LOGE(TAG, "MQTT error");
            break;
        default:
            ESP_LOGI(TAG, "unhandled mqtt event %ld received", event_id);
            break;
    }
}




esp_mqtt_client_handle_t common_mqtt_start(const char *broker_uri) {
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = broker_uri,
        .network.reconnect_timeout_ms = 2000
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    return client;
}
