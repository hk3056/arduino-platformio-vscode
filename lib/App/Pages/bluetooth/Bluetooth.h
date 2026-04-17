#ifndef __BLUETOOTH_PAGE_H
#define __BLUETOOTH_PAGE_H

#include "BluetoothView.h"

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
    static void onEvent(lv_event_t* event);
    static void onTimerUpdate(lv_timer_t* timer);

private:
    BluetoothView View;
    lv_timer_t* timer;
};

}

#endif