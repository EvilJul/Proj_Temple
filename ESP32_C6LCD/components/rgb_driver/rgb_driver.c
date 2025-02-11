#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "rgb_driver.h"
#include <math.h>
#include <time.h>

led_strip_handle_t led_strip;

led_strip_handle_t setup_leds(RGB_t* config)
{
    esp_err_t err;

    led_strip_config_t strip_config = {
        .strip_gpio_num         = config->pin_rgb,
        .max_leds               = config->rgb_num,
        .led_model              = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
        .flags =
            {
                .invert_out = false,   // don't invert the output signal
            },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = 40 * 1000 * 1000,   // RMT counter clock frequency: 10MHz
        .mem_block_symbols = 64,   // the memory size of each RMT channel, in words (bytes)
        .flags =
            {
                .with_dma = false,
            },
    };

    err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (err != ESP_OK) {
        ESP_LOGE("RGB_INFO", "LED_STRIP新建RMT设备失败！");
        return NULL;
    }

    return led_strip;
}

void random_rgb(RGB_t* config)
{
    srand((unsigned int)time(NULL));
    int R, G, B;
    while (1) {
        for (int i = 0; i < config->rgb_num; i++) {
            R = rand() % 256;
            G = rand() % 256;
            B = rand() % 256;
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, R, G, B));
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void breath_effect(uint8_t ticks, uint8_t max_brightness, RGB_t* config)
{
    const uint8_t _period         = ticks;            // 速度
    const uint8_t _max_brightness = max_brightness;   // 亮度

    while (1) {
        for (double t = 0.0;; t += 0.01) {
            uint8_t brightness_r =
                (uint8_t)((_max_brightness / 2) * sin(0.5 * t) + (_max_brightness / 2));
            uint8_t brightness_g =
                (uint8_t)((_max_brightness / 2) * sin(t) + (_max_brightness / 2));
            uint8_t brightness_b =
                (uint8_t)((_max_brightness / 2) * sin(1.5 * t) + (_max_brightness / 2));

            // set all LEDs to the same color and brightness
            for (int i = 0; i < config->rgb_num; i++) {
                ESP_ERROR_CHECK(
                    led_strip_set_pixel(led_strip, i, brightness_r, brightness_g, brightness_b));
            }

            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
            vTaskDelay(pdMS_TO_TICKS(_period / 10));   // delay between each update
        }
    }
}
