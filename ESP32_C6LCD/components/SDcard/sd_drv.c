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


void SD_Init()
{
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and formatted in case
    // when mounting fails.  false true
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true, .max_files = 5, .allocation_unit_size = 16 * 1024};
    sdmmc_card_t* card;
    const char    mount_point[] = MOUNT_POINT;
    ESP_LOGI("SD_INFO", "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing production
    // applications.
    ESP_LOGI("SD_INFO", "Using SPI peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = GPIO_NUM_6,
        .miso_io_num     = GPIO_NUM_5,
        .sclk_io_num     = GPIO_NUM_7,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE("SD_INFO", "Failed to initialize SPI bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs               = SD_CS;
    slot_config.host_id               = host.slot;

    ESP_LOGI("SD_INFO", "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("SD_INFO",
                     "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the "
                     "CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else {
            ESP_LOGE("SD_INFO",
                     "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI("SD_INFO", "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    sd_size = ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024 * 1024);
    ESP_LOGI("SD_INFO", "SD卡大小: %ld GB", sd_size);
}