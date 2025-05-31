#include "../custom_common/wifi_helper.h"
#include "../custom_common/mqtt_helper.h"
#include "../custom_common/powermon_task.h"
#include "../custom_common/config_types.h"

#include "nvs_flash.h"
#include "esp_log.h"



//===== CONFIG =====
#include "credentials.h" //actual wifi credentials outsourced
//#define WIFI_SSID "E14_mqtt-broker"
//#define WIFI_PASS "mqttdemo" // comment out if open wifi
#define WIFI_USE_STATIC_IP       1
#define WIFI_STATIC_IP_ADDR      "10.0.0.82"
#define WIFI_STATIC_NETMASK_ADDR "255.255.0.0"
#define WIFI_STATIC_GW_ADDR      "10.0.0.1"

#define UART_PORT UART_NUM_2
#define MQTT_BROKER_URI "mqtt://10.0.0.102"


// Local config for this ESP32 instance
// configure all connected PZEM-004T sensors
#define PUBLISH_INTERVAL_MS 60000 // interval all sensor data is read and published for each sensor
#define RETRY_INTERVAL_WHEN_READ_FAILED_MS 2000 //retry earlier than next interval when read failed
const ModbusSensor sensors[] = {
    // Note about connection: multiple sensors are connected to shared TX pin (master) 
    //   but each has its own RX pin to send to the master
    //   (shared RX would be shortcircuit since IDLE slaves pull active high)

    // Note:TX0/GPIO10 and RX0/GPIO9 cant be used multiple times, due to crash sometimes when re-configuring it
    {
        .name = "Sensor1, 0x1 - links",
        .modbus_addr = 1,
        .tx_pin = GPIO_NUM_18,
        .rx_pin = GPIO_NUM_19,
        .use_rs485 = true,
        .rs485_dir_pin = GPIO_NUM_21,
        .mqtt_topic_prefix = "Sensordaten/PV/NeueSchupfe/sunnyboyLinks",
        .publish_interval_ms = PUBLISH_INTERVAL_MS,
    },
    {
        .name = "Sensor2, 0xA5 - rechts",
        .modbus_addr = 0xA5, //165 Note: using significantly different than 1 so sensors dont get confused
        .tx_pin = GPIO_NUM_18,
        .rx_pin = GPIO_NUM_19,
        .use_rs485 = true,
        .rs485_dir_pin = GPIO_NUM_21,
        .mqtt_topic_prefix = "Sensordaten/PV/NeueSchupfe/sunnyboyRechts",
        .publish_interval_ms = PUBLISH_INTERVAL_MS,
    }
};


// Variables
#define TAG "app_main"



void app_main(void) {
    ESP_LOGI(TAG, "[APP] Startup..");
    nvs_flash_init();

    wifi_settings_t wifi = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
        .use_static_ip = WIFI_USE_STATIC_IP,
        .ip = WIFI_STATIC_IP_ADDR,
        .netmask = WIFI_STATIC_NETMASK_ADDR,
        .gateway = WIFI_STATIC_GW_ADDR
    };
    ESP_LOGW(TAG, "Starting wifi...");
    common_wifi_start(&wifi);
    vTaskDelay(pdMS_TO_TICKS(4000));


    ESP_LOGW(TAG, "Starting mqtt...");
    esp_mqtt_client_handle_t mqtt_client = common_mqtt_start(MQTT_BROKER_URI);
    vTaskDelay(pdMS_TO_TICKS(2000));


    PMonTaskConfig_t powerMonitor_TaskCfg = {
        .sensors = sensors,
        .sensor_count = sizeof(sensors) / sizeof(sensors[0]),
        .uart_port = UART_PORT,
        .mqtt_client = mqtt_client,
        .retry_interval_on_fail_ms = RETRY_INTERVAL_WHEN_READ_FAILED_MS
    };

    ESP_LOGW(TAG, "Starting publish task...");
    xTaskCreate((TaskFunction_t) common_PMonTask, "PowerMonitor", 4096, (void*) &powerMonitor_TaskCfg, 5, NULL);
}
