#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "http_client.h"
#include "lvgl/lvgl.h"
#include "lvgl_drv.h"
#include "ui.h"
#include <assert.h>
#include <string.h>
#include <time.h>

#include "coin.c"
#include "img_bg_2.c"
#include "img_id.c"
#include "img_wifi_1.c"
#include "img_wifi_2.c"
#include "img_wifi_3.c"
#include "img_wifi_4.c"
#include "like.c"
#include "reply.c"
#include "view.c"

#include "font_zh_14_bpp4.c"
#include "font_zh_16_bpp4.c"
#include "font_zh_8_bpp8.c"

#ifdef _WIN32
#    include <windows.h>
#    define my_sleep(ms) Sleep(ms)
#else
#    include <unistd.h>
#    define my_sleep(ms) usleep(ms * 1000)
#endif

typedef struct
{
    lv_obj_t* view_obj;
    lv_obj_t* like_obj;
    lv_obj_t* coin_obj;
    lv_obj_t* reply_obj;
} LV_LABEL_OBJ;

// 全局变量
static lv_obj_t*      splash_scr         = NULL;
static lv_obj_t*      splash_bar         = NULL;
static lv_obj_t*      splash_label       = NULL;
static lv_obj_t*      splash_label_per   = NULL;
static lv_obj_t*      temp_label_for_top = NULL;
static lv_obj_t*      scr_cpu            = NULL;
static lv_obj_t*      scr_fps            = NULL;
static lv_obj_t*      label_cpu          = NULL;
static lv_obj_t*      label_fps          = NULL;
static lv_obj_t*      arc_cpu            = NULL;
static lv_obj_t*      arc_fps            = NULL;
static int            fps                = 0;
static int            cpu                = 0;
static time_t         now                = 0;
struct tm             time_info          = {0};
const int             retry_count        = 10;
static int            wifi_rssi;
static char           temp_rssi[10];
static lv_style_t     splash_font_style;
static lv_style_t     top_font;
static char           time_buf[20];
static JSON_CONV_BL_t response_data_conv;
static LV_LABEL_OBJ   label_timer_cb_obj;

char* init_char_name(char* name)
{
    char* temp = NULL;
    temp       = realloc(temp, 1 + strlen(name));
    if (temp == NULL) return NULL;

    strncpy(temp, name, strlen(name) + 1);

    return temp;
}

// 动画值更新回调函数
void anim_bar_cb(void* var, int32_t v)
{
    assert(splash_bar != NULL);   // 添加断言，若断言失败会终止程序并输出错误信息
    lv_bar_set_value((lv_obj_t*)var, v, LV_ANIM_OFF);
}

/**
 * @brief 设置背景图片
 */
lv_obj_t* bg_img(lv_obj_t* scr, lv_coord_t w, lv_coord_t h, void* src)
{
    // 创建一个图片
    lv_obj_t* img = lv_img_create(scr);
    // 设置图片源
    lv_img_set_src(img, src);
    // 设置图片为大小
    lv_obj_set_size(img, w, h);

    return img;
}

/**
 * @brief 获取开机加载的数据
 */
void get_start_data(JSON_CONV_BL_t data)
{
    response_data_conv = data;
}

/**
 * @brief 更新进度条
 * @param start_value 起始值
 * @param end_value 结束值
 * @param name 进度加载名称
 */
void splash_anim_update(int32_t start_value, int32_t end_value, char* name)
{
    // 更新字体
    char per_value[20];
    sprintf(per_value, "%ld %%", end_value);
    lv_label_set_text(splash_label_per, per_value);
    char* load_name = init_char_name(name);
    lv_label_set_text(splash_label, load_name);


    // 创建一个动画结构体
    static lv_anim_t bar_anim;
    lv_anim_init(&bar_anim);
    lv_anim_set_var(&bar_anim, splash_bar);        // 设置作用对象
    lv_anim_set_exec_cb(&bar_anim, anim_bar_cb);   // 设置动画值更新回调函数
    lv_anim_set_values(&bar_anim,
                       start_value,
                       end_value);       // 设置起始值和结束值(这里的值为x轴的起始坐标和结束坐标)
    lv_anim_set_time(&bar_anim, 2000);   // 设置动画持续时间（ms）
    lv_anim_set_path_cb(&bar_anim, lv_anim_path_ease_in_out);   // 设置缓动曲线（这里为缓进缓出）
    lv_anim_start(&bar_anim);                                   // 启动动画

    while (lv_anim_count_running() > 0) {
        // lv_task_handler();
        lv_tick_inc(10);   // lvgl系统时钟滴答计数
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief 开机加载动画
 * @return 开机画面屏幕指针
 */
lv_obj_t* splash_anim()
{
    // 获取屏幕
    splash_scr = lv_obj_create(NULL);
    lv_scr_load(splash_scr);

    // 字体设置
    lv_style_init(&splash_font_style);
    lv_style_set_text_color(&splash_font_style, lv_color_make(255, 255, 255));
    lv_style_set_text_font(&splash_font_style, &ui_font_zh_16_bpp4);

    // 设置背景图片
    lv_obj_set_style_bg_color(splash_scr, lv_color_black(), 0);
    lv_obj_align(bg_img(splash_scr, 126, 126, &ui_img_id_img_png), LV_ALIGN_CENTER, 0, -50);

    // 创建一个bar对象
    splash_bar = lv_bar_create(splash_scr);
    lv_obj_set_size(splash_bar, LCD_HOR, 20);
    lv_obj_align(splash_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_range(splash_bar, 0, 100);
    lv_bar_set_start_value(splash_bar, 0, LV_ANIM_OFF);

    // name
    lv_obj_t* name_label = lv_label_create(splash_scr);
    lv_obj_align(name_label, LV_ALIGN_CENTER, 0, 15);
    lv_label_set_text(name_label, "我是大柴啊");
    lv_obj_set_style_text_color(name_label, lv_color_make(220, 116, 228), 0);
    lv_obj_set_style_text_font(name_label, &ui_font_zh_16_bpp4, 0);

    // 创建一个label对象
    splash_label = lv_label_create(splash_scr);
    lv_obj_align(splash_label, LV_ALIGN_BOTTOM_LEFT, 10, -20);
    lv_label_set_text(splash_label, "Loading...");
    lv_obj_add_style(splash_label, &splash_font_style, 0);
    lv_obj_set_style_text_color(splash_label, lv_color_make(255, 255, 255), 0);

    // 创建一个per_label对象
    splash_label_per = lv_label_create(splash_scr);
    lv_obj_align(splash_label_per, LV_ALIGN_BOTTOM_RIGHT, -10, -20);
    lv_obj_add_style(splash_label_per, &splash_font_style, 0);

    // lv_task_handler();

    return splash_scr;
}

void update_arc_anim(lv_obj_t* arc, int32_t value, bool isfps, char* flag1, char* flag2,
                     lv_obj_t* label)
{
    lv_color_t color;
    lv_arc_set_value(arc, value);

    if (value < lv_arc_get_max_value(arc) / 3)
        color = isfps ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN);
    else if (value < (lv_arc_get_max_value(arc) / 3) * 2)
        color = lv_palette_main(LV_PALETTE_YELLOW);
    else
        color = isfps ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED);
    lv_obj_set_style_arc_color(arc, color, 0);

    char str[10];
    sprintf(str, "%s\n %ld%s", flag1, value, flag2);
    lv_label_set_text(label, str);
}

// FPS ARC动画
void fps_arc_anim(void* var, int32_t value)
{
    // ESP_LOGE("DEBUG", "回调函数接收到的FPS值: %ld", value);
    lv_obj_t* arc = (lv_obj_t*)var;
    update_arc_anim(arc, fps, true, "FPS", "Hz", label_fps);
}

// CPU ARC动画
void cpu_arc_anim(void* var, int32_t value)
{
    // ESP_LOGE("DEBUG", "回调函数接收到的CPU值: %ld", value);
    lv_obj_t* arc = (lv_obj_t*)var;
    update_arc_anim(arc, cpu, false, "CPU", "%", label_cpu);
}

/**
 * @brief 环形动画生成
 * @param screen 父屏幕
 * @param value 刷新值
 */
void create_arc_anim(lv_obj_t* screen, int value)
{
    lv_obj_t*          arc     = NULL;
    lv_obj_t*          label   = NULL;
    lv_anim_exec_xcb_t anim_cb = NULL;
    char               str[10];
    int                maxValue = 0;

    if (screen == scr_cpu) {
        sprintf(str, "CPU\n %d%%", value);
        if (arc_cpu == NULL) {
            arc_cpu = lv_arc_create(screen);
            lv_obj_remove_style(arc_cpu, NULL, LV_PART_KNOB);   // 去除手柄
            lv_arc_set_angles(arc_cpu, 0, 180);
            lv_arc_set_value(arc_cpu, 0);   // 设置初始值为 0
            lv_arc_set_range(arc_cpu, 0, 100);
            lv_arc_set_change_rate(arc_cpu, 200);   // 设置弧的速度
            lv_obj_center(arc_cpu);

            label_cpu = lv_label_create(screen);
            lv_obj_center(label_cpu);
            maxValue = 100;
        }
        arc     = arc_cpu;
        label   = label_cpu;
        cpu     = value;
        anim_cb = cpu_arc_anim;
    }

    if (screen == scr_fps) {
        sprintf(str, "FPS\n %dHz", value);
        if (arc_fps == NULL) {
            arc_fps = lv_arc_create(screen);
            lv_obj_remove_style(arc_fps, NULL, LV_PART_KNOB);   // 去除手柄
            lv_arc_set_angles(arc_fps, 0, 180);
            lv_arc_set_value(arc_fps, 0);   // 设置初始值为 0
            lv_arc_set_range(arc_fps, 0, 40);
            lv_arc_set_change_rate(arc_fps, 200);   // 设置弧的速度
            lv_obj_center(arc_fps);

            label_fps = lv_label_create(screen);
            lv_obj_center(label_fps);
            maxValue = 40;
        }
        arc     = arc_fps;
        label   = label_fps;
        fps     = value;
        anim_cb = fps_arc_anim;
    }
    lv_label_set_text(label, str);
    lv_obj_set_style_text_color(label, lv_color_make(255, 255, 255), 0);
    if (value > maxValue) value = maxValue;

    // 自适应单元格大小
    lv_coord_t cell_width  = lv_obj_get_width(screen);
    lv_coord_t cell_height = lv_obj_get_height(screen);
    lv_coord_t arc_size    = (cell_width < cell_height) ? cell_width : cell_height;
    lv_obj_set_size(arc, arc_size, arc_size);
    lv_obj_center(arc);

    static lv_anim_t arc_anim;
    lv_anim_init(&arc_anim);
    lv_anim_set_var(&arc_anim, arc);
    lv_anim_set_exec_cb(&arc_anim, anim_cb);
    uint16_t old = lv_arc_get_value(arc);
    lv_anim_set_time(&arc_anim, 1000);
    lv_anim_set_values(&arc_anim, old, value);
    lv_anim_start(&arc_anim);
}

void cpu_arc_update(struct _lv_timer_t* var)
{
    cpu = 100 - lv_timer_get_idle();
    create_arc_anim((lv_obj_t*)var->user_data, cpu);
}

void fps_arc_update(struct _lv_timer_t* var)
{
    fps = lv_refr_get_fps_avg();
    create_arc_anim((lv_obj_t*)var->user_data, fps);
}

/**
 * @brief 初始化snt事件服务器
 */
void sntp_init_zh()
{
    setenv("TZ", "CST-8", 1);
    tzset();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);   // 设置为轮询模式
    esp_sntp_setservername(0, "pool.ntp.org");     // 使用 NTP 服务器
    esp_sntp_init();
}

/**
 * @brief 同步时间服务器
 */
esp_err_t syn_time()
{
    esp_err_t err   = ESP_OK;
    int       retry = 0;

    while (time_info.tm_year < (2024 - 1900) && ++retry < retry_count) {
        ESP_LOGI("SNTP", "Waiting for system time to be set... (%d/%d)", retry + 1, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &time_info);
    }
    if (retry == retry_count) {
        err = -1;
        ESP_LOGE("SNTP", "Failed to get time from SNTP server!");
    }
    else {
        ESP_LOGI("SNTP", "Time synchronized successfully!");
    }

    return err;
}

/**
 * @brief 定时更新时间
 */
void update_time(struct _lv_timer_t* t)
{
    temp_label_for_top = (lv_obj_t*)t->user_data;
    time(&now);
    localtime_r(&now, &time_info);
    esp_wifi_sta_get_rssi(&wifi_rssi);
    strftime(time_buf, sizeof(time_buf), "%Y/%m/%d\n%H:%M:%S", &time_info);
    sprintf(temp_rssi, "%d", wifi_rssi);
    lv_label_set_text(temp_label_for_top, temp_rssi);
    lv_label_set_text(temp_label_for_top, time_buf);
    memset(temp_rssi, 0, sizeof(temp_rssi));
}

/**
 * @brief 定时发送http请求
 */
void http_get_data(struct _lv_timer_t* var)
{
    char* response_data = http_client_init_get(URL);
    if (response_data != NULL) {
        memset(&response_data_conv, 0, sizeof(response_data_conv));
        response_data_conv = bl_json_data_conversion(response_data);
    }

    char* text_view  = NULL;
    char* text_coin  = NULL;
    char* text_like  = NULL;
    char* text_reply = NULL;

    asprintf(&text_view, "VIEW:\n%d", response_data_conv.view);
    asprintf(&text_coin, "COIN:\n%d", response_data_conv.coin);
    asprintf(&text_like, "LIKE:\n%d", response_data_conv.like);
    asprintf(&text_reply, "REPLY:\n%d", response_data_conv.reply);

    lv_label_set_text(label_timer_cb_obj.view_obj, text_view);
    lv_label_set_text(label_timer_cb_obj.coin_obj, text_coin);
    lv_label_set_text(label_timer_cb_obj.like_obj, text_like);
    lv_label_set_text(label_timer_cb_obj.reply_obj, text_reply);

    free(text_reply);
    free(text_like);
    free(text_coin);
    free(text_view);
}

/**
 * @brief 加载主页
 */
HOMEPAGE_ARC_HEAD homePagee()
{
    HOMEPAGE_ARC_HEAD arc_head;
    arc_head.CPU_ARC = NULL;
    arc_head.FPS_ARC = NULL;

    // 创建一个样式，用于设置透明背景
    static lv_style_t style_table;
    lv_style_init(&style_table);
    // 设置背景透明度为完全透明
    lv_style_set_bg_opa(&style_table, LV_OPA_TRANSP);
    // 设置边框宽度为 0
    lv_style_set_border_width(&style_table, 0);

    lv_obj_t* grid = lv_obj_create(NULL);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);   // 禁止屏幕滚动
    lv_obj_set_style_bg_img_src(grid, &ui_img_bg_2_png, 0);
    // lv_obj_set_style_bg_color(grid, lv_color_hex3(0xffe), 0);
    // 定义网格布局行列描述
    /*
        1.LV_GRID_FR(1) 表示每一列或行占据相等的空间。1 表示比例，这里两列和两行各占一半的空间。
        2.LV_GRID_TEMPLATE_LAST 是一个特殊的标记，表示描述符数组的结束。
    */
    static lv_coord_t col_dsc[] = {172, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {40, 180, 80, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);             // 将网格对象的布局设置为网格布局。
    lv_obj_set_size(grid, LV_HOR_RES, LV_VER_RES);       // 设置网格对象的大小为屏幕的宽度和高度
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);   // 将之前定义的列和行描述符应用到网格对象上
    lv_obj_set_style_pad_all(grid, 0, 0);

    // Top
    lv_obj_t* Top = lv_obj_create(grid);
    lv_obj_set_grid_cell(Top, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_bg_color(Top, lv_color_black(), 0);
    lv_obj_set_style_pad_all(Top, 0, 0);
    // 去除边框
    lv_style_t top_table;
    lv_style_init(&top_table);
    lv_style_set_border_width(&top_table, 0);
    lv_obj_add_style(Top, &top_table, 0);
    // 设置字体为白色
    lv_style_init(&top_font);
    lv_style_set_text_color(&top_font, lv_color_white());
    lv_style_set_text_opa(&top_font, LV_OPA_COVER);
    // 添加label->时间
    strftime(time_buf, sizeof(time_buf), "%Y/%m/%d\n%H:%M:%S", &time_info);
    lv_obj_t* time_label = lv_label_create(Top);
    lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 2, 0);
    lv_label_set_text(time_label, time_buf);
    lv_obj_add_style(time_label, &top_font, 0);
    lv_obj_move_foreground(time_label);
    // 添加Wi-Fi rssi
    lv_obj_t* rssi_label = lv_label_create(Top);
    lv_obj_align(rssi_label, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_style(rssi_label, &top_font, 0);
    esp_wifi_sta_get_rssi(&wifi_rssi);
    sprintf(temp_rssi, "%d", wifi_rssi);
    lv_label_set_text(rssi_label, temp_rssi);
    lv_timer_create(update_time, 1000, time_label);
    memset(temp_rssi, 0, sizeof(temp_rssi));
    lv_obj_align(bg_img(Top, 20, 20, &ui_img_wifi_1_png_data), LV_ALIGN_RIGHT_MID, -20, -3);

    // Media
    lv_obj_t* Media = lv_obj_create(grid);
    lv_obj_set_grid_cell(Media, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_pad_all(Media, 0, 0);
    lv_obj_add_style(Media, &style_table, 0);
    // 制表
    static lv_coord_t media_col[] = {168, LV_GRID_TEMPLATE_LAST};       // 列
    static lv_coord_t media_row[] = {30, 140, LV_GRID_TEMPLATE_LAST};   // 行
    lv_obj_set_layout(Media, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(Media, media_col, media_row);
    lv_obj_set_size(Media, 170, 170);
    //  ID
    lv_obj_t* bili_name_scr = lv_obj_create(Media);
    lv_obj_set_grid_cell(bili_name_scr, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_pad_all(bili_name_scr, 0, 0);
    lv_obj_set_style_bg_color(bili_name_scr, lv_palette_main(LV_PALETTE_ORANGE), 0);
    //-ID LABEL
    lv_obj_t* name_label = lv_label_create(bili_name_scr);
    lv_obj_align(name_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(name_label, "我是大柴啊");
    lv_obj_set_style_text_font(name_label, &ui_font_zh_16_bpp4, 0);
    lv_obj_move_foreground(name_label);

    // Style
    lv_style_t data_font_style;
    lv_style_init(&data_font_style);
    lv_style_set_text_font(&data_font_style, &ui_font_zh_16_bpp4);
    lv_style_set_text_color(&data_font_style, lv_color_make(209, 142, 109));
    // DATA
    lv_obj_t* video_data = lv_obj_create(Media);
    lv_obj_set_grid_cell(video_data, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_pad_all(video_data, 0, 0);
    lv_obj_add_style(video_data, &style_table, 0);
    //-DATA TABLE
    static lv_coord_t video_col[] = {82, 82, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t video_row[] = {68, 68, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(video_data, LV_LAYOUT_GRID);
    lv_obj_set_size(video_data, 166, 138);
    lv_obj_set_grid_dsc_array(video_data, video_col, video_row);
    lv_obj_set_style_pad_all(video_data, 0, 0);
    lv_obj_set_style_pad_gap(video_data, 0, 0);
    // 处理开机请求数据
    char* text_view  = NULL;
    char* text_coin  = NULL;
    char* text_like  = NULL;
    char* text_reply = NULL;
    asprintf(&text_view, "观看:\n%d", response_data_conv.view);
    asprintf(&text_coin, "投币:\n%d", response_data_conv.coin);
    asprintf(&text_like, "点赞:\n%d", response_data_conv.like);
    asprintf(&text_reply, "回复:\n%d", response_data_conv.reply);
    //--VIEW
    lv_obj_t* view_scr = lv_obj_create(video_data);
    lv_obj_set_grid_cell(view_scr, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_pad_all(view_scr, 0, 0);
    lv_obj_align(bg_img(view_scr, 50, 50, &view), LV_ALIGN_TOP_MID, 0, -10);
    //---VIEW LVABEL
    lv_obj_t* view_balel = lv_label_create(view_scr);
    lv_label_set_text(view_balel, text_view);
    lv_obj_align(view_balel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_style(view_scr, &style_table, LV_STATE_DEFAULT);
    lv_obj_add_style(view_balel, &data_font_style, LV_STATE_ANY);
    //--LIKE
    lv_obj_t* like_scr = lv_obj_create(video_data);
    lv_obj_set_grid_cell(like_scr, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_pad_all(like_scr, 0, 0);
    lv_obj_align(bg_img(like_scr, 50, 50, &like), LV_ALIGN_TOP_MID, 0, -10);
    //---LIKE LVABEL
    lv_obj_t* like_balel = lv_label_create(like_scr);
    lv_label_set_text(like_balel, text_like);
    lv_obj_align(like_balel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_style(like_scr, &style_table, LV_STATE_DEFAULT);
    lv_obj_add_style(view_balel, &data_font_style, LV_STATE_DEFAULT);
    //--COIN
    lv_obj_t* coin_scr = lv_obj_create(video_data);
    lv_obj_set_grid_cell(coin_scr, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_align(bg_img(coin_scr, 50, 50, &coin), LV_ALIGN_TOP_MID, 0, -10);
    lv_obj_set_style_pad_all(coin_scr, 0, 0);
    //---COIN LVABEL
    lv_obj_t* coin_balel = lv_label_create(coin_scr);
    lv_label_set_text(coin_balel, text_coin);
    lv_obj_align(coin_balel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_style(coin_scr, &style_table, LV_STATE_DEFAULT);
    lv_obj_add_style(view_balel, &data_font_style, LV_STATE_DEFAULT);
    //--REPLY
    lv_obj_t* reply_scr = lv_obj_create(video_data);
    lv_obj_set_grid_cell(reply_scr, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_pad_all(reply_scr, 0, 0);
    lv_obj_align(bg_img(reply_scr, 50, 50, &reply), LV_ALIGN_TOP_MID, 0, -10);
    //---REPLY LVABEL
    lv_obj_t* reply_balel = lv_label_create(reply_scr);
    lv_label_set_text(reply_balel, text_reply);
    lv_obj_align(reply_balel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_style(reply_scr, &style_table, LV_STATE_DEFAULT);
    lv_obj_add_style(view_balel, &data_font_style, LV_STATE_DEFAULT);
    // 创建定时函数
    label_timer_cb_obj.coin_obj  = coin_balel;
    label_timer_cb_obj.like_obj  = like_balel;
    label_timer_cb_obj.reply_obj = reply_balel;
    label_timer_cb_obj.view_obj  = view_balel;
    lv_timer_create(http_get_data, 1000 * 60 * 15, NULL);
    free(text_reply);
    free(text_like);
    free(text_coin);
    free(text_view);

    // Botton
    lv_obj_t* Botton = lv_obj_create(grid);
    lv_obj_set_grid_cell(Botton, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
    lv_obj_set_style_pad_all(Botton, 0, 0);
    lv_obj_add_style(Botton, &style_table, 0);

    lv_obj_t* bot_scr = lv_obj_create(Botton);
    lv_obj_clear_flag(bot_scr, LV_OBJ_FLAG_SCROLLABLE);
    static lv_coord_t bot_col[] = {80, 80, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t bot_row[] = {70, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(bot_scr, LV_LAYOUT_GRID);
    lv_obj_set_size(bot_scr, 168, 78);
    lv_obj_set_grid_dsc_array(bot_scr, bot_col, bot_row);
    lv_obj_set_style_pad_all(bot_scr, 0, 0);
    lv_obj_add_style(bot_scr, &style_table, 0);

    fps = lv_refr_get_fps_avg();
    cpu = 100 - lv_timer_get_idle();

    //-CPU
    scr_cpu = lv_obj_create(bot_scr);
    lv_obj_set_grid_cell(scr_cpu, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_add_style(scr_cpu, &style_table, 0);
    lv_obj_set_style_pad_all(scr_cpu, 0, 0);
    create_arc_anim(scr_cpu, cpu);

    //-FPS
    scr_fps = lv_obj_create(bot_scr);
    lv_obj_set_grid_cell(scr_fps, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_add_style(scr_fps, &style_table, 0);
    lv_obj_set_style_pad_all(scr_fps, 0, 0);
    create_arc_anim(scr_fps, fps);

    uint32_t time_ms = 1000;   // 动画持续时间，单位为毫秒
    lv_scr_load_anim(grid, LV_SCR_LOAD_ANIM_OVER_LEFT, time_ms, 0, true);
    lv_timer_create(cpu_arc_update, 1000, scr_cpu);
    lv_timer_create(fps_arc_update, 1000, scr_fps);

    arc_head.CPU_ARC = scr_cpu;
    arc_head.FPS_ARC = scr_fps;

    return arc_head;
}
