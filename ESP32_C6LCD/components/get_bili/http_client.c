#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "http_client.h"
#include "mbedtls/debug.h"
#include <cJSON.h>
#include <string.h>

#define TAG "HTTP_CLIENT"
#define CLIENT_TIMEOUT 50000
#define MAX_HTTP_DATA_BUF 1024 * 8

#define HTTP_CLIENT_EVENT_ERROR BIT0
#define HTTP_CLIENT_EVENT_FINISH BIT1
#define HTTP_CLIENT_EVENT_ON_CONNECT BIT2
#define HTTP_CLIENT_EVENT_ON_DATA BIT3
#define HTTP_CLIENT_EVENT_REDIRECT BIT4
#define HTTP_CLIENT_EVENT_FINESH BIT5
#define HTTP_CLIENT_EVENT_ON_HEADER BIT6

EventGroupHandle_t HTTP_CLIENT_EVENT = NULL;
// asm根据自己证书名字进行更改bilibili_ca_cert_pem-->bilibili.ca.cert.pem
/*
macOS命令行生成官方证书：
    echo -n | openssl s_client -showcerts -connect www.bilibili.com:443 2>/dev/null | sed
-ne'/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' > bilibili.ca.cert.pem

    openssl s_client -connect www.bilibili.com:443 -showcerts </dev/null 2>/dev/null | openssl
x509 -outform PEM > bilibili.ca.cert.pem
    */
extern const uint8_t bilibili_pem_start[] asm("_binary_bilibili_ca_cert_pem_start");
extern const uint8_t bilibili_pem_end[] asm("_binary_bilibili_ca_cert_pem_end");
static int           request_attempts = 0;      // 重新连接尝试次数
static char*         http_data_buf    = NULL;   // 动态分配的缓冲区
static size_t        http_data_len    = 0;      // 当前数据长度

// 事件处理
esp_err_t http_client_event(esp_http_client_event_t* evt)
{
    switch (evt->event_id) {
    // 当接收到数据时触发
    case HTTP_EVENT_ON_DATA:
        xEventGroupSetBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_ON_DATA);
        // 动态扩展缓冲区
        http_data_buf = realloc(http_data_buf, http_data_len + evt->data_len + 1);
        if (http_data_buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for response data");
            // return ESP_FAIL;
        }
        // 将新数据追加到缓冲区
        memcpy(http_data_buf + http_data_len, evt->data, evt->data_len);
        http_data_len += evt->data_len;
        http_data_buf[http_data_len] = '\0';   // 添加字符串结束符
        break;

    // 当请求完成时触发
    case HTTP_EVENT_ON_FINISH:
        printf("Request finished\n");
        xEventGroupSetBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_FINESH);
        // printf("finifsh data: %s\n", http_data_buf);
        break;

    // 当发生错误时触发
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP request error!");
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

    return 0;
}

// https初始化&GET请求
char* http_client_init_get(char* url)
{
    if (HTTP_CLIENT_EVENT == NULL) HTTP_CLIENT_EVENT = xEventGroupCreate();
    // ESP_LOGI("DEBUG", "Certificate: %s", (const char*)bilibili_pem_start);
    // ESP_LOGE("DEBUG", "Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    // mbedtls_debug_set_threshold(3);

    esp_http_client_config_t http_client_config = {
        .url                   = url,
        .timeout_ms            = CLIENT_TIMEOUT,
        .event_handler         = http_client_event,
        .method                = HTTP_METHOD_GET,
        .port                  = 443,
        .max_redirection_count = 2,
        .auth_type             = HTTP_AUTH_TYPE_NONE,
        .cert_pem              = (const char*)bilibili_pem_start,
    };
    esp_http_client_handle_t http_client = esp_http_client_init(&http_client_config);
    if (http_client == NULL) {
        ESP_LOGE(TAG, "http-client初始化失败!");
        return NULL;
    }

    // 清空缓冲区
    http_data_buf = malloc(1);   // 初始化为空字符串
    if (http_data_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
        return NULL;
    }
    http_data_buf[0] = '\0';
    http_data_len    = 0;

    while (request_attempts < 4) {
        esp_err_t err = esp_http_client_perform(http_client);
        if (err == ESP_OK) {
            esp_http_client_cleanup(http_client);
            request_attempts = 0;
            return http_data_buf;   // 返回接收到的数据
        }
        else {
            ESP_LOGE(TAG, "http-client数据交互失败！");
            request_attempts++;
            ESP_LOGI(TAG, "正在尝试第%d次请求", request_attempts);
            vTaskDelay(pdMS_TO_TICKS(2000));   // 等待2秒后重试
        }
    }

    esp_http_client_cleanup(http_client);
    request_attempts = 0;
    ESP_LOGE(TAG, "获取数据失败");
    return NULL;   // 返回NULL表示请求失败
}

/**
 * @brief Json 格式数据处理(bilibili)
 * @param json_data传入Json字符串
 * @return 返回处理后的数据
 */
JSON_CONV_BL_t bl_json_data_conversion(char* data)
{
    JSON_CONV_BL_t response_data_canver;
    memset(&response_data_canver, 0, sizeof(response_data_canver));

    cJSON* json_parent = cJSON_Parse(data);
    if (json_parent == NULL) {
        ESP_LOGE("JSON_CONV_INFO", "Error to conversion data:%s \n", cJSON_GetErrorPtr());
        response_data_canver.coin  = 0;
        response_data_canver.like  = 0;
        response_data_canver.view  = 0;
        response_data_canver.reply = 0;
        response_data_canver.title = '\0';
    }
    else {
        ESP_LOGI("JSON_CONVER_INFO", "处理后的JSON数据：\n %s", cJSON_Print(json_parent));
        free(cJSON_Print(json_parent));

        cJSON* bl_data = cJSON_GetObjectItem(json_parent, "data");
        if (bl_data != NULL) {
            cJSON* title               = cJSON_GetObjectItem(bl_data, "title");
            response_data_canver.title = cJSON_Print(title);

            cJSON* stat = cJSON_GetObjectItem(bl_data, "stat");
            if (stat != NULL) {
                response_data_canver.coin = cJSON_GetNumberValue(cJSON_GetObjectItem(stat, "coin"));
                response_data_canver.like = cJSON_GetNumberValue(cJSON_GetObjectItem(stat, "like"));
                response_data_canver.view = cJSON_GetNumberValue(cJSON_GetObjectItem(stat, "view"));
                response_data_canver.reply =
                    cJSON_GetNumberValue(cJSON_GetObjectItem(stat, "reply"));
            }
            else {
                response_data_canver.coin  = 0;
                response_data_canver.like  = 0;
                response_data_canver.view  = 0;
                response_data_canver.reply = 0;
            }
        }
    }

    // 释放内存
    if (http_data_buf) {
        free(http_data_buf);
        http_data_buf = NULL;
        http_data_len = 0;
    }

    return response_data_canver;
}