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
    static void onEvent(lv_event_t* event);

private:
    BluetoothView View;
};

}

#endif