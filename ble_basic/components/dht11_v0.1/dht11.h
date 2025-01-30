#ifndef DHT11_H
#define DHT11_H

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct
{
    float temperature;
    float humidity;
} dht11_data_t;

/**
 * @brief 初始化dht11传感器
 * @param gpio_num dht11引脚
 * @return 无
 */
void dht11_init(gpio_num_t gpio_num);

/**
 * @brief 读取dht11温湿度数据
 * @param data 传入参数，用于将数据装到结构体中
 * @return ESP_OK
 */
esp_err_t dht11_read(dht11_data_t* data);

#endif   // DHT11_H
