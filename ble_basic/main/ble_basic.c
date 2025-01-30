#include "ble_app.h"
#include "dht11.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

#define DHT11_GPIO_NUM GPIO_NUM_13
#define LED_GPIO_PIN GPIO_NUM_12

dht11_data_t data;

void mytask1(void* param)
{
    while (1) {
        memset(&data, 0, sizeof(data));
        if (dht11_read(&data)) {
            ble_set_ch2_value((uint16_t)data.temperature & 0xffff);
            ble_set_ch2_value((uint16_t)data.humidity & 0xffff);
            vTaskDelay(pdMS_TO_TICKS(2000));
        };
    }
}

void app_main(void)
{
    // gpio_config_t led_gpio_cfg = {
    //     .mode         = GPIO_MODE_OUTPUT,
    //     .intr_type    = GPIO_INTR_DISABLE,
    //     .pull_up_en   = GPIO_PULLUP_DISABLE,
    //     .pull_down_en = GPIO_PULLDOWN_DISABLE,
    //     .pin_bit_mask = LED_GPIO_PIN,
    // };
    // gpio_config(&led_gpio_cfg);
    gpio_reset_pin(LED_GPIO_PIN);
    gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT);

    dht11_init(DHT11_GPIO_NUM);

    ESP_ERROR_CHECK(nvs_flash_init());
    ble_cfg_net_init();
    xTaskCreatePinnedToCore(mytask1, "mytask", 4096, NULL, 3, NULL, 1);
}
