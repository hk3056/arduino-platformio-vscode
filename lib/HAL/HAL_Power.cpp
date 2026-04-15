#include "HAL.h"

struct
{
    uint32_t LastHandleTime;
    uint16_t AutoLowPowerTimeout;
    bool AutoLowPowerEnable;
    bool ShutdownReq;
    uint16_t ADCValue;
    HAL::Power_CallbackFunction_t EventCallback;
} Power;

void HAL::Power_Init()
{
    memset(&Power, 0, sizeof(Power));
    Power.AutoLowPowerTimeout = 60;
    Power.AutoLowPowerEnable = false;
    Serial.println("Power: ON");
}

void HAL::Power_HandleTimeUpdate()
{
    Power.LastHandleTime = millis();
}

void HAL::Power_SetAutoLowPowerTimeout(uint16_t sec)
{
    Power.AutoLowPowerTimeout = sec;
}

uint16_t HAL::Power_GetAutoLowPowerTimeout()
{
    return Power.AutoLowPowerTimeout;
}

void HAL::Power_SetAutoLowPowerEnable(bool en)
{
    Power.AutoLowPowerEnable = en;
}

void HAL::Power_Shutdown()
{
}

void HAL::Power_Update()
{
}

void HAL::Power_EventMonitor()
{
}

void HAL::Power_GetInfo(Power_Info_t* info)
{
    info->usage = 100;
    info->isCharging = false;
    info->voltage = 4000;
}

void HAL::Power_SetEventCallback(Power_CallbackFunction_t callback)
{
    Power.EventCallback = callback;
}