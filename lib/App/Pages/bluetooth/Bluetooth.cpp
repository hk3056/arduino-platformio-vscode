#include "Bluetooth.h"
#include "HAL_Bluetooth.h"

using namespace Page;

Bluetooth::Bluetooth()
{
    timer = nullptr;
    deviceBtnCount = 0;

    for (int i = 0; i < MAX_VISIBLE_DEVICES; i++)
    {
        btnDevice[i] = nullptr;
        labelDevice[i] = nullptr;
    }
}

Bluetooth::~Bluetooth()
{
}

void Bluetooth::onCustomAttrConfig()
{
    SetCustomCacheEnable(false);
}

void Bluetooth::onViewLoad()
{
    View.Create(_root);

    AttachEvent(_root);
    AttachEvent(View.ui.swBluetooth);
    AttachEvent(View.ui.btnExit);

    RefreshUI();
}

void Bluetooth::onViewDidLoad()
{
}

void Bluetooth::onViewWillAppear()
{
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_root, lv_color_black(), LV_PART_MAIN);

    lv_indev_t* indev = lv_indev_get_act();
    if (indev)
    {
        lv_indev_wait_release(indev);
    }

    TryStartScan();
    RefreshUI();
}

void Bluetooth::onViewDidAppear()
{
    RebuildGroup();

    if (timer == nullptr)
    {
        timer = lv_timer_create(onTimerUpdate, 300, this);
        lv_timer_ready(timer);
    }
}

void Bluetooth::onViewWillDisappear()
{
    lv_indev_t* indev = lv_indev_get_act();
    if (indev)
    {
        lv_indev_wait_release(indev);
    }

    lv_obj_add_flag(_root, LV_OBJ_FLAG_HIDDEN);
}

void Bluetooth::onViewDidDisappear()
{
    if (timer)
    {
        lv_timer_del(timer);
        timer = nullptr;
    }

    lv_group_t* group = lv_group_get_default();
    if (group)
    {
        lv_group_remove_obj(View.ui.swBluetooth);

        for (int i = 0; i < deviceBtnCount; i++)
        {
            if (btnDevice[i])
            {
                lv_group_remove_obj(btnDevice[i]);
            }
        }

        lv_group_remove_obj(View.ui.btnExit);
    }
}

void Bluetooth::onViewUnload()
{
    lv_obj_clean(_root);
}

void Bluetooth::onViewDidUnload()
{
}

void Bluetooth::AttachEvent(lv_obj_t* obj)
{
    lv_obj_add_event_cb(obj, onEvent, LV_EVENT_ALL, this);
}

void Bluetooth::TryStartScan()
{
    HAL::BluetoothInfo_t info = {};
    HAL::Bluetooth_GetInfo(&info);

    if (info.enabled && !info.connected && !info.scanning)
    {
        HAL::Bluetooth_StartScan(4000);
    }
}

void Bluetooth::RebuildGroup()
{
    lv_group_t* group = lv_group_get_default();
    if (!group)
    {
        return;
    }

    lv_group_remove_obj(View.ui.swBluetooth);
    lv_group_remove_obj(View.ui.btnExit);

    for (int i = 0; i < deviceBtnCount; i++)
    {
        if (btnDevice[i])
        {
            lv_group_remove_obj(btnDevice[i]);
        }
    }

    lv_group_add_obj(group, View.ui.swBluetooth);

    for (int i = 0; i < deviceBtnCount; i++)
    {
        if (btnDevice[i] && !lv_obj_has_state(btnDevice[i], LV_STATE_DISABLED))
        {
            lv_group_add_obj(group, btnDevice[i]);
        }
    }

    lv_group_add_obj(group, View.ui.btnExit);

    if (deviceBtnCount > 0 && btnDevice[0] && !lv_obj_has_state(btnDevice[0], LV_STATE_DISABLED))
    {
        lv_group_focus_obj(btnDevice[0]);
    }
    else
    {
        lv_group_focus_obj(View.ui.swBluetooth);
    }

    lv_group_set_editing(group, false);
}

void Bluetooth::RebuildDeviceList(const HAL::BluetoothInfo_t& info)
{
    lv_obj_clean(View.ui.contAvailable);

    for (int i = 0; i < MAX_VISIBLE_DEVICES; i++)
    {
        btnDevice[i] = nullptr;
        labelDevice[i] = nullptr;
    }
    deviceBtnCount = 0;

    uint8_t count = 0;

    if (!info.enabled)
    {
        count = 1;
    }
    else if (info.deviceCount == 0)
    {
        count = 1;
    }
    else
    {
        count = info.deviceCount > MAX_VISIBLE_DEVICES ? MAX_VISIBLE_DEVICES : info.deviceCount;
    }

    for (int i = 0; i < count; i++)
    {
        lv_obj_t* btn = lv_btn_create(View.ui.contAvailable);
        lv_obj_set_width(btn, 190);
        lv_obj_set_height(btn, 28);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x3A3A3A), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x4FA3FF), LV_STATE_FOCUSED);
        btnDevice[i] = btn;

        lv_obj_t* label = lv_label_create(btn);
        lv_obj_set_style_text_font(label, ResourcePool::GetFont("bahnschrift_17"), 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);

        if (!info.enabled)
        {
            lv_label_set_text(label, "Bluetooth Off");
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }
        else if (info.deviceCount == 0)
        {
            lv_label_set_text(label, info.scanning ? "Scanning..." : "No Devices");
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }
        else
        {
            lv_label_set_text(
                label,
                info.devices[i].name[0] ? info.devices[i].name : info.devices[i].address
            );
        }

        lv_obj_align(label, LV_ALIGN_LEFT_MID, 12, 0);
        labelDevice[i] = label;
        AttachEvent(btn);
    }

    deviceBtnCount = count;
    RebuildGroup();
}

void Bluetooth::RefreshUI()
{
    HAL::BluetoothInfo_t info = {};
    HAL::Bluetooth_GetInfo(&info);

    if (info.enabled)
    {
        lv_obj_add_state(View.ui.swBluetooth, LV_STATE_CHECKED);
        lv_label_set_text(View.ui.labelState, info.scanning ? "Scanning..." : "On");
        lv_obj_set_style_text_color(View.ui.labelState, lv_color_hex(0x4FA3FF), 0);
    }
    else
    {
        lv_obj_clear_state(View.ui.swBluetooth, LV_STATE_CHECKED);
        lv_label_set_text(View.ui.labelState, "Off");
        lv_obj_set_style_text_color(View.ui.labelState, lv_color_hex(0x888888), 0);
    }

    if (info.connected)
    {
        lv_label_set_text(
            View.ui.labelConnectedName,
            info.connectedName[0] ? info.connectedName : "Connected Device"
        );
        lv_label_set_text(View.ui.labelConnectedInfo, "Connected");
    }
    else
    {
        lv_label_set_text(View.ui.labelConnectedName, "No connected device");
        lv_label_set_text(View.ui.labelConnectedInfo, "-");
    }

    RebuildDeviceList(info);
}

void Bluetooth::onTimerUpdate(lv_timer_t* timer)
{
    Bluetooth* instance = (Bluetooth*)timer->user_data;
    if (!instance)
    {
        return;
    }

    instance->RefreshUI();

    HAL::BluetoothInfo_t info = {};
    HAL::Bluetooth_GetInfo(&info);

    if (info.enabled && !info.connected && !info.scanning && info.deviceCount == 0)
    {
        HAL::Bluetooth_StartScan(4000);
    }
}

void Bluetooth::onEvent(lv_event_t* event)
{
    Bluetooth* instance = (Bluetooth*)lv_event_get_user_data(event);
    LV_ASSERT_NULL(instance);

    lv_obj_t* obj = lv_event_get_current_target(event);
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_event_get_key(event);

        if (key == LV_KEY_ESC)
        {
            instance->_Manager->Pop();
            return;
        }

        if (key == LV_KEY_ENTER)
        {
            if (obj == instance->View.ui.btnExit)
            {
                instance->_Manager->Pop();
                return;
            }

            if (obj == instance->View.ui.swBluetooth)
            {
                HAL::BluetoothInfo_t info = {};
                HAL::Bluetooth_GetInfo(&info);

                HAL::Bluetooth_Enable(!info.enabled);
                if (!info.enabled)
                {
                    HAL::Bluetooth_StartScan(4000);
                }

                instance->RefreshUI();
                return;
            }

            for (int i = 0; i < instance->deviceBtnCount; i++)
            {
                if (obj == instance->btnDevice[i])
                {
                    HAL::Bluetooth_Connect(i);
                    instance->RefreshUI();
                    return;
                }
            }
        }
    }

    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_CLICKED)
    {
        if (obj == instance->View.ui.btnExit)
        {
            instance->_Manager->Pop();
            return;
        }

        if (obj == instance->View.ui.swBluetooth)
        {
            HAL::BluetoothInfo_t info = {};
            HAL::Bluetooth_GetInfo(&info);

            HAL::Bluetooth_Enable(!info.enabled);
            if (!info.enabled)
            {
                HAL::Bluetooth_StartScan(4000);
            }

            instance->RefreshUI();
            return;
        }

        for (int i = 0; i < instance->deviceBtnCount; i++)
        {
            if (obj == instance->btnDevice[i])
            {
                HAL::Bluetooth_Connect(i);
                instance->RefreshUI();
                return;
            }
        }
    }
}