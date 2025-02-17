/*
LVGL移植步骤：
1、初始化注册LVGL显示驱动
2、初始化注册LVGL触摸驱动
3、初始化ST7789硬件接口
4、初始化CST816T硬件接口
5、提供一个定时器给LVGL
*/

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl/lvgl.h"
#include "lvgl_drv.h"

// 定义个全局显示变量
static lv_disp_drv_t         lv_disp_drv;
static ledc_channel_config_t ledc_channel;
esp_lcd_panel_handle_t       panel_handle = NULL;

//**************************************初始化SPI总线***********************************************************************

/**
 * @brief 初始化SPI总线
 */
esp_err_t init_spi_bus(ST7789_t* cfg)
{
    esp_err_t err = ESP_OK;
    // 配置SPI总线
    ESP_LOGI("SPI_INFO", "----->配置SPI总线<------");
    spi_bus_config_t bus_cfg = {
        .sclk_io_num     = cfg->LCD_CLK_NUM,
        .mosi_io_num     = cfg->LCD_MOSI_NUM,
        .miso_io_num     = cfg->LCD_MISO_NUM,
        .quadwp_io_num   = cfg->QUADWP_IO_NUM,
        .quadhd_io_num   = cfg->QUADHD_IO_NUM,
        .max_transfer_sz = cfg->MAX_TRANSFER_SZ,
    };
    err = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE("SPI_INFO", "初始化LCD SPI总线失败！");
        return err;
    }
    return err;
}

//************************************初始化st7789硬件驱动********************************************************************

/**
 * @brief 颜色转换完成时调用
 */
bool lv_flush_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t* edata,
                 void* user_ctx)
{
    lv_disp_drv_t* disp_drv = (lv_disp_drv_t*)user_ctx;
    lv_disp_flush_ready(disp_drv);   // 通知 LVGL 当前帧的刷新已经完成
    return false;   // 表示不需要进一步处理这个事件。这个返回值通常用于指示事件是否已经被完全处理
}

/**
 * @brief 初始化st7789硬件驱动
 */
esp_err_t st7789_hw_init(ST7789_t* cfg)
{
    esp_err_t err = ESP_OK;

    // 配置LCD面板IO
    ESP_LOGI("LCD_PANEL", "----->配置LCD面板IO<------");
    esp_lcd_panel_io_handle_t     lcd_panel_io      = NULL;
    esp_lcd_panel_io_spi_config_t lcd_panel_spi_cfg = {
        .dc_gpio_num         = cfg->LCD_DC_NUM,
        .cs_gpio_num         = cfg->LCD_CS_NUM,
        .pclk_hz             = cfg->PCLK_HZ,
        .lcd_cmd_bits        = cfg->LCD_CMD_BITS,
        .lcd_param_bits      = cfg->LCD_PARAM_BITS,
        .spi_mode            = 0,
        .trans_queue_depth   = 10,
        .on_color_trans_done = lv_flush_cb,   // 颜色转换完成时调用
        .user_ctx = &lv_disp_drv,   // 这个参数是用户定义的上下文，可以传递给on_color_trans_done
    };
    err = esp_lcd_new_panel_io_spi(SPI2_HOST, &lcd_panel_spi_cfg, &lcd_panel_io);
    if (err != ESP_OK) {
        ESP_LOGE("LCD_PANEL", "配置LCD IO面板失败！");
        return err;
    }

    // 初始化ST7789画板
    ESP_LOGI("ST7789_INFP", "----->初始化ST7789画板<------");
    // esp_lcd_panel_handle_t     panel_handle  = NULL;
    esp_lcd_panel_dev_config_t panel_dev_cfg = {
        .reset_gpio_num = GPIO_NUM_21,               // 复位引脚
        .color_space    = ESP_LCD_COLOR_SPACE_RGB,   // 颜色空间类型
        .bits_per_pixel = 16,                        // 每个像素位数
    };
    err = esp_lcd_new_panel_st7789(lcd_panel_io, &panel_dev_cfg, &panel_handle);
    if (err != ESP_OK) {
        ESP_LOGE("ST7789_INFP", "初始化ST7789画板失败！");
        return err;
    }

    // 开启背光
    BK_Init(cfg);
    BK_Light(88);

    // 初始化面板显示区域
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true);   // 如果需要反转颜色
    esp_lcd_panel_disp_on_off(panel_handle, true);    // 打开显示

    return err;
}

//****************************************显示驱动厨初始化*********************************************************************

/**
 * @brief 写入数据到显示芯片的函数
 */
void disp_flush(struct _lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p)
{
    // esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)disp_drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    esp_lcd_panel_draw_bitmap(panel_handle,
                              offsetx1 + OFFSET_X,
                              offsety1 + OFFSET_Y,
                              offsetx2 + OFFSET_X + 1,
                              offsety2 + OFFSET_Y + 1,
                              color_p);
}

/**
 * @brief 显示驱动厨初始化
 */
esp_err_t lv_disp_init()
{
    esp_err_t                 err = ESP_OK;
    static lv_disp_draw_buf_t lv_draw_buf;   // 定义lv显示缓存

    // 定义内存区域大小
    // sizeof(lv_color_t)-->一个像素的资大小
    // MALLOC_CAP_INTERNAL：表示分配内部内存，通常是芯片内部的 SRAM。
    // MALLOC_CAP_DMA：表示分配的内存支持 DMA（直接内存访问）操作
    lv_color_t* buf1 =
        heap_caps_malloc(LCD_DISP_LEN * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    lv_color_t* buf2 =
        heap_caps_malloc(LCD_DISP_LEN * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (buf1 == NULL || buf2 == NULL) {
        ESP_LOGE("LVGL_INFO", "分配缓存空间失败！");
        return -1;
    }
    lv_disp_draw_buf_init(&lv_draw_buf, buf1, buf2, LCD_DISP_LEN);

    // 填充display driver成员
    lv_disp_drv_init(&lv_disp_drv);
    lv_disp_drv.hor_res  = LCD_HOR;      // 水平宽度
    lv_disp_drv.ver_res  = LCD_VER;      // 垂直宽度
    lv_disp_drv.flush_cb = disp_flush;   // 写入数据到显示芯片的函数
    lv_disp_drv.draw_buf = &lv_draw_buf;
    lv_disp_drv_register(&lv_disp_drv);

    return err;
}

//************************************提供一个定时器给LVGL***********************************************************************

/**
 * @brief LVGL定时器回调函数
 */
void lv_timer_cb(void* arg)
{
    uint32_t tick_interval = *(uint32_t*)arg;
    lv_tick_inc(tick_interval);
}

/**
 * @brief 提供一个定时器给LVGL
 */
esp_err_t lv_timer()
{
    esp_err_t err = ESP_OK;

    ESP_LOGI("LVGL_TIMER", "----->创建LVGL定时器<------");
    static uint32_t               tick_interval = 5;   // 定义个定时间隔
    const esp_timer_create_args_t arg           = {
                  .name                  = "LVGL Timer",
                  .arg                   = &tick_interval,   // 定时器的间隔
                  .callback              = lv_timer_cb,      // 定时器回调函数
                  .dispatch_method       = ESP_TIMER_TASK,   // 定时器精度
                  .skip_unhandled_events = true,             // 超时没有执行的回调函数跳过
    };
    esp_timer_handle_t timer_handle;
    esp_timer_create(&arg, &timer_handle);   // 创建定时器
    if (err != ESP_OK) {
        ESP_LOGE("LVGL_TIMER", "创建LVGL定时器失败！");
        return err;
    }
    err = esp_timer_start_periodic(timer_handle, tick_interval * 1000);   // 使定时器周期的运行
    if (err != ESP_OK) {
        ESP_LOGE("LVGL_TIMER", "LVGL定时器设置周期运行失败！");
        return err;
    }

    return err;
}

//****************************************初始化cst916t硬件驱动*****************************************************************

/**
 * @brief 初始化cst916t硬件驱动
 */
esp_err_t cst816t_hw_init()
{
    esp_err_t err = ESP_OK;

    return err;
}

//******************************************初始化触摸驱动***********************************************************************

/**
 * @brief 获取触摸坐标(由于这个函数调用比较频繁，可以放到内存中进行调用来提升速度)
 */
void IRAM_ATTR read_callback(struct _lv_indev_drv_t* indev_drv, lv_indev_data_t* data)
{
    //     uint16_t x, y;
    //     int      state;
    //     cst816t_read(&x, &y, &state);
    //     // x,y坐标
    //     data->point.x = x;
    //     data->point.y = y;
    //     data->state   = state;
}

/**
 * @brief 初始化触摸驱动
 */
void lv_indev_ini()
{
    // esp_err_t err = ESP_OK;

    // 定义个lv触摸驱动
    static lv_indev_drv_t lv_indev;
    // 初始化驱动
    lv_indev_drv_init(&lv_indev);
    lv_indev.type    = LV_INDEV_TYPE_POINTER;   // 类型为触摸屏
    lv_indev.read_cb = read_callback;           // 从哪获取触摸坐标
}

//******************************************LCD背光初始化***********************************************************************

/**
 * @brief LCD背光初始化
 */
void BK_Init(ST7789_t* cfg)
{
    ESP_LOGI("LCD_BACKLIGHT", "----->初始化LCD背光<------");
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << cfg->LCD_BL_NUM,
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bk_gpio_config);
    gpio_set_level(cfg->LCD_BL_NUM, 0);   // 关闭背光

    // 配置LEDC
    ledc_timer_config_t ledc_timer = {.duty_resolution = LEDC_TIMER_13_BIT,
                                      .freq_hz         = 5000,
                                      .speed_mode      = LEDC_LOW_SPEED_MODE,
                                      .timer_num       = LEDC_TIMER_0,
                                      .clk_cfg         = LEDC_AUTO_CLK};
    ledc_timer_config(&ledc_timer);

    ledc_channel.channel    = LEDC_CHANNEL_0;
    ledc_channel.duty       = 0;
    ledc_channel.gpio_num   = cfg->LCD_BL_NUM;
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel.timer_sel  = LEDC_TIMER_0;
    ledc_channel_config(&ledc_channel);
    ledc_fade_func_install(0);
}

/**
 * @brief LCD背光亮度调节
 */
void BK_Light(uint8_t Light)
{
    if (Light > 100) Light = 100;
    uint16_t Duty = ((1 << LEDC_TIMER_13_BIT) - 1) - (81 * (100 - Light));
    if (Light == 0) Duty = 0;
    // 设置PWM占空比
    ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, Duty);
    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
}
