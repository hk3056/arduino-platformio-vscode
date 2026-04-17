#include "Bluetooth.h"

using namespace Page;

static const char* kBluetoothDeviceNames[3] =
{
    "Device A",
    "Device B",
    "Device C"
};

Bluetooth::Bluetooth()
{
    isBluetoothOn = true;
    connectedDeviceIndex = -1;
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

    UpdateBluetoothState(isBluetoothOn);
}

void Bluetooth::onViewDidLoad()
{
}

void Bluetooth::onViewWillAppear()
{
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_root, lv_color_black(), LV_PART_MAIN);

    lv_indev_t* indev = lv_indev_get_act();
    if (indev)
    {
        lv_indev_wait_release(indev);
    }
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
}

void Bluetooth::onViewWillDisappear()
{
}

void Bluetooth::onViewDidDisappear()
{
}

void Bluetooth::onViewUnload()
{
}

void Bluetooth::onViewDidUnload()
{
}

void Bluetooth::AttachEvent(lv_obj_t* obj)
{
    lv_obj_add_event_cb(obj, onEvent, LV_EVENT_ALL, this);
}

void Bluetooth::SetConnectedDevice(int index)
{
    connectedDeviceIndex = index;

    if (index < 0 || index >= 3)
    {
        lv_label_set_text(View.ui.labelConnectedName, "No connected device");
        lv_label_set_text(View.ui.labelConnectedInfo, "-");
        return;
    }

    lv_label_set_text(View.ui.labelConnectedName, kBluetoothDeviceNames[index]);
    lv_label_set_text(View.ui.labelConnectedInfo, "Connected");
}

void Bluetooth::UpdateBluetoothState(bool isOn)
{
    isBluetoothOn = isOn;

    if (isOn)
    {
        lv_obj_add_state(View.ui.swBluetooth, LV_STATE_CHECKED);
        lv_label_set_text(View.ui.labelState, "On");
        lv_obj_set_style_text_color(View.ui.labelState, lv_color_hex(0x4FA3FF), 0);

        for (int i = 0; i < 3; i++)
        {
            lv_obj_clear_state(View.ui.btnDevice[i], LV_STATE_DISABLED);
        }

        if (connectedDeviceIndex >= 0)
        {
            SetConnectedDevice(connectedDeviceIndex);
        }
        else
        {
            SetConnectedDevice(-1);
        }
    }
    else
    {
        lv_obj_clear_state(View.ui.swBluetooth, LV_STATE_CHECKED);
        lv_label_set_text(View.ui.labelState, "Off");
        lv_obj_set_style_text_color(View.ui.labelState, lv_color_hex(0x888888), 0);

        lv_label_set_text(View.ui.labelConnectedName, "No connected device");
        lv_label_set_text(View.ui.labelConnectedInfo, "-");

        for (int i = 0; i < 3; i++)
        {
            lv_obj_add_state(View.ui.btnDevice[i], LV_STATE_DISABLED);
        }
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
                instance->UpdateBluetoothState(!instance->isBluetoothOn);
                return;
            }

            if (instance->isBluetoothOn)
            {
                for (int i = 0; i < 3; i++)
                {
                    if (obj == instance->View.ui.btnDevice[i])
                    {
                        instance->SetConnectedDevice(i);
                        return;
                    }
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
            instance->UpdateBluetoothState(!instance->isBluetoothOn);
            return;
        }

        if (instance->isBluetoothOn)
        {
            for (int i = 0; i < 3; i++)
            {
                if (obj == instance->View.ui.btnDevice[i])
                {
                    instance->SetConnectedDevice(i);
                    return;
                }
            }
        }
    }
}