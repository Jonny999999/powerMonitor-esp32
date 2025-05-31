# Register custom_common component
idf_component_register(
    SRCS 
        "wifi_helper.c"
        "mqtt_helper.c"
        "powermon_task.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        esp_wifi
        esp_netif
        nvs_flash
        mqtt
        freertos
        driver
        pzem004tv3
)
