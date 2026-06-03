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

        /* 蓝牙总开关 */
        lv_obj_t* swBluetooth;
        lv_obj_t* labelState;

        /* 心率模块 */
        lv_obj_t* labelHeartTitle;
        lv_obj_t* swHeartRate;
        lv_obj_t* btnHeartRateName;
        lv_obj_t* labelHeartRateName;
        lv_obj_t* labelHeartRateHint;

        /* 踏频模块 */
        lv_obj_t* labelCadenceTitle;
        lv_obj_t* swCadence;
        lv_obj_t* btnCadenceName;
        lv_obj_t* labelCadenceName;
        lv_obj_t* labelCadenceHint;

        /* 退出按钮 */
        lv_obj_t* btnExit;
        lv_obj_t* labelExit;

    } ui;
};

} // namespace Page

#endif