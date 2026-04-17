#include "Bluetooth.h"
#include "HAL_Bluetooth.h"

using namespace Page;

Bluetooth::Bluetooth()
{
    timer = nullptr;
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

    for (int i = 0; i < 3; i++)
    {
        AttachEvent(View.ui.btnDevice[i]);
    }

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
    lv_group_t* group = lv_group_get_default();
    if (group)
    {
        lv_group_add_obj(group, View.ui.swBluetooth);

        for (int i = 0; i < 3; i++)
        {
            lv_group_add_obj(group, View.ui.btnDevice[i]);
        }

        lv_group_add_obj(group, View.ui.btnExit);
        lv_group_focus_obj(View.ui.swBluetooth);
        lv_group_set_editing(group, false);
    }

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

        for (int i = 0; i < 3; i++)
        {
            lv_group_remove_obj(View.ui.btnDevice[i]);
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

    for (int i = 0; i < 3; i++)
    {
        if (!info.enabled)
        {
            lv_obj_add_state(View.ui.btnDevice[i], LV_STATE_DISABLED);
            lv_label_set_text(View.ui.labelDevice[i], "-");
            continue;
        }

        lv_obj_clear_state(View.ui.btnDevice[i], LV_STATE_DISABLED);

        if (i < info.deviceCount)
        {
            lv_label_set_text(
                View.ui.labelDevice[i],
                info.devices[i].name[0] ? info.devices[i].name : info.devices[i].address
            );
        }
        else
        {
            lv_label_set_text(View.ui.labelDevice[i], info.scanning ? "Scanning..." : "-");
            lv_obj_add_state(View.ui.btnDevice[i], LV_STATE_DISABLED);
        }
    }
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

            for (int i = 0; i < 3; i++)
            {
                if (obj == instance->View.ui.btnDevice[i])
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

        for (int i = 0; i < 3; i++)
        {
            if (obj == instance->View.ui.btnDevice[i])
            {
                HAL::Bluetooth_Connect(i);
                instance->RefreshUI();
                return;
            }
        }
    }
}