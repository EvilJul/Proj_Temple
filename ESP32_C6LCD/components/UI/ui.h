#ifndef _UI_H_
#define _UI_H_

// 自定义结构体来扩展动画信息，添加旧屏幕指针
typedef struct
{
    lv_anim_t base_anim;
    lv_obj_t* prev_screen;
} custom_anim_t;

// 用于返回主屏幕CPU、FPS值
typedef struct
{
    lv_obj_t* CPU_ARC;
    lv_obj_t* FPS_ARC;
} HOMEPAGE_ARC_HEAD;

/**
 * @brief 更新进度条
 * @param start_value 起始值
 * @param end_value 结束值
 * @param name 进度加载名称
 */
void splash_anim_update(int32_t start_value, int32_t end_value, char* name);

/**
 * @brief 开机加载动画
 * @return 开机画面屏幕指针
 */
lv_obj_t* splash_anim();

/**
 * @brief 加载主页
 */
HOMEPAGE_ARC_HEAD homePagee();

/**
 * @brief 环形动画生成
 * @param screen 父屏幕
 * @param value 刷新值
 */
void create_arc_anim(lv_obj_t* screen, int value);

/**
 * @brief 同步时间服务器
 */
esp_err_t syn_time();

/**
 * @brief 初始化snt事件服务器
 */
void sntp_init_zh();

//  * @brief FPS值计算
//  */
// void count_fps_value();

// /**
//  * @brief 更新fps值
//  */
// void fps_update();

// /**
//  * @brief 创建一个fps显示区域
//  * @param show_scree 传入屏幕指针
//  * @return fps label指针
//  */
// lv_obj_t* create_fps_area(lv_obj_t* show_scree);

#endif