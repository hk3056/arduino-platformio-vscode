#ifndef __BLUETOOTH_PAGE_H
#define __BLUETOOTH_PAGE_H

#include "BluetoothView.h"
#include "HAL_Bluetooth.h"

namespace Page
{

class Bluetooth : public PageBase
{
public:
    Bluetooth();
    virtual ~Bluetooth();

    virtual void onCustomAttrConfig();

    virtual void onViewLoad();
    virtual void onViewDidLoad();
    virtual void onViewWillAppear();
    virtual void onViewDidAppear();
    virtual void onViewWillDisappear();
    virtual void onViewDidDisappear();

    virtual void onViewUnload();
    virtual void onViewDidUnload();

private:
    enum DeviceTarget_t
    {
        TARGET_HEART_RATE = 0,
        TARGET_CADENCE
    };

private:
    void AttachEvent(lv_obj_t* obj);

    void RefreshUI();
    void TryStartScan();

    void BuildFocusGroup();
    void ClearFocusGroup();

    void OnBluetoothSwitchChanged();
    void OnHeartRateSwitchChanged();
    void OnCadenceSwitchChanged();

    void ConnectTarget(DeviceTarget_t target);

    int FindDeviceByName(const HAL::BluetoothInfo_t& info, const char* name);
    bool IsConnectedToName(const HAL::BluetoothInfo_t& info, const char* name);

    const char* GetTargetName(DeviceTarget_t target);

    static void onEvent(lv_event_t* event);
    static void onTimerUpdate(lv_timer_t* timer);

private:
    BluetoothView View;

    lv_timer_t* timer;

    bool heartRateEnable;
    bool cadenceEnable;

    uint32_t lastScanTick;
};

} // namespace Page

#endif