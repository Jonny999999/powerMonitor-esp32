#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <arpa/inet.h> 


#include "pzem004tv3.h"



//===== CONFIG =====
#include "credentials.h" //actual wifi credentials outsourced
//#define WIFI_SSID "E14_mqtt-broker"
//#define WIFI_PASS "mqttdemo" // comment out if open wifi
#define WIFI_USE_STATIC_IP       1
#define WIFI_STATIC_IP_ADDR      "10.0.0.81"
#define WIFI_STATIC_NETMASK_ADDR "255.255.0.0"
#define WIFI_STATIC_GW_ADDR      "10.0.0.1"

#define MQTT_BROKER_URI "mqtt://10.0.0.102"  // replace with your laptop IP

#define BUZZER_GPIO 14
#define ADC_CHANNEL ADC1_CHANNEL_6 // GPIO34



//===== Variables =====
static const char *TAG = "PMon-main";
esp_mqtt_client_handle_t mqtt_client;



void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    #if WIFI_USE_STATIC_IP
    ESP_LOGI(TAG, "Using static IP configuration");
    if (esp_netif_dhcpc_stop(netif) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop DHCP client");
    } else {
        esp_netif_ip_info_t ip;
        memset(&ip, 0, sizeof(ip));
        ip.ip.addr = ipaddr_addr(WIFI_STATIC_IP_ADDR);
        ip.netmask.addr = ipaddr_addr(WIFI_STATIC_NETMASK_ADDR);
        ip.gw.addr = ipaddr_addr(WIFI_STATIC_GW_ADDR);

        if (esp_netif_set_ip_info(netif, &ip) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set IP info");
        } else {
            ESP_LOGI(TAG, "Static IP set to: %s", WIFI_STATIC_IP_ADDR);
        }
    }
#else
    ESP_LOGI(TAG, "Using DHCP");
#endif

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            #ifdef WIFI_PASS
            .password = WIFI_PASS,
            #endif
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}


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





static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            //ESP_LOGI(TAG, "MQTT connected, subscribing to 'button'");
            //esp_mqtt_client_subscribe(event->client, "button", mqtt_current_qos_level);
            //esp_mqtt_client_subscribe(event->client, "qos-level", 2);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Received topic: %.*s | data: %.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);
            //if (strncmp(event->topic, "button", event->topic_len) == 0) {
            //    buzzer_beep();
            //    ESP_LOGI(TAG, "button topic received!");
            //}
            //else if (strncmp(event->topic, "qos-level", event->topic_len) == 0) {
            //    static char raw[10];     // incoming payload
            //    int len = event->data_len;
            //    if (len > sizeof(raw) - 1) len = sizeof(raw) - 1;
            //    memcpy(raw, event->data, len);
            //    raw[len] = '\0';
            //    ESP_LOGI(TAG, "qos-level topic received! data: %s", raw);
            //    mqtt_current_qos_level = atoi(raw);
            //    ESP_LOGW(TAG, "changed qos level to %d", mqtt_current_qos_level);
            //    for (int i=0; i < mqtt_current_qos_level+1; i++){ // level 0 = 1 beep
            //        buzzer_beep();
            //        vTaskDelay(pdMS_TO_TICKS(100));
            //    }
            //    ESP_LOGW(TAG, "re-subscribing topic 'button' with new qos level...");
            //    esp_mqtt_client_subscribe(event->client, "button", mqtt_current_qos_level);
            //}
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");

        default:
            ESP_LOGI(TAG, "unhandled mqtt event %ld received", event_id);
            break;
    }
}


static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .network.reconnect_timeout_ms = 1000
    };
    // mqtt_client is a global variable to have access in other tasks
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}


// struct for configuring one connected PZEM-004T sensor
typedef struct {
    const char *name;               // For logging
    uint8_t modbus_addr;            // Modbus address (slave ID)
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
    const char *mqtt_topic_prefix;  // e.g. "sensor1"
    int publish_interval_ms;        // Interval for MQTT publishing
} ModbusSensor;


// configure all connected PZEM-004T sensors
#define NUM_SENSORS 4 // sensor count configured
#define UART_PORT UART_NUM_2
#define PUBLISH_INTERVAL_MS 60000 // interval all sensor data is read and published for each sensor
#define RETRY_INTERVAL_WHEN_READ_FAILED_MS 5000 //retry earlier than next interval when read failed
ModbusSensor sensors[NUM_SENSORS] = {
    // Note about connection: multiple sensors are connected to shared TX pin (master) 
    //   but each has its own RX pin to send to the master
    //   (shared RX would be shortcircuit since IDLE slaves pull active high)
    {
        .name = "Sensor 1",
        .modbus_addr = 1,
        .tx_pin = GPIO_NUM_16,
        .rx_pin = GPIO_NUM_17,
        .mqtt_topic_prefix = "Sensordaten/PV/Schupfe/sunnyboyLinks",
        .publish_interval_ms = PUBLISH_INTERVAL_MS,
    },
    {
        .name = "Sensor 2",
        .modbus_addr = 2,
        .tx_pin = GPIO_NUM_16,
        .rx_pin = GPIO_NUM_18,
        .mqtt_topic_prefix = "Sensordaten/PV/Schupfe/sunnyboyRechts",
        .publish_interval_ms = PUBLISH_INTERVAL_MS,
    },
    {
        .name = "Sensor 3",
        .modbus_addr = 3,
        .tx_pin = GPIO_NUM_16,
        .rx_pin = GPIO_NUM_19,
        .mqtt_topic_prefix = "Sensordaten/PV/Schupfe/goodweLinks",
        .publish_interval_ms = PUBLISH_INTERVAL_MS,
    },
    {
        .name = "Sensor 4",
        .modbus_addr = 4,
        .tx_pin = GPIO_NUM_16,
        .rx_pin = GPIO_NUM_21,
        .mqtt_topic_prefix = "Sensordaten/PV/Schupfe/goodweRechts",
        .publish_interval_ms = PUBLISH_INTERVAL_MS,
    }
};




// repeatedly read and publish all data of multiple sensors
// where the UART interface is re-initialized for each sensor to 
// allow individual uart pin configuration for each sensor
void PMonTask(void *arg) {
    int64_t last_publish_time[NUM_SENSORS] = {0};
    _current_values_t pzValues;

    while (1) {
        int64_t now = esp_timer_get_time() / 1000;

        for (int i = 0; i < NUM_SENSORS; i++) {
            if ((now - last_publish_time[i]) >= sensors[i].publish_interval_ms) {
                ESP_LOGI(TAG, "[%s] sensor is due for publishing, Initializing sensor at addr=0x%02X on TX=%d, RX=%d",
                         sensors[i].name,
                         sensors[i].modbus_addr,
                         sensors[i].tx_pin,
                         sensors[i].rx_pin);

                // Create new config for this sensor
                pzem_setup_t config = {
                    .pzem_uart   = UART_PORT,
                    .pzem_rx_pin = sensors[i].rx_pin,
                    .pzem_tx_pin = sensors[i].tx_pin,
                    .pzem_addr   = sensors[i].modbus_addr
                };

                // Initialize pins/config
                PzemInit(&config);

                // Log before read
                ESP_LOGI(TAG, "[%s] Starting read from sensor addr=0x%02X", sensors[i].name, sensors[i].modbus_addr);

                if (PzemGetValues(&config, &pzValues)) {
                    bool allZero = (pzValues.voltage == 0.0f && 
                                    pzValues.current == 0.0f && 
                                    pzValues.power == 0.0f && 
                                    pzValues.energy == 0.0f && 
                                    pzValues.frequency == 0.0f && 
                                    pzValues.pf == 0.0f);
                    if (allZero) {
                        ESP_LOGE(TAG, "[%s] Read succeeded but returned all zeros – treating as failed", sensors[i].name);
                        last_publish_time[i] += RETRY_INTERVAL_WHEN_READ_FAILED_MS; // when failed set next retry to faster interval
                    } else {
                        ESP_LOGI(TAG, "[%s] Read success", sensors[i].name);
                        printf("[%s] Vrms: %.1fV - Irms: %.3fA - P: %.1fW - E: %.2fWh\n", sensors[i].name, pzValues.voltage, pzValues.current, pzValues.power, pzValues.energy);
                        printf("[%s] Freq: %.1fHz - PF: %.2f\n", sensors[i].name, pzValues.frequency, pzValues.pf);

                        // publish all received values to the corresponding topics (with prefix of sensor)
                        char topic[128];
                        char payload[64];

                        snprintf(topic, sizeof(topic), "%s/voltage", sensors[i].mqtt_topic_prefix);
                        snprintf(payload, sizeof(payload), "%.1f", pzValues.voltage);
                        esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);

                        snprintf(topic, sizeof(topic), "%s/current", sensors[i].mqtt_topic_prefix);
                        snprintf(payload, sizeof(payload), "%.3f", pzValues.current);
                        esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);

                        snprintf(topic, sizeof(topic), "%s/power", sensors[i].mqtt_topic_prefix);
                        snprintf(payload, sizeof(payload), "%.1f", pzValues.power);
                        esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);

                        snprintf(topic, sizeof(topic), "%s/energy", sensors[i].mqtt_topic_prefix);
                        snprintf(payload, sizeof(payload), "%.2f", pzValues.energy);
                        esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);

                        snprintf(topic, sizeof(topic), "%s/frequency", sensors[i].mqtt_topic_prefix);
                        snprintf(payload, sizeof(payload), "%.1f", pzValues.frequency);
                        esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);

                        snprintf(topic, sizeof(topic), "%s/pf", sensors[i].mqtt_topic_prefix);
                        snprintf(payload, sizeof(payload), "%.2f", pzValues.pf);
                        esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);

                        last_publish_time[i] = now; // success, set next read to configured interval
                    } // endif - data is valid
                } else { // else - read successfull -> read failed
                    ESP_LOGE(TAG, "[%s] Failed to read sensor at addr=0x%02X", sensors[i].name, sensors[i].modbus_addr);
                    last_publish_time[i] += RETRY_INTERVAL_WHEN_READ_FAILED_MS; // when failed set next retry to faster interval
                }

                printf("\n");
            }// endif - is due for publishing
        } // endfor - each configured sensor

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelete(NULL);
}





void app_main(void) {
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    ESP_LOGW(TAG, "Starting wifi...");
    nvs_flash_init();
    wifi_init_sta();
    // register event handler to automatically reconnect when connection lost
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    vTaskDelay(pdMS_TO_TICKS(6000));

    //ESP_LOGW(TAG, "Configuring gpios...");
    //gpio_reset_pin(BUZZER_GPIO);
    //gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);
    //adc1_config_width(ADC_WIDTH_BIT_12);
    //adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);

    ESP_LOGW(TAG, "Starting mqtt...");
    mqtt_app_start();
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGW(TAG, "Starting publish task...");
    xTaskCreate(PMonTask, "PowerMonitor", 4096, NULL, 5, NULL);

}



