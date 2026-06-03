#include "Bluetooth.h"
#include "HAL_Bluetooth.h"

#include <string.h>

using namespace Page;

/* 心率模块蓝牙名字 */
static const char* HEART_RATE_DEVICE_NAME = "XTrack-HR";

/* 踏频模块蓝牙名字 */
static const char* CADENCE_DEVICE_NAME = "XTrack-CAD";

static const uint32_t SCAN_INTERVAL_MS = 5000;
static const uint32_t SCAN_TIME_MS = 4000;

Bluetooth::Bluetooth()
{
    timer = nullptr;

    heartRateEnable = false;
    cadenceEnable = false;

    lastScanTick = 0;
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
    AttachEvent(View.ui.swHeartRate);
    AttachEvent(View.ui.swCadence);
    AttachEvent(View.ui.btnHeartRateName);
    AttachEvent(View.ui.btnCadenceName);
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
    lv_obj_set_style_bg_color(_root, lv_color_hex(0x101010), LV_PART_MAIN);

    lv_indev_t* indev = lv_indev_get_act();
    if (indev)
    {
        lv_indev_wait_release(indev);
    }

    lastScanTick = 0;

    RefreshUI();
}

void Bluetooth::onViewDidAppear()
{
    BuildFocusGroup();
    TryStartScan();

    if (timer == nullptr)
    {
        timer = lv_timer_create(onTimerUpdate, 300, this);
        if (timer)
        {
            lv_timer_ready(timer);
        }
    }
}

void Bluetooth::onViewWillDisappear()
{
    HAL::Bluetooth_StopScan();

    if (timer)
    {
        lv_timer_del(timer);
        timer = nullptr;
    }

    ClearFocusGroup();

    lv_indev_t* indev = lv_indev_get_act();
    if (indev)
    {
        lv_indev_wait_release(indev);
    }

    lv_obj_add_flag(_root, LV_OBJ_FLAG_HIDDEN);
}

void Bluetooth::onViewDidDisappear()
{
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
    if (!obj)
    {
        return;
    }

    lv_obj_add_event_cb(obj, onEvent, LV_EVENT_ALL, this);
}

void Bluetooth::BuildFocusGroup()
{
    lv_group_t* group = lv_group_get_default();

    if (!group)
    {
        return;
    }

    /*
     * 这里只在页面出现时建立一次焦点顺序。
     * 不要放到 RefreshUI() 里反复执行，否则焦点会一直被重置。
     */
    lv_group_remove_all_objs(group);

    lv_group_add_obj(group, View.ui.swBluetooth);
    lv_group_add_obj(group, View.ui.swHeartRate);
    lv_group_add_obj(group, View.ui.btnHeartRateName);
    lv_group_add_obj(group, View.ui.swCadence);
    lv_group_add_obj(group, View.ui.btnCadenceName);
    lv_group_add_obj(group, View.ui.btnExit);

    lv_group_focus_obj(View.ui.swBluetooth);
    lv_group_set_editing(group, false);
}

void Bluetooth::ClearFocusGroup()
{
    lv_group_t* group = lv_group_get_default();

    if (!group)
    {
        return;
    }

    lv_group_remove_obj(View.ui.swBluetooth);
    lv_group_remove_obj(View.ui.swHeartRate);
    lv_group_remove_obj(View.ui.btnHeartRateName);
    lv_group_remove_obj(View.ui.swCadence);
    lv_group_remove_obj(View.ui.btnCadenceName);
    lv_group_remove_obj(View.ui.btnExit);
}

const char* Bluetooth::GetTargetName(DeviceTarget_t target)
{
    if (target == TARGET_HEART_RATE)
    {
        return HEART_RATE_DEVICE_NAME;
    }

    return CADENCE_DEVICE_NAME;
}

int Bluetooth::FindDeviceByName(const HAL::BluetoothInfo_t& info, const char* name)
{
    if (!name)
    {
        return -1;
    }

    for (int i = 0; i < info.deviceCount; i++)
    {
        if (strcmp(info.devices[i].name, name) == 0)
        {
            return i;
        }
    }

    return -1;
}

bool Bluetooth::IsConnectedToName(const HAL::BluetoothInfo_t& info, const char* name)
{
    if (!info.connected || !name)
    {
        return false;
    }

    if (info.connectedName[0] && strcmp(info.connectedName, name) == 0)
    {
        return true;
    }

    int index = FindDeviceByName(info, name);

    if (index >= 0 &&
        info.connectedAddress[0] &&
        strcmp(info.connectedAddress, info.devices[index].address) == 0)
    {
        return true;
    }

    return false;
}

void Bluetooth::TryStartScan()
{
    HAL::BluetoothInfo_t info = {};
    HAL::Bluetooth_GetInfo(&info);

    if (!info.enabled)
    {
        return;
    }

    if (!heartRateEnable && !cadenceEnable)
    {
        return;
    }

    if (info.scanning)
    {
        return;
    }

    bool needHeartRate = heartRateEnable &&
                         !IsConnectedToName(info, HEART_RATE_DEVICE_NAME) &&
                         FindDeviceByName(info, HEART_RATE_DEVICE_NAME) < 0;

    bool needCadence = cadenceEnable &&
                       !IsConnectedToName(info, CADENCE_DEVICE_NAME) &&
                       FindDeviceByName(info, CADENCE_DEVICE_NAME) < 0;

    if (!needHeartRate && !needCadence)
    {
        return;
    }

    uint32_t now = lv_tick_get();

    if (lastScanTick == 0 || now - lastScanTick >= SCAN_INTERVAL_MS)
    {
        lastScanTick = now;
        HAL::Bluetooth_StartScan(SCAN_TIME_MS);
    }
}

void Bluetooth::OnBluetoothSwitchChanged()
{
    bool checked = lv_obj_has_state(View.ui.swBluetooth, LV_STATE_CHECKED);

    HAL::Bluetooth_Enable(checked);

    if (!checked)
    {
        heartRateEnable = false;
        cadenceEnable = false;

        lv_obj_clear_state(View.ui.swHeartRate, LV_STATE_CHECKED);
        lv_obj_clear_state(View.ui.swCadence, LV_STATE_CHECKED);

        HAL::Bluetooth_Disconnect();
    }

    lastScanTick = 0;

    RefreshUI();
    TryStartScan();
}

void Bluetooth::OnHeartRateSwitchChanged()
{
    HAL::BluetoothInfo_t info = {};
    HAL::Bluetooth_GetInfo(&info);

    heartRateEnable = lv_obj_has_state(View.ui.swHeartRate, LV_STATE_CHECKED);

    if (!info.enabled)
    {
        heartRateEnable = false;
        lv_obj_clear_state(View.ui.swHeartRate, LV_STATE_CHECKED);
        lv_label_set_text(View.ui.labelHeartRateHint, "Bluetooth Off");
        RefreshUI();
        return;
    }

    if (!heartRateEnable && IsConnectedToName(info, HEART_RATE_DEVICE_NAME))
    {
        HAL::Bluetooth_Disconnect();
    }

    lastScanTick = 0;

    RefreshUI();
    TryStartScan();
}

void Bluetooth::OnCadenceSwitchChanged()
{
    HAL::BluetoothInfo_t info = {};
    HAL::Bluetooth_GetInfo(&info);

    cadenceEnable = lv_obj_has_state(View.ui.swCadence, LV_STATE_CHECKED);

    if (!info.enabled)
    {
        cadenceEnable = false;
        lv_obj_clear_state(View.ui.swCadence, LV_STATE_CHECKED);
        lv_label_set_text(View.ui.labelCadenceHint, "Bluetooth Off");
        RefreshUI();
        return;
    }

    if (!cadenceEnable && IsConnectedToName(info, CADENCE_DEVICE_NAME))
    {
        HAL::Bluetooth_Disconnect();
    }

    lastScanTick = 0;

    RefreshUI();
    TryStartScan();
}

void Bluetooth::ConnectTarget(DeviceTarget_t target)
{
    HAL::BluetoothInfo_t info = {};
    HAL::Bluetooth_GetInfo(&info);

    if (!info.enabled)
    {
        if (target == TARGET_HEART_RATE)
        {
            lv_label_set_text(View.ui.labelHeartRateHint, "Bluetooth Off");
        }
        else
        {
            lv_label_set_text(View.ui.labelCadenceHint, "Bluetooth Off");
        }

        return;
    }

    if (target == TARGET_HEART_RATE && !heartRateEnable)
    {
        lv_label_set_text(View.ui.labelHeartRateHint, "Heart Rate Off");
        return;
    }

    if (target == TARGET_CADENCE && !cadenceEnable)
    {
        lv_label_set_text(View.ui.labelCadenceHint, "Cadence Off");
        return;
    }

    const char* targetName = GetTargetName(target);
    int index = FindDeviceByName(info, targetName);

    if (index < 0)
    {
        if (target == TARGET_HEART_RATE)
        {
            lv_label_set_text(View.ui.labelHeartRateHint, "No Bluetooth Device");
        }
        else
        {
            lv_label_set_text(View.ui.labelCadenceHint, "No Bluetooth Device");
        }

        lastScanTick = 0;
        TryStartScan();
        RefreshUI();
        return;
    }

    if (target == TARGET_HEART_RATE)
    {
        lv_label_set_text(View.ui.labelHeartRateHint, "Connecting...");
    }
    else
    {
        lv_label_set_text(View.ui.labelCadenceHint, "Connecting...");
    }

    bool ok = HAL::Bluetooth_Connect((uint8_t)index);

    if (!ok)
    {
        if (target == TARGET_HEART_RATE)
        {
            lv_label_set_text(View.ui.labelHeartRateHint, "Connection Failed");
        }
        else
        {
            lv_label_set_text(View.ui.labelCadenceHint, "Connection Failed");
        }

        lastScanTick = 0;
        TryStartScan();
    }

    RefreshUI();
}

void Bluetooth::RefreshUI()
{
    HAL::BluetoothInfo_t info = {};
    HAL::Bluetooth_GetInfo(&info);

    bool heartFound = FindDeviceByName(info, HEART_RATE_DEVICE_NAME) >= 0;
    bool cadenceFound = FindDeviceByName(info, CADENCE_DEVICE_NAME) >= 0;

    bool heartConnected = IsConnectedToName(info, HEART_RATE_DEVICE_NAME);
    bool cadenceConnected = IsConnectedToName(info, CADENCE_DEVICE_NAME);

    /* 蓝牙总开关 */
    if (info.enabled)
    {
        lv_obj_add_state(View.ui.swBluetooth, LV_STATE_CHECKED);

        if (info.scanning)
        {
            lv_label_set_text(View.ui.labelState, "Scanning");
        }
        else
        {
            lv_label_set_text(View.ui.labelState, "On");
        }

        lv_obj_set_style_text_color(View.ui.labelState, lv_color_hex(0x4FA3FF), 0);
    }
    else
    {
        lv_obj_clear_state(View.ui.swBluetooth, LV_STATE_CHECKED);
        lv_label_set_text(View.ui.labelState, "Off");
        lv_obj_set_style_text_color(View.ui.labelState, lv_color_hex(0x888888), 0);
    }

    /* 心率开关显示 */
    if (heartRateEnable)
    {
        lv_obj_add_state(View.ui.swHeartRate, LV_STATE_CHECKED);
    }
    else
    {
        lv_obj_clear_state(View.ui.swHeartRate, LV_STATE_CHECKED);
    }

    /* 踏频开关显示 */
    if (cadenceEnable)
    {
        lv_obj_add_state(View.ui.swCadence, LV_STATE_CHECKED);
    }
    else
    {
        lv_obj_clear_state(View.ui.swCadence, LV_STATE_CHECKED);
    }

    /* 心率名字按钮 */
    lv_label_set_text(View.ui.labelHeartRateName, HEART_RATE_DEVICE_NAME);

    if (!info.enabled)
    {
        lv_obj_set_style_bg_color(View.ui.btnHeartRateName, lv_color_hex(0x3A3A3A), 0);
        lv_label_set_text(View.ui.labelHeartRateHint, "Bluetooth Off");
    }
    else if (!heartRateEnable)
    {
        lv_obj_set_style_bg_color(View.ui.btnHeartRateName, lv_color_hex(0x3A3A3A), 0);
        lv_label_set_text(View.ui.labelHeartRateHint, "Heart Rate Off");
    }
    else if (heartConnected)
    {
        lv_obj_set_style_bg_color(View.ui.btnHeartRateName, lv_color_hex(0x4FA3FF), 0);
        lv_label_set_text(View.ui.labelHeartRateHint, "Connected");
    }
    else if (heartFound)
    {
        lv_obj_set_style_bg_color(View.ui.btnHeartRateName, lv_color_hex(0x3A3A3A), 0);
        lv_label_set_text(View.ui.labelHeartRateHint, "Press to Connect");
    }
    else
    {
        lv_obj_set_style_bg_color(View.ui.btnHeartRateName, lv_color_hex(0x3A3A3A), 0);
        lv_label_set_text(View.ui.labelHeartRateHint, "No Bluetooth Device");
    }

    /* 踏频名字按钮 */
    lv_label_set_text(View.ui.labelCadenceName, CADENCE_DEVICE_NAME);

    if (!info.enabled)
    {
        lv_obj_set_style_bg_color(View.ui.btnCadenceName, lv_color_hex(0x3A3A3A), 0);
        lv_label_set_text(View.ui.labelCadenceHint, "Bluetooth Off");
    }
    else if (!cadenceEnable)
    {
        lv_obj_set_style_bg_color(View.ui.btnCadenceName, lv_color_hex(0x3A3A3A), 0);
        lv_label_set_text(View.ui.labelCadenceHint, "Cadence Off");
    }
    else if (cadenceConnected)
    {
        lv_obj_set_style_bg_color(View.ui.btnCadenceName, lv_color_hex(0x4FA3FF), 0);
        lv_label_set_text(View.ui.labelCadenceHint, "Connected");
    }
    else if (cadenceFound)
    {
        lv_obj_set_style_bg_color(View.ui.btnCadenceName, lv_color_hex(0x3A3A3A), 0);
        lv_label_set_text(View.ui.labelCadenceHint, "Press to Connect");
    }
    else
    {
        lv_obj_set_style_bg_color(View.ui.btnCadenceName, lv_color_hex(0x3A3A3A), 0);
        lv_label_set_text(View.ui.labelCadenceHint, "No Bluetooth Device");
    }
}

void Bluetooth::onTimerUpdate(lv_timer_t* timer)
{
    Bluetooth* instance = (Bluetooth*)timer->user_data;

    if (!instance)
    {
        return;
    }

    if (lv_obj_has_flag(instance->_root, LV_OBJ_FLAG_HIDDEN))
    {
        return;
    }

    HAL::Bluetooth_Update();

    instance->RefreshUI();
    instance->TryStartScan();
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
            if (obj == instance->View.ui.btnHeartRateName)
            {
                instance->ConnectTarget(TARGET_HEART_RATE);
                return;
            }

            if (obj == instance->View.ui.btnCadenceName)
            {
                instance->ConnectTarget(TARGET_CADENCE);
                return;
            }

            if (obj == instance->View.ui.btnExit)
            {
                instance->_Manager->Pop();
                return;
            }
        }
    }

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        if (obj == instance->View.ui.swBluetooth)
        {
            instance->OnBluetoothSwitchChanged();
            return;
        }

        if (obj == instance->View.ui.swHeartRate)
        {
            instance->OnHeartRateSwitchChanged();
            return;
        }

        if (obj == instance->View.ui.swCadence)
        {
            instance->OnCadenceSwitchChanged();
            return;
        }
    }

    if (code == LV_EVENT_CLICKED)
    {
        if (obj == instance->View.ui.btnHeartRateName)
        {
            instance->ConnectTarget(TARGET_HEART_RATE);
            return;
        }

        if (obj == instance->View.ui.btnCadenceName)
        {
            instance->ConnectTarget(TARGET_CADENCE);
            return;
        }

        if (obj == instance->View.ui.btnExit)
        {
            instance->_Manager->Pop();
            return;
        }
    }
}