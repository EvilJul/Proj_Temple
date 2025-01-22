#include <stdio.h>
#include "idf_wifi.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WIFI "WIFI"
#define IP "IP_EVENT"
// #define SSID "iPhone"
// #define PASSWD "1234567890"
#define SSID "202"
#define PASSWD "Ti@n!99&"
// #define BILIBILI_API_URL "https://api.bilibili.com/x/web-interface/view?bvid=你的视频BV号"
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

static bool is_request = false;  // 标记是否正在重新连接
static int request_attempts = 0; // 重新连接尝试次数
// DigiCert Global Root CA
static const char *BILIBILI_CA_CERT = "-----BEGIN CERTIFICATE-----\n"
                                      "MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n"
                                      "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
                                      "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n"
                                      "QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n"
                                      "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
                                      "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n"
                                      "9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n"
                                      "CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n"
                                      "nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n"
                                      "43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n"
                                      "T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n"
                                      "gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n"
                                      "BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n"
                                      "TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n"
                                      "DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n"
                                      "hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n"
                                      "06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n"
                                      "PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n"
                                      "YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n"
                                      "CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n"
                                      "-----END CERTIFICATE-----";

EventGroupHandle_t HTTP_CLIENT_EVENT;

void http_client_event(esp_http_client_event_t *evt)
{
    ESP_LOGI(CLIENT_TAG, "http client event id:%d", evt->event_id);
    switch (evt->event_id)
    {
    // 当接收到数据时触发
    case HTTP_EVENT_ON_DATA:
        printf("Received data: %.*s\n", evt->data_len, (char *)evt->data);
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
    default:
        break;
    }
}

void http_client_init()
{
    // esp_event_handler_register();
    esp_http_client_config_t http_client_config = {
        .url = BILIBILI_API_URL,
        .timeout_ms = HTTP_CLIENT_TIMEOUT,
        .event_handler = http_client_event,
        .method = HTTP_METHOD_POST,
        // .skip_cert_common_name_check = true,
        .cert_pem = BILIBILI_CA_CERT,
    };
    esp_http_client_handle_t http_client = esp_http_client_init(&http_client_config);
    if (http_client == NULL)
        ESP_LOGE(CLIENT_TAG, "http-client初始化失败!");

    esp_err_t err;
    err = esp_http_client_perform(http_client);
    if (err != ESP_OK)
        ESP_LOGE(CLIENT_TAG, "数据交互失败！");

    err = esp_http_client_cleanup(http_client);
    if (err != ESP_OK)
        ESP_LOGE(CLIENT_TAG, "http-client关闭失败！");

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
    //             BaseType_t bit = xEventGroupWaitBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_ERROR | HTTP_CLIENT_EVENT_ON_CONNECT, pdTRUE, pdFALSE, portMAX_DELAY);

    //             if (bit & HTTP_CLIENT_EVENT_ERROR)
    //             {
    //                 ESP_LOGE(CLIENT_TAG, "fail");
    //             }
    //             if (bit & HTTP_CLIENT_EVENT_ON_CONNECT)
    //             {
    //                 ESP_LOGE(CLIENT_TAG, "connect");
    //                 // xEventGroupWaitBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_ON_DATA, pdTRUE, pdTRUE, portMAX_DELAY);
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
                         esp_event_handler_t event_handler, void *event_handler_arg)
{
    switch (event_id)
    {
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
    if (err != ESP_OK)
    {
        ESP_LOGE("INFO", "我完蛋了！");
        return;
    }
    xEventGroupWaitBits(HTTP_CLIENT_EVENT, WIFI_GET_IP, pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));
    http_client_init();
}
