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
 * @brief 配置SPI并挂载SD卡(有些问题好像)
 * @return 初始化是否成功，成功返回ESP_OK
 */
esp_err_t init_sd_spi();

/**
 * @brief 配置SPI并挂载SD卡
 * @return 初始化是否成功，成功返回ESP_OK
 */
void SD_Init();

/**
 * @brief 读取SD卡文件
 */
esp_err_t sd_read_file(const char* path);


/**
 * @brief 向SD写入文件
 */
esp_err_t sd_write_file(const char* path, char* data);

#endif