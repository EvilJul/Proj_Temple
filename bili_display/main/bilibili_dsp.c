#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "idf_wifi.h"
#include <cJSON.h>
#include <http_client.h>
#include <stdio.h>

#define WIFI "WIFI"
#define IP "IP_EVENT"
// #define SSID "iPhone"
// #define PASSWD "1234567890"
#define SSID "202"
#define PASSWD "Ti@n!99&"
#define BILIBILI_API_URL "https://api.bilibili.com/x/web-interface/view?bvid=BV1USrzYTE7T"

#define WIFI_GET_IP BIT7
#define WIFI_LOSE_IP BIT8
#define IS_NEED_UPDATE BIT9
#define HTTP_RESPONSE_DATA 1024 * 8

static char*  response_buf;
static size_t response_buf_len = 0;

EventGroupHandle_t CLIENT_EVENT, JOSN_EVENT;
TaskHandle_t       JSON_FORMAT = NULL;

static void run_on_event(esp_event_base_t event_base, int32_t event_id,
                         esp_event_handler_t event_handler, void* event_handler_arg)
{
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(IP, "已成功获取到ip");
        xEventGroupSetBits(CLIENT_EVENT, WIFI_GET_IP);
        break;

    case IP_EVENT_STA_LOST_IP:
        ESP_LOGI(IP, "获取ip失败！");
        xEventGroupSetBits(CLIENT_EVENT, WIFI_LOSE_IP);
        break;
    };
}

static void json_format(void* param)
{
    while (1) {
        xEventGroupWaitBits(JOSN_EVENT, IS_NEED_UPDATE, pdTRUE, pdTRUE, portMAX_DELAY);

        cJSON* char_to_json = cJSON_Parse(response_buf);
        if (char_to_json == NULL)
            ESP_LOGE("CJSON", "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        else {
            ESP_LOGE("JSON_DATA", "%s\n", cJSON_Print(char_to_json));
            free(cJSON_Print(char_to_json));
        }

        // cJSON* data = cJSON_GetObjectItem(char_to_json, "data");
        // if (data != NULL) {
        //     cJSON* title = cJSON_GetObjectItem(data, "title");
        //     if (title != NULL) {
        //         ESP_LOGE("CJSON", "=============================");
        //     }
        //     else
        //         ESP_LOGE("CJSON", "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        // }
        // else
        //     ESP_LOGE("CJSON", "Error parsing JSON: %s\n", cJSON_GetErrorPtr());

        cJSON_Delete(char_to_json);
    }
}

void app_main(void)
{
    esp_err_t err;
    CLIENT_EVENT = xEventGroupCreate();
    JOSN_EVENT   = xEventGroupCreate();
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, run_on_event, NULL);
    nvs_init();
    wifi_sta_model_init();
    err = wifi_connect(SSID, PASSWD, GPIO_NUM_2);
    if (err != ESP_OK) {
        ESP_LOGE("INFO", "我完蛋了！");
        return;
    }
    xEventGroupWaitBits(CLIENT_EVENT, WIFI_GET_IP, pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
    // http_client_init();
    while (1) {
        TickType_t tick = xTaskGetTickCount();

        // 初始化数据缓存区
        response_buf    = malloc(1);
        response_buf[0] = "\0";

        char* temp = http_client_init_get(BILIBILI_API_URL);
        if (temp != NULL) {
            // 动态调整缓冲区大小
            response_buf = realloc(response_buf, response_buf_len + sizeof(temp) + 1);
            memcpy(response_buf + response_buf_len, temp, sizeof(temp));
            response_buf_len += sizeof(temp);
            response_buf[response_buf_len] = "\0";
            ESP_LOGE("MSG", "%s\n", response_buf);

            if (JSON_FORMAT == NULL)
                xTaskCreate(json_format, "json_format", 1024 * 8, NULL, 1, &JSON_FORMAT);
            // printf("Received data: %p\n", (void*)response_buf);
            xEventGroupSetBits(JOSN_EVENT, IS_NEED_UPDATE);
        }
        else
            ESP_LOGE("CLIENT_MSG", "HTTPS RESPONSE ERROR!");

        vTaskDelayUntil(&tick, pdMS_TO_TICKS(1000 * 30));
    }
}
