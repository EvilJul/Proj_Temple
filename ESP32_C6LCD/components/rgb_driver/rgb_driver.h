#ifndef _RGB_DRIVER_
#define _RGB_DRIVER_

#include "driver/gpio.h"
#include "led_strip_types.h"

/**
 * @param pin_rgb RGB灯珠引脚
 * @param rgb_num RGB灯珠数量
 */
typedef struct
{
    gpio_num_t pin_rgb;
    int        rgb_num;
} RGB_t;

/**
 * @brief 初始化LED配置
 * @param config RGB灯珠基础配置
 * @return success: led_strip_handle_t对象；fail:NULL
 */
led_strip_handle_t setup_leds(RGB_t* config);

/**
 * @brief RGB灯珠随机闪烁
 */
void random_rgb(RGB_t* cfg);

/**
 * @brief RGB呼吸灯闪烁
 * @param strip_handle
 * @param ticks 控制速度
 * @param max_brightness 控制亮度
 */
void breath_effect(uint8_t ticks, uint8_t max_brightness, RGB_t* cfg);

#endif
