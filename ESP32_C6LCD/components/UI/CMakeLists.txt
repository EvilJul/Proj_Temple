set(
    srcs 
    "ui.c"
    "coin.c"
    "like.c"
    "reply.c"
    "view.c"
    "img_bg_2.c"
    "img_id.c"
    "img_wifi_1.c"
    "img_wifi_2.c"
    "img_wifi_3.c"
    "img_wifi_4.c"
)

idf_component_register(
    SRCS "${srcs}"
    PRIV_REQUIRES lvgl freertos lvgl_drv driver esp_wifi get_bili
    INCLUDE_DIRS "."
)