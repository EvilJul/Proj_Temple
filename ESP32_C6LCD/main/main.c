/*
    ·LVGL画面
        - 开机画面：√进度根据硬件加载情况进行显示、尝试解决重置后刷新闪屏问题
        - 主界面：√FPS显示、BILI数据显示、背景显示、√动画优化、
            -√CPU FPS直接从LVGL库中函数获取值
    ·Wi-Fi连接功能
        -√写死Wi-Fi配置信息
        -优化：设备更行触摸屏后改称可选择、字符输入面板
    ·PSP遥感控制功能
    ·蓝牙连接功能
    ·SD卡读写功能
    ·http访问功能
    ·√时间更新功能
*/

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "idf_wifi.h"
#include "lvgl/lvgl.h"
#include "lvgl_drv.h"
#include "rgb_driver.h"
#include "sd_drv.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void lvgl_task(void* Param);       // lvgl事件处理
void hw_init(void* Param);         // 硬件初始化
void splash_screen(void* Param);   // 创建开机动画
void led_rgb_disp(void* Param);    // LED呼吸闪烁

#define MAX_TRANSFER 4000
#define PCLK (40 * 1000 * 1000)

#define SSID "202"
#define PASSWD "Ti@n!99&"

#define GOT_IP BIT0
#define RESTART_TURE BIT1

static ST7789_t st7789_cfg = {
    .LCD_CLK_NUM     = GPIO_NUM_7,
    .LCD_MOSI_NUM    = GPIO_NUM_6,
    .LCD_MISO_NUM    = GPIO_NUM_5,
    .LCD_DC_NUM      = GPIO_NUM_15,
    .LCD_CS_NUM      = GPIO_NUM_14,
    .LCD_BL_NUM      = GPIO_NUM_22,
    .QUADWP_IO_NUM   = -1,
    .QUADHD_IO_NUM   = -1,
    .MAX_TRANSFER_SZ = MAX_TRANSFER,
    .PCLK_HZ         = PCLK,
    .LCD_CMD_BITS    = 8,
    .LCD_PARAM_BITS  = 8,
};
static RGB_t rgb_cfg = {
    .pin_rgb = GPIO_NUM_8,
    .rgb_num = 1,
};
static HOMEPAGE_ARC_HEAD _arc_head;

lv_obj_t*   _splash_scr    = NULL;
static int  progress_value = 5, temp_progress_value = 0;
static bool splash_scr_hid = false;
char*       init_name;

TaskHandle_t LVGL_TASK_HANDLE = NULL, HW_INIT = NULL, SPLASH_SCREEN = NULL, FPS_DISP = NULL,
             RESTART_ESP = NULL, FPS_UPDATE_VAL = NULL, LED_RGB_DISP = NULL, CPU_UPDATE_VAL = NULL;
EventGroupHandle_t IP_EVENT_GROUP, RESTART_EVENT;

char* name_init(char* name)
{
    init_name = (char*)realloc(NULL, 1 + strlen(name));
    if (init_name == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory.\n");
        return NULL;   // 返回一个空指针表示失败
    }
    strncpy(init_name, name, strlen(name) + 1);

    return init_name;
}

void netif_ip_event(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                    void* event_data)
{
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
        xEventGroupSetBits(IP_EVENT_GROUP, GOT_IP);
        progress_value += 5;
        temp_progress_value = progress_value;
        splash_anim_update(temp_progress_value, progress_value, name_init("GOT IP..."));
        free(init_name);
        break;

    default: break;
    }
}

void lvgl_task(void* Param)
{
    while (1) {
        lv_task_handler();

        if (splash_scr_hid) {
            if (_splash_scr != NULL && lv_obj_is_valid(_splash_scr)) {
                _arc_head      = homePagee();
                splash_scr_hid = false;

                vTaskDelete(SPLASH_SCREEN);
                SPLASH_SCREEN = NULL;
                _splash_scr   = NULL;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void led_rgb_disp(void* Param)
{
    while (1) {
        breath_effect(88, 245, &rgb_cfg);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void hw_init(void* Param)
{
    esp_err_t err;
    temp_progress_value = progress_value;
    progress_value += 5;
    splash_anim_update(temp_progress_value, progress_value, name_init("NVS init..."));
    splash_anim_update(temp_progress_value, progress_value, name_init("LVGL init..."));
    free(init_name);
    progress_value += 5;
    temp_progress_value = progress_value;
    splash_anim_update(temp_progress_value, progress_value, name_init("SPI init..."));
    free(init_name);
    progress_value += 5;
    temp_progress_value = progress_value;
    splash_anim_update(temp_progress_value, progress_value, name_init("SD init..."));
    free(init_name);
    progress_value += 5;
    temp_progress_value = progress_value;
    splash_anim_update(temp_progress_value, progress_value, name_init("LCD init..."));
    free(init_name);

    setup_leds(&rgb_cfg);
    progress_value += 5;
    temp_progress_value = progress_value;
    splash_anim_update(temp_progress_value, progress_value, name_init("RGB init..."));
    free(init_name);
    xTaskCreate(led_rgb_disp, "led_rgb_disp", 1024 * 4, NULL, 2, &LED_RGB_DISP);

    err = wifi_sta_model_init();
    if (err != ESP_OK) xEventGroupSetBits(RESTART_EVENT, RESTART_TURE);
    progress_value += 5;
    temp_progress_value = progress_value;
    splash_anim_update(temp_progress_value, progress_value, name_init("WIFI init..."));
    free(init_name);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, netif_ip_event, NULL);

    err = wifi_connect(SSID, PASSWD, -1);
    if (err != ESP_OK) xEventGroupSetBits(RESTART_EVENT, RESTART_TURE);
    progress_value += 5;
    temp_progress_value = progress_value;
    splash_anim_update(temp_progress_value, progress_value, name_init("Connecting"));
    free(init_name);

    xEventGroupWaitBits(IP_EVENT_GROUP, GOT_IP, pdTRUE, pdTRUE, portMAX_DELAY);

    sntp_init_zh();
    err = syn_time();
    if (err != ESP_OK) xEventGroupSetBits(RESTART_EVENT, RESTART_TURE);
    progress_value += 5;
    temp_progress_value = progress_value;
    splash_anim_update(temp_progress_value, progress_value, name_init("TIME INIT..."));
    free(init_name);

    // BLE初始化


    progress_value = 100;
    splash_anim_update(temp_progress_value, progress_value, name_init("FINISH..."));
    free(init_name);
    splash_scr_hid = true;

    while (1) {
        err = syn_time();
        while (err != ESP_OK) {
            err = syn_time();
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        vTaskDelay(pdMS_TO_TICKS(1000 * 60 * 5));
    }
}

void splash_screen(void* Param)
{
    _splash_scr = splash_anim();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void restart_esp(void* param)
{
    xEventGroupWaitBits(RESTART_EVENT, RESTART_TURE, pdTRUE, pdTRUE, portMAX_DELAY);
    esp_restart();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    esp_err_t err;
    IP_EVENT_GROUP = xEventGroupCreate();
    RESTART_EVENT  = xEventGroupCreate();

    lv_init();
    nvs_init();
    err = init_spi_bus(&st7789_cfg);
    if (err != ESP_OK) {
        ESP_LOGE("ERROR", "启动失败！%s", esp_err_to_name(err));
        return;
    }
    // err = init_sd_spi();
    // if (err != ESP_OK) {
    //     ESP_LOGE("ERROR", "启动失败！%s", esp_err_to_name(err));
    //     return;
    // }
    err = st7789_hw_init(&st7789_cfg);
    if (err != ESP_OK) {
        ESP_LOGE("ERROR", "启动失败！%s", esp_err_to_name(err));
        return;
    }
    err = lv_disp_init();
    if (err != ESP_OK) {
        ESP_LOGE("ERROR", "启动失败！%s", esp_err_to_name(err));
        return;
    }
    BK_Light(10);
    err = lv_timer();
    if (err != ESP_OK) {
        ESP_LOGE("ERROR", "启动失败！%s", esp_err_to_name(err));
        return;
    }

    xTaskCreate(splash_screen, "splash_screen", 1024 * 8, NULL, 3, &SPLASH_SCREEN);
    xTaskCreate(lvgl_task, "lvgl_task", 1024 * 64, NULL, 3, &LVGL_TASK_HANDLE);
    xTaskCreate(hw_init, "hw_init", 1024 * 128, NULL, 2, &HW_INIT);
    xTaskCreate(restart_esp, "restart_esp", 1024 * 1, NULL, 4, &RESTART_ESP);
}
