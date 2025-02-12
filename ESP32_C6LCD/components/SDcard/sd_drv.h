#ifndef _SD_DRV_H_
#    define _SD_SRV_H_

#    include "driver/gpio.h"
#    include "lvgl_drv.h"

#    define MOUNT_POINT "/sdcard"
#    define SD_CS GPIO_NUM_4
// #define SD_MOSI GPIO_NUM_6
// #define SD_MISO GPIO_NUM_5
// #define SD_SCLK GPIO_NUM_7

/**
 * @brief 配置SPI
 * @param cfg 和LCD公用引脚参数
 * @return 初始化是否成功，成功返回ESP_OK
 */
esp_err_t init_sd_spi();


void SD_Init();

#endif