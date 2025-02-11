set(component_srcs "idf_wifi.c")

idf_component_register(
    SRCS "${component_srcs}" 
    PRIV_REQUIRES   driver
                    nvs_flash
                    esp_event
                    esp_netif
                    esp_wifi
    INCLUDE_DIRS ".")