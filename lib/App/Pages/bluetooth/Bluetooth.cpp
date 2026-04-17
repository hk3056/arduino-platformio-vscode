#include "Bluetooth.h"

using namespace Page;

Bluetooth::Bluetooth()
{
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

void Bluetooth::onEvent(lv_event_t* event)
{
    Bluetooth* instance = (Bluetooth*)lv_event_get_user_data(event);
    LV_ASSERT_NULL(instance);

    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_KEY)
    {
        uint32_t key = lv_event_get_key(event);
        if (key == LV_KEY_ENTER || key == LV_KEY_ESC)
        {
            instance->_Manager->Pop();
            return;
        }
    }

    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LEAVE)
    {
        instance->_Manager->Pop();
        return;
    }
}