#ifndef __RIDEDATA_H
#define __RIDEDATA_H

#include "../Page.h"
#include "Common/DataProc/DataProc.h"
#include <stdio.h>
#include <string.h>

namespace Page
{

class RideData : public PageBase
{
public:
    RideData();
    virtual ~RideData();

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
    typedef struct
    {
        lv_obj_t* cont;
        lv_obj_t* title;
        lv_obj_t* value;
        lv_obj_t* unit;
    } DataCell_t;

private:
    struct
    {
        lv_obj_t* cont;

        lv_obj_t* arcSpeed;
        lv_obj_t* labelSpeed;
        lv_obj_t* labelMax;
        lv_obj_t* labelAvg;

        DataCell_t cells[7];
    } ui;

    Account* account;
    lv_timer_t* timer;

    HAL::SportStatus_Info_t sportStatusInfo;
    HAL::PHT_Info_t phtInfo;
    HAL::GPS_Info_t gpsInfo;

private:
    void CreateUI();
    void DeleteUI();

    void CreateSpeedPanel(lv_obj_t* parent);
    void CreateCells(lv_obj_t* parent);

    void CreateCell(
        lv_obj_t* parent,
        DataCell_t* cell,
        const char* title,
        const char* unit,
        lv_coord_t x,
        lv_coord_t y,
        lv_coord_t w,
        lv_coord_t h
    );

    void InitModel();
    void DeinitModel();
    void Update();

    void SetStatusBarStyle(DataProc::StatusBar_Style_t style);

    static void onTimerUpdate(lv_timer_t* timer);
    static void onEvent(lv_event_t* event);
    static int onDataEvent(Account* account, Account::EventParam_t* param);
};

}

#endif