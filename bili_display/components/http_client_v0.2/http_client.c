#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "http_client.h"

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
extern const uint8_t bilibili_pem_start[] asm("_binary_bilibili_ca_cert_pem_start");
extern const uint8_t bilibili_pem_end[] asm("_binary_bilibili_ca_cert_pem_end");
static bool          is_request       = false;   // 标记是否正在重新连接
static int           request_attempts = 0;       // 重新连接尝试次数
static char*         http_data_buf    = NULL;    // 动态分配的缓冲区
static size_t        http_data_len    = 0;       // 当前数据长度

// 事件处理
void http_client_event(esp_http_client_event_t* evt)
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
        // printf("Received data: %s\n", http_data_buf);
        break;

    // 当请求完成时触发
    case HTTP_EVENT_ON_FINISH:
        printf("Request finished\n");
        xEventGroupSetBits(HTTP_CLIENT_EVENT, HTTP_CLIENT_EVENT_FINESH);
        printf("finifsh data: %s\n", http_data_buf);
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
}

// https初始化&GET请求
char* http_client_init_get(char* url)
{
    if (HTTP_CLIENT_EVENT == NULL) HTTP_CLIENT_EVENT = xEventGroupCreate();

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

    // 清空缓冲区
    http_data_buf = malloc(1);   // 初始化为空字符串
    if (http_data_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
        return NULL;
    }
    http_data_buf[0] = '\0';
    http_data_len    = 0;

    while (request_attempts < 4) {
        esp_http_client_handle_t http_client = esp_http_client_init(&http_client_config);
        if (http_client == NULL) {
            ESP_LOGE(TAG, "http-client初始化失败!");
            return NULL;
        }

        esp_err_t err = esp_http_client_perform(http_client);
        if (err == ESP_OK) {
            // 请求成功
            esp_http_client_cleanup(http_client);
            return http_data_buf;   // 返回接收到的数据
        }
        else {
            // 请求失败
            ESP_LOGE(TAG, "http-client数据交互失败！");
            esp_http_client_cleanup(http_client);
            request_attempts++;
            ESP_LOGI(TAG, "正在尝试第%d次请求", request_attempts);
            vTaskDelay(pdMS_TO_TICKS(1000));   // 等待1秒后重试
        }
    }

    ESP_LOGE(TAG, "请求超时！");
    return NULL;   // 返回NULL表示请求失败
}
