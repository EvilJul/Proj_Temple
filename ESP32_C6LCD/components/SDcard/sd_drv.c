#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_vfs_fat.h"
#include "lvgl_drv.h"
#include "nvs_flash.h"
#include "sd_drv.h"
#include "sdmmc_cmd.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

static uint32_t sd_size;

sdspi_dev_handle_t SD_DEV_HANDLE;
sdmmc_card_t*      SDCARD;

/**
 * @brief 配置SPI
 * @param cfg 和LCD公用引脚参数
 * @return 初始化是否成功，成功返回ESP_OK
 */
esp_err_t init_sd_spi()
{
    esp_err_t err = ESP_OK;

    // ESP_LOGI("SD_INFO", "----->配置SD SPI驱动<------");
    sdmmc_host_t sdmmc_host_ = SDSPI_HOST_DEFAULT();
    sdmmc_host_.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    // // 配置SPI总线
    ESP_LOGI("SPI_INFO", "----->配置SPI总线<------");
    spi_bus_config_t bus_cfg = {
        .sclk_io_num     = GPIO_NUM_7,
        .mosi_io_num     = GPIO_NUM_6,
        .miso_io_num     = GPIO_NUM_5,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4000,
    };
    err = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE("SPI_INFO", "初始化LCD SPI总线失败！");
        return err;
    }

    sdspi_device_config_t sdspi_dev_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    sdspi_dev_cfg.gpio_cs               = SD_CS;
    sdspi_dev_cfg.host_id               = sdmmc_host_.slot;

    // 挂载sd卡
    ESP_LOGI("SD_INFO", "----->进行挂载SD卡<------");
    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .max_files              = 5,
        .format_if_mount_failed = false,
        .allocation_unit_size   = 16 * 1024,
    };
    err = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &sdmmc_host_, &sdspi_dev_cfg, &mount_cfg, &SDCARD);
    if (err != ESP_OK) {
        ESP_LOGE("SD_INFO", "SD挂载失败！");
        return err;
    }
    ESP_LOGI("SD_INFO", "SD挂载成功！挂载点：%s", MOUNT_POINT);

    sdmmc_card_print_info(stdout, SDCARD);
    sd_size = ((uint64_t)SDCARD->csd.capacity) * SDCARD->csd.sector_size / (1024 * 1024 * 1024);
    ESP_LOGI("SD_INFO", "SD卡大小: %ld GB", sd_size);

    return err;
}