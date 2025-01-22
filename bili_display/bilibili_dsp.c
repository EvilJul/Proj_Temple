#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "idf_wifi.h"
#include <stdio.h>

#define WIFI "WIFI"
#define IP "IP_EVENT"
// #define SSID "iPhone"
// #define PASSWD "1234567890"
#define SSID "202"
#define PASSWD "Ti@n!99&"
#define BILIBILI_API_URL "https://api.bilibili.com/x/web-interface/view?bvid=BV1USrzYTE7T"
#define HTTP_CLIENT_TIMEOUT 50000
#define CLIENT_TAG "HTTP_CLIENT"

#define WIFI_GET_IP BIT7
#define WIFI_LOSE_IP BIT8

#define HTTP_CLIENT_EVENT_ERROR BIT0
#define HTTP_CLENT_EVENT_FINISH BIT1
#define HTTP_CLIENT_EVENT_ON_CONNECT BIT2
#define HTTP_CLIENT_EVENT_ON_DATA BIT3
#define HTTP_CLIENT_EVENT_REDIRECT BIT4
#define HTTP_CLIENT_EVENT_FINESH BIT5
#define HTTP_CLIENT_EVENT_ON_HEADER BIT6

static bool is_request       = false;   // 标记是否正在重新连接
static int  request_attempts = 0;       // 重新连接尝试次数

// extern const uint8_t bilibili_com_cer_start[] asm("_binary_bilibili_ca_cert_pem_start");
// extern const uint8_t bilibili_com_cer_end[] asm("_binary_bilibili_ca_cert_pem_end");

EventGroupHandle_t HTTP_CLIENT_EVENT;

void http_client_event(esp_http_client_event_t* evt)
{
    ESP_LOGI(CLIENT_TAG, "http client event id:%d", evt->event_id);
    switch (evt->event_id) {
    // 当接收到数据时触发
    case HTTP_EVENT_ON_DATA:
        printf("Received data: %.*s\n", evt->data_len, (char*)evt->data);
        xEventGroupSetBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_ON_DATA);
        break;

    // 当请求完成时触发
    case HTTP_EVENT_ON_FINISH:
        printf("Request finished\n");
        xEventGroupSetBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_FINESH);
        break;

    // 当发生错误时触发
    case HTTP_EVENT_ERROR:
        ESP_LOGE(CLIENT_TAG, "HTTP request error!");
        xEventGroupSetBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_ERROR);
        break;

    // 当与服务器成功建立连接时触发
    case HTTP_EVENT_ON_CONNECTED:
        printf("Connected to server\n");
        xEventGroupSetBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_ON_CONNECT);
        break;

    // 当接收到响应头时触发
    case HTTP_EVENT_ON_HEADER:
        printf("Received header: %s: %s\n", evt->header_key, evt->header_value);
        xEventGroupSetBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_ON_HEADER);
        break;

    // 当发生重定向时触发
    case HTTP_EVENT_REDIRECT:
        printf("Redirecting to: %s\n", evt->header_value);
        xEventGroupSetBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_REDIRECT);
        break;

    // 处理其他事件
    default: break;
    }
}

void http_client_init()
{
    // esp_event_handler_register();
    // esp_http_client_config_t http_client_config = {
    //     .url                   = BILIBILI_API_URL,
    //     .timeout_ms            = HTTP_CLIENT_TIMEOUT,
    //     .event_handler         = http_client_event,
    //     .method                = HTTP_METHOD_GET,
    //     .port                  = 443,
    //     .max_redirection_count = 2,
    //     .auth_type             = HTTP_AUTH_TYPE_NONE,
    //     // .skip_cert_common_name_check = true,
    //     .cert_pem = (const char*)bilibili_com_cer_start,
    // };
    char                     local_response_buffer[2048 + 1] = {0};
    esp_http_client_config_t http_client_config              = {
                     .host          = BILIBILI_API_URL,
                     .path          = "/get",
                     .query         = "esp",
                     .event_handler = http_client_event,
                     .user_data     = local_response_buffer,   // Pass address of local buffer to get response
                     .disable_auto_redirect = true,
    };
    esp_http_client_handle_t http_client = esp_http_client_init(&http_client_config);
    if (http_client == NULL) ESP_LOGE(CLIENT_TAG, "http-client初始化失败!");

    esp_err_t err;
    err = esp_http_client_perform(http_client);
    if (err != ESP_OK) ESP_LOGE(CLIENT_TAG, "数据交互失败！");

    err = esp_http_client_cleanup(http_client);
    if (err != ESP_OK) ESP_LOGE(CLIENT_TAG, "http-client关闭失败！");

    BaseType_t bit;
    bit = xEventGroupGetBits(HTTP_CLIENT_EVENT);
    ESP_LOGI(CLIENT_TAG, "ID:%d; \n", bit);

    // {
    //     if (!is_request)
    //     {
    //         is_request = true;
    //         ESP_LOGE(CLIENT_TAG, "http-client数据交互失败！尝试重新连接...");
    //         if (request_attempts < 4)
    //         {
    //             request_attempts++;
    //             esp_http_client_perform(http_client);
    //             ESP_LOGI(CLIENT_TAG, "正在尝试第%d次连接", request_attempts);
    //             BaseType_t bit = xEventGroupWaitBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_ERROR |
    //             HTTP_CLIENT_EVENT_ON_CONNECT, pdTRUE, pdFALSE, portMAX_DELAY);

    //             if (bit & HTTP_CLIENT_EVENT_ERROR)
    //             {
    //                 ESP_LOGE(CLIENT_TAG, "fail");
    //             }
    //             if (bit & HTTP_CLIENT_EVENT_ON_CONNECT)
    //             {
    //                 ESP_LOGE(CLIENT_TAG, "connect");
    //                 // xEventGroupWaitBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_ON_DATA, pdTRUE,
    //                 pdTRUE, portMAX_DELAY);
    //             }
    //         }
    //         else
    //         {
    //             ESP_LOGE(CLIENT_TAG, "连接超时！");
    //         }
    //         is_request = false;
    //     }
    // }
}

static void run_on_event(esp_event_base_t event_base, int32_t event_id,
                         esp_event_handler_t event_handler, void* event_handler_arg)
{
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(IP, "已成功获取到ip");
        xEventGroupSetBits(HTTP_CLIENT_EVENT, WIFI_GET_IP);
        break;

    case IP_EVENT_STA_LOST_IP:
        ESP_LOGI(IP, "获取ip失败！");
        xEventGroupSetBits(HTTP_CLIENT_EVENT, WIFI_LOSE_IP);
        break;
    };
}

void app_main(void)
{
    esp_err_t err;
    HTTP_CLIENT_EVENT = xEventGroupCreate();
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, run_on_event, NULL);
    nvs_init();
    wifi_sta_model_init();
    err = wifi_connect(SSID, PASSWD, GPIO_NUM_2);
    if (err != ESP_OK) {
        ESP_LOGE("INFO", "我完蛋了！");
        return;
    }
    xEventGroupWaitBits(HTTP_CLIENT_EVENT, WIFI_GET_IP, pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
    http_client_init();
}
