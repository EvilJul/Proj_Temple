set(srcs "ble_app.c")

idf_component_register(
                    SRCS "${srcs}"
                    PRIV_REQUIRES driver esp_timer bt nvs_flash
                    INCLUDE_DIRS "." 
                    )