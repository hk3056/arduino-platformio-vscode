#ifndef __HAL_BLUETOOTH_H
#define __HAL_BLUETOOTH_H

#include "HAL.h"

namespace HAL
{

struct BluetoothDeviceItem_t
{
    char name[40];
    char address[24];
    int rssi;
};

struct BluetoothInfo_t
{
    bool enabled;
    bool scanning;
    bool connected;
    char connectedName[40];
    char connectedAddress[24];

    uint8_t deviceCount;
    BluetoothDeviceItem_t devices[8];
};

bool Bluetooth_Init();
void Bluetooth_Update();

bool Bluetooth_Enable(bool en);
bool Bluetooth_IsEnabled();

bool Bluetooth_StartScan(uint32_t scanMs = 4000);
void Bluetooth_StopScan();

bool Bluetooth_Connect(uint8_t index);
void Bluetooth_Disconnect();

void Bluetooth_GetInfo(BluetoothInfo_t* info);

}

#endif