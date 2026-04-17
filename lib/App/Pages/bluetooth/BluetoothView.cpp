#include "BluetoothView.h"

using namespace Page;

void BluetoothView::Create(lv_obj_t* root)
{
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);

    lv_obj_t* label = lv_label_create(root);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Bluetooth");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 25);
    ui.labelTitle = label;

    label = lv_label_create(root);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x999999), 0);
    lv_label_set_text(label, "Press ENTER / ESC to go back");
    lv_obj_center(label);
    ui.labelHint = label;
}