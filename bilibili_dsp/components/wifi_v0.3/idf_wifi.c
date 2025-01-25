#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "time.h"
#include <esp_event_loop.h>
#include <string.h>

#define WIFI "WIFI"
#define NVS "NVS"
#define EVENT "WIFI-EVENT"
#define SCAN_START BIT0
#define SCAN_DONE BIT1
#define WIFI_CONNECTED BIT2
#define WIFI_DISCONNECTED BIT3
#define WIFI_RECONNECT_FAIL BIT5
#define FLAG_DONE BIT4
#define WIFI_GPIO_RESET(gpio_num) gpio_reset_pin(gpio_num)
#define WIFI_GPIO_INPUT_MODE(gpio_num) gpio_set_direction(gpio_num, GPIO_MODE_INPUT)
#define WIFI_GPIO_OUTPUT_MODE(gpio_num) gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT)
#define WIFI_GPIO_SET_LOW(gpio_num) gpio_set_level(gpio_num, 0)
#define WIFI_GPIO_SET_HIGH(gpio_num) gpio_set_level(gpio_num, 1)
#define WIFI_GPIO_GET_LEVEL(gpio_num) gpio_get_level(gpio_num)

static bool is_reconnecting    = false;   // 标记是否正在重新连接
static int  reconnect_attempts = 0;       // 重新连接尝试次数
int         flag               = 1;
gpio_num_t  wifi_led_gpio;

EventGroupHandle_t wifi_event_group = NULL;

void printScanAp(wifi_ap_record_t* records, uint16_t ap_num)
{
    printf("%30s %s %s %s\n", "SSID", "频道", "强度", "Mac地址");
    for (int i = 0; i < ap_num; i++) {
        printf("%30s  %3d  %3d  %02X-%02X-%02X-%02X-%02X-%02X\n",
               records[i].ssid,
               records[i].primary,
               records[i].rssi,
               records[i].bssid[0],
               records[i].bssid[1],
               records[i].bssid[2],
               records[i].bssid[3],
               records[i].bssid[4],
               records[i].bssid[5]);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
    xEventGroupSetBits(wifi_event_group, FLAG_DONE);
    fflush(stdout);
}

void run_on_event(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{
    ESP_LOGI("WIFI_RESGINE", "run_on_event current ID:%d", (int)id);
    switch (id) {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(EVENT, "WIFI_EVENT_STA_START");
        xEventGroupSetBits(wifi_event_group, SCAN_START);
        break;

    case WIFI_EVENT_AP_START:
        ESP_LOGI(EVENT, "WIFI_EVENT_AP_START");
        xEventGroupSetBits(wifi_event_group, SCAN_START);
        break;

    case WIFI_EVENT_SCAN_DONE:
        ESP_LOGI(EVENT, "WIFI_EVENT_SCAN_DONE");
        xEventGroupSetBits(wifi_event_group, SCAN_DONE);
        break;

    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(EVENT, "WIFI_EVENT_STA_CONNECTED");
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED);
        ESP_LOGI(WIFI, "Wi-Fi连接成功");
        WIFI_GPIO_RESET(wifi_led_gpio);
        WIFI_GPIO_OUTPUT_MODE(wifi_led_gpio);
        for (uint8_t i = 0; i < 8; i++) {
            WIFI_GPIO_SET_HIGH(wifi_led_gpio);
            vTaskDelay(pdMS_TO_TICKS(300));
            WIFI_GPIO_SET_LOW(wifi_led_gpio);
            vTaskDelay(pdMS_TO_TICKS(300));
            i++;
        }
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        if (!is_reconnecting) {
            is_reconnecting = true;   // 标记为正在重新连接
            xEventGroupSetBits(wifi_event_group, WIFI_DISCONNECTED);

            if (reconnect_attempts < 3) {   // 最大重新连接次数为 3 次
                reconnect_attempts++;       // 增加尝试次数
                ESP_LOGE(WIFI, "Wi-Fi连接断开！尝试重新连接（第 %d 次）。。。", reconnect_attempts);
                ESP_ERROR_CHECK(esp_wifi_connect());

                // 等待连接结果
                BaseType_t bit = xEventGroupWaitBits(wifi_event_group,
                                                     WIFI_CONNECTED | WIFI_DISCONNECTED,
                                                     pdTRUE,
                                                     pdFALSE,
                                                     pdMS_TO_TICKS(5000));

                if (bit & WIFI_CONNECTED) {
                    ESP_LOGI(WIFI, "Wi-Fi重新连接成功！");
                    reconnect_attempts = 0;   // 重置尝试次数
                }
                else {
                    ESP_LOGE(WIFI, "Wi-Fi连接失败!");
                    if (reconnect_attempts >= 3) {
                        ESP_LOGE(WIFI, "已达到最大重新连接次数（3 次），停止尝试！");
                        WIFI_GPIO_RESET(wifi_led_gpio);
                        WIFI_GPIO_OUTPUT_MODE(wifi_led_gpio);
                        for (int i = 0; i < 8; i++) {
                            WIFI_GPIO_SET_HIGH(wifi_led_gpio);
                            vTaskDelay(pdMS_TO_TICKS(800));
                            WIFI_GPIO_SET_LOW(wifi_led_gpio);
                            vTaskDelay(pdMS_TO_TICKS(800));
                        }
                        esp_wifi_stop();
                        xEventGroupSetBits(wifi_event_group, WIFI_RECONNECT_FAIL);
                        // 可以在这里添加其他逻辑，例如进入低功耗模式或重启设备
                    }
                }
            }
            else {
                ESP_LOGE(WIFI, "已达到最大重新连接次数（3 次），停止尝试！");
                xEventGroupSetBits(wifi_event_group, WIFI_RECONNECT_FAIL);
            }

            is_reconnecting = false;   // 无论成功还是失败，都重置标志
        }
        break;

    case IP_EVENT_STA_GOT_IP:
        esp_netif_ip_info_t ip_info;
        esp_netif_t*        sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
            char ip_str[16];
            esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
            ESP_LOGI(WIFI, "IP Address: %s", ip_str);
        }
        else {
            ESP_LOGE(WIFI, "Failed to get IP address");
        }
        break;

        // 未解决打station IP-----------------------------------------------------------------
    case WIFI_EVENT_AP_STACONNECTED:
        ESP_LOGI(WIFI, "WIFI_EVENT_AP_STACONNECTED");
        wifi_sta_list_t sta_list;
        esp_wifi_ap_get_sta_list(&sta_list);

        for (int i = 0; i < sta_list.num; i++) {
            esp_netif_ip_info_t ip_info;
            esp_netif_t*        ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
            if (esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
                char ip_str[16];
                esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
                ESP_LOGI(WIFI, "ESP32的 IP 地址: %s", ip_str);
            };
        };
        break;
    }
}

void nvs_init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(NVS, "NVS Flash初始化失败! 正在擦除NVS Flash...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    };
    ESP_ERROR_CHECK(err);
    wifi_event_group = xEventGroupCreate();
}

const esp_err_t wifi_sta_model_init()
{
    ESP_LOGI(WIFI, "正在初始化tcp/ip适配器");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_config);

    ESP_LOGI(WIFI, "正在配置Wi-Fi模块位STATION模式");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, run_on_event, NULL);
    ESP_LOGI(WIFI, "正在启动Wi-Fi模块");
    esp_err_t err = esp_wifi_start();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(WIFI, "Wi-Fi模块未初始化");
        return ESP_ERR_WIFI_NOT_INIT;
    }
    else if (err == ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGE(WIFI, "Wi-Fi模块未启动");
        return ESP_ERR_WIFI_NOT_STARTED;
    }
    else if (err == ESP_ERR_WIFI_MODE) {
        ESP_LOGE(WIFI, "Wi-Fi模块模式错误");
        return ESP_ERR_WIFI_MODE;
    }
    else if (err == ESP_ERR_WIFI_CONN) {
        ESP_LOGE(WIFI, "Wi-Fi模块内部错误");
        return ESP_ERR_WIFI_CONN;
    }
    return ESP_OK;
}

const esp_err_t wifi_ap_model_init(wifi_ap_config_t wifi_ap_config)
{
    // size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    // ESP_LOGI(WIFI, "可用内存: %d 字节", free_heap);
    ESP_LOGI(WIFI, "正在初始化Wi-Fi模块为AP模式");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL) {
        ESP_LOGE(WIFI, "创建默认Wi-Fi AP网络接口失败");
        return ESP_FAIL;
    }
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_config);

    ESP_LOGI(WIFI, "正在配置Wi-Fi模块");
    wifi_config_t ap_config = {0};
    strncpy((char*)ap_config.ap.ssid, (char*)wifi_ap_config.ssid, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid[sizeof(ap_config.ap.ssid) - 1] = '\0';   // 确保字符串以NULL结尾
    strncpy((char*)ap_config.ap.password,
            (char*)wifi_ap_config.password,
            sizeof(ap_config.ap.password) - 1);
    ap_config.ap.password[sizeof(ap_config.ap.password) - 1] = '\0';   // 确保字符串以NULL结尾
    ap_config.ap.max_connection                              = wifi_ap_config.max_connection;
    ap_config.ap.authmode                                    = wifi_ap_config.authmode;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));

    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, run_on_event, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, run_on_event, NULL);
    ESP_LOGI(WIFI, "正在启动Wi-Fi模块");
    esp_err_t err = esp_wifi_start();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(WIFI, "Wi-Fi模块未初始化");
        return ESP_ERR_WIFI_NOT_INIT;
    }
    else if (err == ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGE(WIFI, "Wi-Fi模块未启动");
        return ESP_ERR_WIFI_NOT_STARTED;
    }
    else if (err == ESP_ERR_WIFI_MODE) {
        ESP_LOGE(WIFI, "Wi-Fi模块模式错误");
        return ESP_ERR_WIFI_MODE;
    }
    else if (err == ESP_ERR_WIFI_CONN) {
        ESP_LOGE(WIFI, "Wi-Fi模块内部错误");
        return ESP_ERR_WIFI_CONN;
    }

    return ESP_OK;
}

esp_err_t wifi_connect(char* ssid, char* password, gpio_num_t gpio)
{
    // 初始化Wi-Fi配置
    wifi_config_t wifi_config = {0};
    // 复制SSID到wifi_config.sta.ssid
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';   // 确保字符串以NULL结尾
    // 复制密码到wifi_config.sta.password
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';   // 确保字符串以NULL结尾

    ESP_LOGI(
        WIFI, "配置Wi-Fi SSID: %s PASSWARD: %s", wifi_config.sta.bssid, wifi_config.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, run_on_event, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, run_on_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, run_on_event, NULL);

    ESP_LOGI(WIFI, "等待Wi-Fi连接");
    xEventGroupWaitBits(wifi_event_group, SCAN_START, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(WIFI, "正在连接Wi-Fi");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_INIT) ESP_LOGE(WIFI, "Wi-Fi模块未初始化");
    if (err == ESP_ERR_WIFI_NOT_STARTED) ESP_LOGE(WIFI, "Wi-Fi模块未启动");
    if (err == ESP_ERR_WIFI_MODE) ESP_LOGE(WIFI, "Wi-Fi模块模式错误");
    if (err == ESP_ERR_WIFI_CONN) ESP_LOGE(WIFI, "Wi-Fi模块内部错误");

    if (gpio >= 0) wifi_led_gpio = gpio;

    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group, WIFI_CONNECTED | WIFI_DISCONNECTED, pdTRUE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED) flag = ESP_OK;
    if (bits & WIFI_DISCONNECTED) {
        BaseType_t re_bit = xEventGroupWaitBits(
            wifi_event_group, WIFI_RECONNECT_FAIL | WIFI_CONNECTED, pdTRUE, pdFALSE, portMAX_DELAY);
        if (re_bit & WIFI_CONNECTED) flag = ESP_OK;
        if (re_bit & WIFI_RECONNECT_FAIL) flag = -1;
    }

    return flag;
}

void showAPs(uint16_t max_ap_number)
{
    esp_err_t err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, run_on_event, NULL);
    xEventGroupWaitBits(wifi_event_group, SCAN_START, pdTRUE, pdFALSE, portMAX_DELAY);
    wifi_country_t scan_config = {
        .cc    = "CN",
        .schan = 1,
        .nchan = 13,
    };
    ESP_ERROR_CHECK(esp_wifi_set_country(&scan_config));
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));

    xEventGroupWaitBits(wifi_event_group, SCAN_DONE, pdFALSE, pdFALSE, portMAX_DELAY);
    uint16_t         ap_num = 0;
    uint16_t         max_ap = max_ap_number;
    wifi_ap_record_t records[max_ap];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));
    ESP_LOGI(WIFI, "周围有%d个AP", ap_num);
    memset(records, 0, sizeof(records));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&max_ap, &records));
    printScanAp(&records, ap_num);
}

void sntp_to_init()
{
    // 设置时区为中国标准时间 (UTC+8)
    setenv("TZ", "CST-8", 1);
    tzset();

    sntp_setoperatingmode(SNTP_OPMODE_POLL);   // 设置为轮询模式
    sntp_setservername(0, "pool.ntp.org");     // 使用 NTP 服务器
    sntp_init();
}

void sntp_updateTime()
{
    // 等待时间同步完成
    time_t    now         = 0;
    struct tm timeinfo    = {0};
    int       retry       = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2024 - 1900) && ++retry < retry_count) {
        ESP_LOGI("SNTP", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry == retry_count) {
        ESP_LOGE("SNTP", "Failed to get time from SNTP server!");
    }
    else {
        ESP_LOGI("SNTP", "Time synchronized successfully!");
    }
}
