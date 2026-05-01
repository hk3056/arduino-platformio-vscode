#include "Arduino.h"
#include "App.h"
#include "HAL.h"
#include "lvgl.h"
#include "lv_port.h"

static void lv_tick_task(void* arg)                  // 定义 LVGL 时基更新回调函数
{
    lv_tick_inc(2);                                  // 向 LVGL 递增 2ms 系统节拍，用于界面刷新与动画计时
}

void setup()                                         // 系统初始化函数，仅在上电后执行一次
{
    lv_init();                                       // 初始化 LVGL 图形库核心
    HAL::HAL_Init();                                 // 初始化硬件抽象层，完成底层硬件配置
    lv_port_init();                                  // 初始化 LVGL 移植接口，完成显示与输入设备注册
    App_Init();                                      // 初始化应用层程序，完成界面及业务逻辑配置

    const esp_timer_create_args_t lvgl_tick_timer_args = {   // 定义 ESP 高精度定时器创建参数
        .callback = &lv_tick_task,                            // 指定定时器回调函数为 LVGL 时基更新函数
        .arg = nullptr,                                       // 回调函数不传入附加参数
        .dispatch_method = ESP_TIMER_TASK,                    // 采用任务调度方式执行回调函数
        .name = "lvgl_tick"                                   // 设置定时器名称为 lvgl_tick，便于调试与识别
    };

    static esp_timer_handle_t lvgl_tick_timer = nullptr;      // 定义定时器句柄并初始化为空指针
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer); // 根据配置参数创建高精度软件定时器
    esp_timer_start_periodic(lvgl_tick_timer, 2000);          // 启动周期定时器，每 2000 微秒触发一次，即 2ms
}

void loop()                                           // 主循环函数，系统运行期间持续重复执行
{
    HAL::HAL_Update();                                // 更新硬件状态信息，完成外设数据采集与状态刷新
    vTaskDelay(5 / portTICK_PERIOD_MS);               // 延时 5ms，降低处理器占用率并让出任务调度时间
    lv_timer_handler();                               // 调用 LVGL 任务处理函数，完成界面刷新、动画更新及事件处理
}