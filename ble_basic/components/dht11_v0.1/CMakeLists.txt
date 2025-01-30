set(srcs "dht11.c")

idf_component_register(
                    SRCS "${srcs}"
                    PRIV_REQUIRES driver esp_timer
                    INCLUDE_DIRS "."
                    )