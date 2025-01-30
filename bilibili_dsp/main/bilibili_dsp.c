#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "http_client.h"
#include "idf_wifi.h"
#include "ssd1306.h"
#include <stdio.h>
#include <string.h>

#define SDA_GPIO GPIO_NUM_13
#define SCL_GPIO GPIO_NUM_18
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// #define SSID "iPhone"
// #define PASSWD "1234567890"
#define SSID "202"
#define PASSWD "Ti@n!99&"
#define BILIBILI_API_URL "https://api.bilibili.com/x/web-interface/view?bvid=BV17xfWYnENz"
#define VIEW "View: "
#define COIN "Coin: "
#define REPLY "Reply: "
#define LIKE "Like: "

#define WIFI_GET_IP BIT0
#define WIFI_GET_IP_FAIL BIT1
#define SSD1306_UPDATE BIT2
#define SNTP_UPDATE_SUC BIT3
#define SNTP_UPDATE_FAIL BIT4
#define HTTP_GET_RESPONSE BIT5
#define HTTP_GET_RESPONSE_FAIL BIT6

#define GOT_IP "GOT_IP"

typedef struct
{
    char* title;
    int   view, reply, coin, like;
} BILIBILI_t;

static char*  response_buf;
static size_t response_len;
BILIBILI_t    msg;
SSD1306_t     dev;

EventGroupHandle_t SNTP_SERVER, JSON_FORMAT, SSD1306_SERVER, RESPONSE_SERVER, GET_IP, RESTART;
TaskHandle_t       CJSON_FORMAT = NULL, SSD1306_DISP = NULL, INIT_SNTP = NULL, HTTP_RESPOSNE = NULL,
             ESP_RESTART = NULL;
TimerHandle_t SNTP_TIMER = NULL, HTTP_GET_TIMER = NULL;

void time_reflash()
{
    time_t    now         = 0;
    struct tm timeInfo    = {0};
    int       retry       = 0;
    const int retry_count = 10;

    while (timeInfo.tm_year < (2025 - 1900) && ++retry <= retry_count) {
        ESP_LOGI("SNTP_UPDATE", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeInfo);
    }
    if (retry >= retry_count) {
        ESP_LOGE("SNTP_UPDATE", "Failed to get time from SNTP server!");
        xEventGroupSetBits(SNTP_SERVER, SNTP_UPDATE_FAIL);
        xEventGroupSetBits(RESTART, SNTP_UPDATE_FAIL);
    }
    else {
        ESP_LOGI("SNTP_UPDATE", "Time synchronized successfully!");
        xEventGroupSetBits(SNTP_SERVER, SNTP_UPDATE_SUC);
        xEventGroupSetBits(SSD1306_SERVER, SNTP_UPDATE_SUC);
    }
}

static void run_on_event(esp_event_base_t event_base, int32_t event_id,
                         esp_event_handler_t event_handler, void* event_handler_arg)
{
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(GOT_IP, "已成功获取到ip");
        xEventGroupSetBits(GET_IP, WIFI_GET_IP);
        break;

    case IP_EVENT_STA_LOST_IP:
        ESP_LOGE(GOT_IP, "获取ip失败！");
        xEventGroupSetBits(GET_IP, WIFI_GET_IP_FAIL);
        xEventGroupSetBits(RESTART, WIFI_GET_IP_FAIL);
        break;
    };
}

static void sntp_refresh_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI("SNTP_UPDATE", "Refreshing SNTP time...");
    esp_sntp_stop();   // 停止 SNTP 服务
    sntp_init();       // 重新初始化 SNTP 服务，触发时间校准

    time_reflash();
}

void initialize_sntp(void* param)
{
    setenv("TZ", "CTS-8", 1);
    tzset();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);   // 设置为轮询模式
    sntp_setservername(0, "pool.ntp.org");     // 使用 NTP 服务器
    esp_sntp_stop();
    sntp_init();
    time_reflash();

    SNTP_TIMER = xTimerCreate("sntp时间同步",
                              pdMS_TO_TICKS(1000 * 60 * 5),
                              pdTRUE,
                              (void*)0,
                              sntp_refresh_timer_callback);
    if (SNTP_TIMER != NULL) {
        xTimerStart(SNTP_TIMER, 0);   // 启动定时器
        ESP_LOGI("SNTP_UPDATE", "SNTP refresh timer started");
    }
    else {
        ESP_LOGE("SNTP_UPDATE", "Failed to create SNTP refresh timer");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void ssd1306_show(void* param)
{
    char      view_str[20], reply_str[20], coin_str[20], like_str[20], time_str[50];
    time_t    now;
    struct tm timeInfo;

    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev, 0, "Loading data...", strlen("Loading data..."), false);

    while (1) {
        xEventGroupWaitBits(SSD1306_SERVER,
                            SNTP_UPDATE_SUC | HTTP_GET_RESPONSE | SSD1306_UPDATE,
                            pdTRUE,
                            pdTRUE,
                            portMAX_DELAY);

        ssd1306_clear_line(&dev, 0, false);
        ssd1306_display_text(&dev, 0, "Updae data...", strlen("Updae data..."), false);

        time(&now);
        localtime_r(&now, &timeInfo);
        strftime(time_str, sizeof(time_str), "%Y/%m/%d %H:%M:%S", &timeInfo);
        sprintf(view_str, "%s%d", VIEW, msg.view);
        sprintf(reply_str, "%s%d", REPLY, msg.reply);
        sprintf(coin_str, "%s%d", COIN, msg.coin);
        sprintf(like_str, "%s%d", LIKE, msg.like);
        vTaskDelay(pdMS_TO_TICKS(500));

        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, 0, "BILIBILI: DACHAI", strlen("BILIBILI: DACHAI"), false);
        ssd1306_display_text(&dev, 1, time_str, strlen(time_str), false);
        ssd1306_display_text(&dev, 2, msg.title, strlen(msg.title), false);
        ssd1306_display_text(&dev, 3, view_str, strlen(view_str), false);
        ssd1306_display_text(&dev, 4, like_str, strlen(like_str), false);
        ssd1306_display_text(&dev, 5, reply_str, strlen(reply_str), false);
        ssd1306_display_text(&dev, 6, coin_str, strlen(coin_str), false);
        ssd1306_display_text(&dev, 7, "===================", strlen("==================="), false);
    }
}

void json_format(void* param)
{
    while (1) {
        xEventGroupWaitBits(RESPONSE_SERVER, HTTP_GET_RESPONSE, pdTRUE, pdTRUE, portMUX_NO_TIMEOUT);
        memset(&msg, 0, sizeof(msg));

        cJSON* json_div1 = cJSON_Parse(*(char**)param);
        if (json_div1 == NULL) ESP_LOGE("CJSON", "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        // else
        // {
        //     ESP_LOGE("JSON_DATA", "%s\n", cJSON_Print(char_to_json));
        //     free(cJSON_Print(char_to_json));
        // }

        cJSON* data = cJSON_GetObjectItem(json_div1, "data");
        if (data != NULL) {
            cJSON* _title = cJSON_GetObjectItem(data, "title");
            if (_title != NULL) msg.title = cJSON_Print(_title);

            cJSON* _stat = cJSON_GetObjectItem(data, "stat");
            if (_stat != NULL) {
                msg.coin  = cJSON_GetNumberValue(cJSON_GetObjectItem(_stat, "coin"));
                msg.like  = cJSON_GetNumberValue(cJSON_GetObjectItem(_stat, "like"));
                msg.reply = cJSON_GetNumberValue(cJSON_GetObjectItem(_stat, "reply"));
                msg.view  = cJSON_GetNumberValue(cJSON_GetObjectItem(_stat, "view"));

                xEventGroupSetBits(SSD1306_SERVER, SSD1306_UPDATE);
                if (SSD1306_DISP == NULL)
                    xTaskCreate(ssd1306_show, "ssd1306_show", 1024 * 8, NULL, 1, &SSD1306_DISP);
            }
            else
                ESP_LOGE("CJSON", "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void http_get_data(void* url)
{
    while (1) {
        TickType_t current_time = xTaskGetTickCount();
        response_buf            = malloc(1);
        response_buf[0]         = '\0';
        response_len            = 0;
        char* temp              = http_client_init_get(BILIBILI_API_URL);
        if (temp != NULL) {
            response_buf = realloc(response_buf, response_len + strlen(temp) + 1);
            memcpy(response_buf + response_len, temp, strlen(temp));
            response_len += strlen(temp);
            response_buf[response_len] = '\0';

            if (CJSON_FORMAT == NULL) {
                xTaskCreate(
                    json_format, "json_format", 1024 * 8, (void*)&response_buf, 1, &CJSON_FORMAT);
            }
            xEventGroupSetBits(RESPONSE_SERVER, HTTP_GET_RESPONSE);
            xEventGroupSetBits(SSD1306_SERVER, HTTP_GET_RESPONSE);
        }
        else {
            ESP_LOGE("CLIENT_MSG", "B站时评数据获取失败！");
            xEventGroupSetBits(RESPONSE_SERVER, HTTP_GET_RESPONSE_FAIL);
            xEventGroupSetBits(RESTART, HTTP_GET_RESPONSE_FAIL);
        }
        vTaskDelayUntil(&current_time, pdMS_TO_TICKS(1000 * 60 * 5));
    }
}

void restart_esp(void* param)
{
    while (1) {
        xEventGroupWaitBits(RESTART,
                            HTTP_GET_RESPONSE_FAIL | SNTP_UPDATE_FAIL | WIFI_GET_IP_FAIL,
                            pdTRUE,
                            pdFALSE,
                            portMAX_DELAY);
        esp_restart();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void app_main(void)
{
    esp_err_t err;
    SNTP_SERVER     = xEventGroupCreate();
    JSON_FORMAT     = xEventGroupCreate();
    SSD1306_SERVER  = xEventGroupCreate();
    RESPONSE_SERVER = xEventGroupCreate();
    GET_IP          = xEventGroupCreate();
    RESTART         = xEventGroupCreate();

    i2c_master_init(&dev, SDA_GPIO, SCL_GPIO, -1);
    ssd1306_init(&dev, OLED_WIDTH, OLED_HEIGHT);
    ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 0xff);
    ssd1306_display_text_x3(&dev, 3, "EvilSep", strlen("EvilSep"), false);

    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, run_on_event, NULL);
    nvs_init();
    wifi_sta_model_init();
    err = wifi_connect(SSID, PASSWD, GPIO_NUM_2);
    if (err != ESP_OK) {
        ESP_LOGE("WIFI_CONNEVT", "WIFI连接失败！请重启！");
        ssd1306_clear_screen(&dev, false);
        ssd1306_display_text(&dev, 2, "WI-FI CONNECT FAIL!", strlen("WI-FI CONNECT FAIL!"), false);
        ssd1306_display_text(&dev,
                             4,
                             "PLEASE RESTART OR CHECK ERROR!",
                             strlen("PLEASE RESTART OR CHECK ERROR!"),
                             false);
        return;
    }
    xEventGroupWaitBits(GET_IP, WIFI_GET_IP, pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
    if (INIT_SNTP == NULL)
        xTaskCreate(initialize_sntp, "initiallize_sntp", 1024 * 4, NULL, 1, &INIT_SNTP);
    if (HTTP_RESPOSNE == NULL)
        xTaskCreate(http_get_data, "http_get_data", 1024 * 8, NULL, 1, &HTTP_RESPOSNE);
    if (ESP_RESTART == NULL)
        xTaskCreate(restart_esp, "restart_esp", 1024 * 4, NULL, 1, ESP_RESTART);
}
