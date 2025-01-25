#ifndef IDF_WIFI_H
#define IDF_WIFI_H

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_wifi.h"

/**
 * @brief Station模式初始化
 */
const esp_err_t wifi_sta_model_init();

/**
 * @brief Ap模式初始化
 * @param wifi_ap_config ap配置
 * @param wifi_ap_config.ssid 名称
 * @param wifi_ap_config.password 密码
 * @param wifi_ap_config.max_connection 最大连接数
 * @param wifi_ap_config.channel 信道
 */
const esp_err_t wifi_ap_model_init(wifi_ap_config_t wifi_ap_config);

/**
 * @brief 显示当前ap。以下情况无法进行扫描ap：当前扫描被阻止；Wi-Fi已经连接上。
 * @param max_ap_number 最大AP显示数量
 */
void showAPs(uint16_t max_ap_number);

/**
 * @brief 连接Wi-Fi
 * @param ssid Wi-Fi名称
 * @param password Wi-Fi密码
 * @param gpio 用于Wi-Fi连接提示的LED GPIO引脚
 */
esp_err_t wifi_connect(char* ssid, char* password, gpio_num_t gpio);

/**
 * @brief 初始化NVS Flash
 */
void nvs_init();

/**
 * @brief 初始化SNTP
 */
void sntp_to_init();

/**
 * @同步服务器事件
 */
void sntp_updateTime();

#endif