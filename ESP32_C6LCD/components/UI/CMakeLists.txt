set(srcs "ui.c")

idf_component_register(
    SRCS "${srcs}"
    PRIV_REQUIRES lvgl freertos lvgl_drv driver esp_wifi get_bili
    INCLUDE_DIRS "."
)