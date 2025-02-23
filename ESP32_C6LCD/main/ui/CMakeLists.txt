SET(SOURCES screens/ui_loadScr.c
    screens/ui_mainScr.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_id_img_png.c
    images/ui_img_bg_2_png.c
    images/ui_img_wifi_1_png.c
    images/ui_img_gif_0_png.c
    images/ui_img_bg_1_png.c
    images/ui_img_wifi_2_png.c
    images/ui_img_wifi_3_png.c
    images/ui_img_wifi_4_png.c
    images/ui_img_gif_1_png.c
    images/ui_img_gif_2_png.c
    images/ui_img_gif_3_png.c
    images/ui_img_gif_4_png.c
    images/ui_img_gif_5_png.c
    images/ui_img_gif_6_png.c
    images/ui_img_gif_7_png.c
    images/ui_img_gif_8_png.c
    images/ui_img_gif_9_png.c
    images/ui_img_gif_10_png.c
    fonts/ui_font_zh_14_bpp4.c
    fonts/ui_font_zh_16_bpp4.c
    fonts/ui_font_zh__8_bpp8.c)

add_library(ui ${SOURCES})
