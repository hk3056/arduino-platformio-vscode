#include "BluetoothView.h"

using namespace Page;

void BluetoothView::Create(lv_obj_t* root)
{
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x101010), 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t* label = lv_label_create(root);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Bluetooth");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 18, 16);
    ui.labelTitle = label;

    /* Global switch */
    lv_obj_t* sw = lv_switch_create(root);
    lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, -18, 14);
    lv_obj_add_state(sw, LV_STATE_CHECKED);
    ui.swBluetooth = sw;

    label = lv_label_create(root);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x4FA3FF), 0);
    lv_label_set_text(label, "On");
    lv_obj_align_to(label, sw, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    ui.labelState = label;

    /* Connected device card */
    lv_obj_t* cont = lv_obj_create(root);
    lv_obj_set_size(cont, 220, 78);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_set_style_radius(cont, 18, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_white(), 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    ui.contConnected = cont;

    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
    lv_label_set_text(label, "Connected Device");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 14, 8);
    ui.labelConnectedTitle = label;

    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_label_set_text(label, "No connected device");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 14, 28);
    ui.labelConnectedName = label;

    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
    lv_label_set_text(label, "-");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 14, 52);
    ui.labelConnectedInfo = label;

    /* Available title */
    label = lv_label_create(root);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x888888), 0);
    lv_label_set_text(label, "Available Devices");
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 24, 142);
    ui.labelAvailableTitle = label;

    /* Available list container */
    cont = lv_obj_create(root);
    lv_obj_set_size(cont, 220, 122);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 162);
    lv_obj_set_style_radius(cont, 18, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x2A2A2A), 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    ui.contAvailable = cont;

    static const char* deviceNames[3] =
    {
        "Device A",
        "Device B",
        "Device C"
    };

    for (int i = 0; i < 3; i++)
    {
        lv_obj_t* btn = lv_btn_create(cont);
        lv_obj_set_size(btn, 190, 26);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 8 + i * 34);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x3A3A3A), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x4FA3FF), LV_STATE_FOCUSED);
        ui.btnDevice[i] = btn;

        label = lv_label_create(btn);
        lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_17"), 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_label_set_text(label, deviceNames[i]);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 12, 0);
        ui.labelDevice[i] = label;
    }

    /* Exit button */
    lv_obj_t* btn = lv_btn_create(root);
    lv_obj_set_size(btn, 220, 44);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_radius(btn, 16, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E2E2E), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x4FA3FF), LV_STATE_FOCUSED);
    ui.btnExit = btn;

    label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Exit");
    lv_obj_center(label);
    ui.labelExit = label;
}