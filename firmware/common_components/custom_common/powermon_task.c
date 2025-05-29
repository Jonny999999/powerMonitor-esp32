#include "powermon_task.h"
#include "pzem004tv3.h"
#include "esp_log.h"

#define TAG "common_PMon"


// repeatedly read and publish all data of multiple sensors
// where the UART interface is re-initialized for each sensor to 
// allow individual uart pin configuration for each sensor
void common_PMonTask(void *arg) {

    // extract config parameters from passed struct
    PMonTaskConfig_t *cfg = (PMonTaskConfig_t *)arg;
    const ModbusSensor *sensors = cfg->sensors;
    const int sensor_count = cfg->sensor_count;
    const uart_port_t uart_port = cfg->uart_port;
    const esp_mqtt_client_handle_t mqtt_client = cfg->mqtt_client;
    const int retry_interval_ms = cfg->retry_interval_on_fail_ms;

    // variables
    int64_t last_publish_time[sensor_count];
    memset(last_publish_time, 0, sizeof(last_publish_time));
    _current_values_t pzValues; // store module readout

    
    // repeatedly readout and publish modules in their interval
    while (1) {
        int64_t now = esp_timer_get_time() / 1000;

        // loop through all configured sensors
        for (int i = 0; i < sensor_count; i++) {
            if ((now - last_publish_time[i]) >= sensors[i].publish_interval_ms) {

                ESP_LOGI(TAG, "[%s] Due for publish. Init sensor addr=0x%02X TX=%d RX=%d RS485-MODE=%d",
                         sensors[i].name,
                         sensors[i].modbus_addr,
                         sensors[i].tx_pin,
                         sensors[i].rx_pin,
                         sensors[i].use_rs485);

                // Create new uart config for this sensor
                pzem_setup_t config = {
                    .pzem_uart   = uart_port,
                    .pzem_rx_pin = sensors[i].rx_pin,
                    .pzem_tx_pin = sensors[i].tx_pin,
                    .pzem_addr   = sensors[i].modbus_addr,
                    .use_rs485   = sensors[i].use_rs485,
                    .rs485_dir_pin = sensors[i].rs485_dir_pin
                };

                // Initialize pins/config
                PzemInit(&config);

                // Log before read
                ESP_LOGI(TAG, "[%s] Starting read from sensor addr=0x%02X", sensors[i].name, sensors[i].modbus_addr);

                // read
                if (PzemGetValues(&config, &pzValues)) {
                    bool allZero = (pzValues.voltage == 0.0f &&
                                    pzValues.current == 0.0f &&
                                    pzValues.power == 0.0f &&
                                    pzValues.energy == 0.0f &&
                                    pzValues.frequency == 0.0f &&
                                    pzValues.pf == 0.0f);

                    if (allZero) {
                        ESP_LOGE(TAG, "[%s] Read succeeded but all values zero – treating as failed", sensors[i].name);
                        last_publish_time[i] += retry_interval_ms; // when failed set next retry to faster interval
                    } else {
                        ESP_LOGI(TAG, "[%s] Read OK", sensors[i].name);
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
                    last_publish_time[i] += retry_interval_ms; // when failed set next retry to faster interval
                }

                // ensure there is a small delay between sensor readouts (prevents wrong sensor answering or all data 0)
                if (sensors[i].use_rs485)
                    vTaskDelay(pdMS_TO_TICKS(200));

                printf("\n");
            }// endif - is due for publishing
        } // endfor - each configured sensor

        vTaskDelay(pdMS_TO_TICKS(500));
    } // end while(1)

    vTaskDelete(NULL); // not really needed but formal
}
