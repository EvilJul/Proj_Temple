# 基于lvgl V8.3.10移植的驱动

set(srcs "lvgl_drv.c")

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS  "."
    PRIV_REQUIRES driver lvgl esp_lcd freertos
)