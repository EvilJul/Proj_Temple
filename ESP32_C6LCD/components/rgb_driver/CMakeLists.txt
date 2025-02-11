set(srcs "rgb_driver.c")

idf_component_register(
        SRCS "${srcs}"
        PRIV_REQUIRES freertos led_strip driver
        INCLUDE_DIRS "."
        )
