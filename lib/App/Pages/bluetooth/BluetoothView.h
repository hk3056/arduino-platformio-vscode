#ifndef __BLUETOOTH_VIEW_H
#define __BLUETOOTH_VIEW_H

#include "../Page.h"

namespace Page
{

class BluetoothView
{
public:
    void Create(lv_obj_t* root);

public:
    struct
    {
        lv_obj_t* labelTitle;

        lv_obj_t* swBluetooth;
        lv_obj_t* labelState;

        lv_obj_t* contConnected;
        lv_obj_t* labelConnectedTitle;
        lv_obj_t* labelConnectedName;
        lv_obj_t* labelConnectedInfo;

        lv_obj_t* labelAvailableTitle;
        lv_obj_t* contAvailable;

        lv_obj_t* btnDevice[3];
        lv_obj_t* labelDevice[3];

        lv_obj_t* btnExit;
        lv_obj_t* labelExit;
    } ui;
};

}

#endif