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
#define WIFI_SSID "E14_mqtt-broker"
//#define WIFI_PASS "mqttdemo" // comment out if open wifi
#define WIFI_USE_STATIC_IP       1
#define WIFI_STATIC_IP_ADDR      "192.168.0.111"
#define WIFI_STATIC_NETMASK_ADDR "255.255.255.0"
#define WIFI_STATIC_GW_ADDR      "192.168.0.1"

#define MQTT_BROKER_URI "mqtt://192.168.0.1"  // replace with your laptop IP

#define BUZZER_GPIO 14
#define ADC_CHANNEL ADC1_CHANNEL_6 // GPIO34



//===== Variables =====
static const char *TAG = "ESP_A-poti";
esp_mqtt_client_handle_t mqtt_client;
uint8_t mqtt_current_qos_level = 0;



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


void buzzer_beep() {
    gpio_set_level(BUZZER_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(30));
    gpio_set_level(BUZZER_GPIO, 0);
}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected, subscribing to 'button'");
            esp_mqtt_client_subscribe(event->client, "button", mqtt_current_qos_level);
            esp_mqtt_client_subscribe(event->client, "qos-level", 2);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Received topic: %.*s | data: %.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);
            if (strncmp(event->topic, "button", event->topic_len) == 0) {
                buzzer_beep();
                ESP_LOGI(TAG, "button topic received!");
            }
            else if (strncmp(event->topic, "qos-level", event->topic_len) == 0) {
                static char raw[10];     // incoming payload
                int len = event->data_len;
                if (len > sizeof(raw) - 1) len = sizeof(raw) - 1;
                memcpy(raw, event->data, len);
                raw[len] = '\0';
                ESP_LOGI(TAG, "qos-level topic received! data: %s", raw);
                mqtt_current_qos_level = atoi(raw);
                ESP_LOGW(TAG, "changed qos level to %d", mqtt_current_qos_level);
                for (int i=0; i < mqtt_current_qos_level+1; i++){ // level 0 = 1 beep
                    buzzer_beep();
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                ESP_LOGW(TAG, "re-subscribing topic 'button' with new qos level...");
                esp_mqtt_client_subscribe(event->client, "button", mqtt_current_qos_level);
            }
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


static void publish_poti_task(void *arg) {
    const int threshold = 10;
    const int slow_publish_interval_ms = 10000;
    const int check_interval_ms = 25;
    const int adc_sample_count = 150;

    int last_sent_value = -threshold;
    int last_publish_time = 0;

    while (1) {
        uint32_t sum = 0;
        for (int i = 0; i < adc_sample_count; i++){
            sum += adc1_get_raw(ADC_CHANNEL);
        }
        int raw = sum / adc_sample_count;
        
        int diff = abs(raw - last_sent_value);
        int now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (last_sent_value == -1 || diff > threshold || (now - last_publish_time >= slow_publish_interval_ms)) {
            char msg[32];
            snprintf(msg, sizeof(msg), "%d", raw);
            esp_mqtt_client_publish(mqtt_client, "poti", msg, 0, mqtt_current_qos_level, 0);
            last_sent_value = raw;
            last_publish_time = now;
            ESP_LOGI(TAG, "Published value: %d", raw);
        }

        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
    }
}






pzem_setup_t pzConf =
{
    .pzem_uart   = UART_NUM_2,              /*  <== Specify the UART you want to use, UART_NUM_0, UART_NUM_1, UART_NUM_2 (ESP32 specific) */
    .pzem_rx_pin = GPIO_NUM_16,             /*  <== GPIO for RX */
    .pzem_tx_pin = GPIO_NUM_17,             /*  <== GPIO for TX */
    .pzem_addr   = PZ_DEFAULT_ADDRESS,      /*  If your module has a different address, specify here or update the variable in pzem004tv3.h */
};

TaskHandle_t PMonTHandle = NULL;
_current_values_t pzValues;            /* Measured values */

void PMonTask( void * pz );

void app_main()
{
    /* Initialize/Configure UART */
    PzemInit( &pzConf );

    xTaskCreate( PMonTask, "PowerMon", ( 256 * 8 ), NULL, tskIDLE_PRIORITY, &PMonTHandle );
}



void PMonTask( void * pz )
{
    for( ;; )
    {
        PzemGetValues( &pzConf, &pzValues );
        printf( "Vrms: %.1fV - Irms: %.3fA - P: %.1fW - E: %.2fWh\n", pzValues.voltage, pzValues.current, pzValues.power, pzValues.energy );
        printf( "Freq: %.1fHz - PF: %.2f\n", pzValues.frequency, pzValues.pf );

        ESP_LOGI( TAG, "Stack High Water Mark: %ld Bytes free", ( unsigned long int ) uxTaskGetStackHighWaterMark( NULL ) );     /* Show's what's left of the specified stacksize */

        vTaskDelay( pdMS_TO_TICKS( 2500 ) );
    }

    vTaskDelete( NULL );
}





//void app_main(void) {
//    ESP_LOGI(TAG, "[APP] Startup..");
//    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
//    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
//
//    //ESP_LOGW(TAG, "Starting wifi...");
//    //nvs_flash_init();
//    //wifi_init_sta();
//    //vTaskDelay(pdMS_TO_TICKS(6000));
//
//    //ESP_LOGW(TAG, "Configuring gpios...");
//    //gpio_reset_pin(BUZZER_GPIO);
//    //gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);
//    //adc1_config_width(ADC_WIDTH_BIT_12);
//    //adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);
//
//    //ESP_LOGW(TAG, "Starting mqtt...");
//    //mqtt_app_start();
//    //vTaskDelay(pdMS_TO_TICKS(3000));
//
//    //ESP_LOGW(TAG, "Starting publish task...");
//    //xTaskCreate(modbus_sensor_task, "modbus_sensor_task", 4096, NULL, 5, NULL);
//
//}



