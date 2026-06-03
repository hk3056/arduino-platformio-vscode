#include "BluetoothView.h"

using namespace Page;

static void SetLabelStyle(lv_obj_t* label, uint32_t color, const char* fontName)
{
    lv_obj_set_style_text_font(label, ResourcePool::GetFont(fontName), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
}

static void SetButtonBaseStyle(lv_obj_t* btn)
{
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_border_width(btn, 0, 0);

    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3A3A3A), 0);

    /* 焦点只加白色外框，不改变背景颜色 */
    lv_obj_set_style_outline_width(btn, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(btn, lv_color_hex(0xFFFFFF), LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(btn, 3, LV_STATE_FOCUSED);
}

static lv_obj_t* CreateNameButton(lv_obj_t* root, int y, const char* text)
{
    lv_obj_t* btn = lv_btn_create(root);
    lv_obj_set_size(btn, 190, 40);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);

    SetButtonBaseStyle(btn);

    lv_obj_t* label = lv_label_create(btn);
    SetLabelStyle(label, 0xFFFFFF, "bahnschrift_17");
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return btn;
}

void BluetoothView::Create(lv_obj_t* root)
{
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x101010), 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t* label = lv_label_create(root);
    SetLabelStyle(label, 0xFFFFFF, "bahnschrift_17");
    lv_label_set_text(label, "Bluetooth");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 18, 16);
    ui.labelTitle = label;

    /* 蓝牙总开关 */
    lv_obj_t* sw = lv_switch_create(root);
    lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -18, 14);
    ui.swBluetooth = sw;

    label = lv_label_create(root);
    SetLabelStyle(label, 0x888888, "bahnschrift_13");
    lv_label_set_text(label, "Off");
    lv_obj_align_to(label, sw, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    ui.labelState = label;

    /* 心率标题 */
    label = lv_label_create(root);
    SetLabelStyle(label, 0xFFFFFF, "bahnschrift_17");
    lv_label_set_text(label, "Heart Rate");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 18, 66);
    ui.labelHeartTitle = label;

    /* 心率开关 */
    sw = lv_switch_create(root);
    lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -18, 58);
    ui.swHeartRate = sw;

    /* 心率蓝牙名字按钮 */
    ui.btnHeartRateName = CreateNameButton(root, 98, "XTrack-HR");
    ui.labelHeartRateName = lv_obj_get_child(ui.btnHeartRateName, 0);

    label = lv_label_create(root);
    SetLabelStyle(label, 0x888888, "bahnschrift_13");
    lv_label_set_text(label, "Heart Rate Off");
    lv_obj_align_to(label, ui.btnHeartRateName, LV_ALIGN_OUT_BOTTOM_MID, 0, 7);
    ui.labelHeartRateHint = label;

    /* 踏频标题 */
    label = lv_label_create(root);
    SetLabelStyle(label, 0xFFFFFF, "bahnschrift_17");
    lv_label_set_text(label, "Cadence");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 18, 178);
    ui.labelCadenceTitle = label;

    /* 踏频开关 */
    sw = lv_switch_create(root);
    lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -18, 170);
    ui.swCadence = sw;

    /* 踏频蓝牙名字按钮 */
    ui.btnCadenceName = CreateNameButton(root, 210, "XTrack-CAD");
    ui.labelCadenceName = lv_obj_get_child(ui.btnCadenceName, 0);

    label = lv_label_create(root);
    SetLabelStyle(label, 0x888888, "bahnschrift_13");
    lv_label_set_text(label, "Cadence Off");
    lv_obj_align_to(label, ui.btnCadenceName, LV_ALIGN_OUT_BOTTOM_MID, 0, 7);
    ui.labelCadenceHint = label;

    /* 退出按钮 */
    lv_obj_t* btn = lv_btn_create(root);
    lv_obj_set_size(btn, 150, 38);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -18);

    SetButtonBaseStyle(btn);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E2E2E), 0);

    ui.btnExit = btn;

    label = lv_label_create(btn);
    SetLabelStyle(label, 0xFFFFFF, "bahnschrift_17");
    lv_label_set_text(label, "Exit");
    lv_obj_center(label);
    ui.labelExit = label;
}