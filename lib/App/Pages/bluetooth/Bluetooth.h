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

    void BuildFocusGroup();
    void ClearFocusGroup();

    const char* GetTargetName(DeviceTarget_t target);

    int FindDeviceByNameContains(
        const HAL::BluetoothInfo_t& info,
        const char* key
    );

    bool NameContains(const char* name, const char* key);
    bool IsHeartRateConnected(const HAL::BluetoothInfo_t& info);
    bool IsCadenceConnected(const HAL::BluetoothInfo_t& info);

    void TryStartScan();

    void OnBluetoothSwitchChanged();
    void OnHeartRateSwitchChanged();
    void OnCadenceSwitchChanged();

    void ConnectTarget(DeviceTarget_t target);

    void RefreshUI();

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