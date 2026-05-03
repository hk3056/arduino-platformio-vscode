#include "BluetoothView.h"

using namespace Page;

void BluetoothView::Create(lv_obj_t* root) {
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x101010), 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t* label = lv_label_create(root);
    lv_obj_set_style_text_font(label,
        ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Bluetooth");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 18, 16);
    ui.labelTitle = label;

    /* 全局开关 */
    lv_obj_t* sw = lv_switch_create(root);
    lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -18, 14);
    lv_obj_add_state(sw, LV_STATE_CHECKED);
    ui.swBluetooth = sw;

    label = lv_label_create(root);
    lv_obj_set_style_text_font(label,
        ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x4FA3FF), 0);
    lv_label_set_text(label, "On");
    lv_obj_align_to(label, sw, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    ui.labelState = label;

    /* 已连接设备卡片 */
    lv_obj_t* cont = lv_obj_create(root);
    lv_obj_set_size(cont, 220, 78);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_set_style_radius(cont, 18, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_white(), 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    ui.contConnected = cont;

    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label,
        ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
    lv_label_set_text(label, "Connected Device");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 14, 8);
    ui.labelConnectedTitle = label;

    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label,
        ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_label_set_text(label, "No connected device");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 14, 28);
    ui.labelConnectedName = label;

    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label,
        ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
    lv_label_set_text(label, "-");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 14, 52);
    ui.labelConnectedInfo = label;

    /* 可用设备标题 */
    label = lv_label_create(root);
    lv_obj_set_style_text_font(label,
        ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x888888), 0);
    lv_label_set_text(label, "Available Devices");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 18, 142);
    ui.labelAvailableTitle = label;

    /* 设备列表容器 */
    cont = lv_obj_create(root);
    lv_obj_set_size(cont, 210, LV_SIZE_CONTENT);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 168);
    lv_obj_set_style_pad_all(cont, 6, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    ui.contAvailable = cont;

    /* 返回按钮 */
    lv_obj_t* btn = lv_btn_create(root);
    lv_obj_set_size(btn, 80, 36);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -18, -18);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x4FA3FF), 0);
    ui.btnExit = btn;

    label = lv_label_create(btn);
    lv_obj_set_style_text_font(label,
        ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Exit");
    lv_obj_center(label);
    ui.labelExit = label;
}