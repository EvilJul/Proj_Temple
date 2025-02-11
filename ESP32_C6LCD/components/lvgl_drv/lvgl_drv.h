#ifndef _LVGL_DRV_H
#define _LVGL_DRV_H

#include "driver/gpio.h"
#include "esp_err.h"

// 定义屏幕宽度高度
#define LCD_HOR 172
#define LCD_VER 320
// 定义显示缓存宽度
#define LCD_DISP_LEN LCD_HOR*(LCD_VER / 6)
//
#define OFFSET_X 34
#define OFFSET_Y 0

/**
 * @brief st7789芯片SPI参数配置
 * @param LCD_CLK_NUM 时钟线的GPIO编号
 * @param LCD_MOSI_NUM 主输出/从输入(Master Output Slave Input)线的GPIO编号
 * @param LCD_MISO_NUM 从输出/主输入(Slave Output, Master Input)线的GPIO编号
 * @param LCD_DC_NUM 数据/命令选择(Data/Command Select)信号的GPIO编号
 * @param LCD_CS_NUM 片选(Chip Select)信号的GPIO编号
 * @param LCD_BL_NUM LCD背光的GPIO编号
 * @param QUADWP_IO_NUM 四位数据宽度模式下的写保护线的GPIO编号
 * @param QUADHD_IO_NUM 四位数据宽度模式下的读保护线的GPIO编号
 * @param MAX_TRANSFER_SZ  SPI总线每次传输的最大字节数
 * @param PCLK_HZ 像素时钟频率
 * @param LCD_CMD_BITS 命令字节的大小
 * @param LCD_PARAM_BITS 参数字节的大小
 */
typedef struct
{
    gpio_num_t   LCD_CLK_NUM;
    gpio_num_t   LCD_MOSI_NUM;
    gpio_num_t   LCD_MISO_NUM;
    gpio_num_t   LCD_DC_NUM;
    gpio_num_t   LCD_CS_NUM;
    gpio_num_t   LCD_BL_NUM;
    int          QUADWP_IO_NUM;
    int          QUADHD_IO_NUM;
    int          MAX_TRANSFER_SZ;
    unsigned int PCLK_HZ;
    int          LCD_CMD_BITS;
    int          LCD_PARAM_BITS;
} ST7789_t;

/**
 * @brief 显示驱动厨初始化
 */
esp_err_t lv_disp_init();

/**
 * @brief 初始化触摸驱动
 */
void lv_indev_ini();

/**
 * @brief 初始化st7789硬件驱动
 */
esp_err_t st7789_hw_init(ST7789_t* cfg);

/**
 * @brief 初始化cst916t硬件驱动
 */
esp_err_t cst816t_hw_init();

/**
 * @brief 提供一个定时器给LVGL
 */
esp_err_t lv_timer();

/**
 * @brief 初始化LCD背光
 */
void BK_Init(ST7789_t* cfg);

/**
 * @brief LCD背光亮度调节
 */
void BK_Light(uint8_t Light);

/**
 * @brief 初始化SPI总线
 * @param cfg 传入参数
 */
esp_err_t init_spi_bus(ST7789_t* cfg);


#endif
