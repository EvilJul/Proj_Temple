set(srcs "sd_drv.c")

idf_component_register(
    SRCS "${srcs}"
    PRIV_REQUIRES  driver nvs_flash fatfs lvgl_drv
    INCLUDE_DIRS "."
)