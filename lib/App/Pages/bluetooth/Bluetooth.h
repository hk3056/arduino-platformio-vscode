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
    void AttachEvent(lv_obj_t* obj);
    void RefreshUI();
    void TryStartScan();
    void RebuildDeviceList(const HAL::BluetoothInfo_t& info);
    void RebuildGroup();
    static void onEvent(lv_event_t* event);
    static void onTimerUpdate(lv_timer_t* timer);

private:
    static const uint8_t MAX_VISIBLE_DEVICES = 8;

    BluetoothView View;
    lv_timer_t* timer;

    lv_obj_t* btnDevice[MAX_VISIBLE_DEVICES];
    lv_obj_t* labelDevice[MAX_VISIBLE_DEVICES];
    uint8_t deviceBtnCount;
};

}

#endif