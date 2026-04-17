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
        lv_obj_t* labelHint;
    } ui;
};

}

#endif