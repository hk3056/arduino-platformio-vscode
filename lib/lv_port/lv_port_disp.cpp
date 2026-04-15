#include "lv_port.h"
#include "lvgl.h"
#include "HAL.h"
#include <Arduino.h>

#define SCREEN_BUFFER_LINES          80
#define SCREEN_BUFFER_SIZE           (CONFIG_SCREEN_HOR_RES * SCREEN_BUFFER_LINES)
#define SCREEN_USE_DOUBLE_BUFFER     0   // 并口同步刷新，先优先用大单缓冲

static void disp_flush_cb(lv_disp_drv_t* disp, const lv_area_t *area, lv_color_t* color_p)
{
    if (area->x2 < area->x1 || area->y2 < area->y1) {
        lv_disp_flush_ready(disp);
        return;
    }

    const lv_coord_t w = area->x2 - area->x1 + 1;
    const lv_coord_t h = area->y2 - area->y1 + 1;

    uint32_t t0 = micros();

    HAL::Display_PushRect(area->x1, area->y1, w, h, (const uint16_t*)color_p);

    uint32_t t1 = micros();

    
    lv_disp_flush_ready(disp);
}

static void disp_monitor_cb(lv_disp_drv_t* disp_drv, uint32_t time, uint32_t px)
{
}

void lv_port_disp_init()
{
    static lv_color_t lv_disp_buf[SCREEN_BUFFER_SIZE];
#if SCREEN_USE_DOUBLE_BUFFER
    static lv_color_t lv_disp_buf2[SCREEN_BUFFER_SIZE];
#endif
    static lv_disp_draw_buf_t disp_buf;

#if SCREEN_USE_DOUBLE_BUFFER
    lv_disp_draw_buf_init(&disp_buf, lv_disp_buf, lv_disp_buf2, SCREEN_BUFFER_SIZE);
#else
    lv_disp_draw_buf_init(&disp_buf, lv_disp_buf, NULL, SCREEN_BUFFER_SIZE);
#endif

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res   = CONFIG_SCREEN_HOR_RES;
    disp_drv.ver_res   = CONFIG_SCREEN_VER_RES;
    disp_drv.flush_cb  = disp_flush_cb;
    disp_drv.monitor_cb = disp_monitor_cb;
    disp_drv.draw_buf  = &disp_buf;
    disp_drv.full_refresh = 0;

    lv_disp_drv_register(&disp_drv);
}
