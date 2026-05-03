#ifndef __HAL_BLUETOOTH_H
#define __HAL_BLUETOOTH_H

#include "HAL.h"
#include <NimBLEDevice.h>

namespace HAL {

// ---------- 数据结构 ----------
struct BluetoothDeviceItem_t {
    char    name[40];
    char    address[24];
    int     rssi;
};

struct BluetoothInfo_t {
    bool    enabled;
    bool    scanning;
    bool    connected;
    char    connectedName[40];
    char    connectedAddress[24];
    uint8_t deviceCount;
    BluetoothDeviceItem_t devices[8];
};

// ---------- 基础管理 ----------
bool Bluetooth_Init();
void Bluetooth_Update();
bool Bluetooth_Enable(bool en);
bool Bluetooth_IsEnabled();

// ---------- 扫描 ----------
bool Bluetooth_StartScan(uint32_t scanMs = 4000);
void Bluetooth_StopScan();

// ---------- 连接 ----------
bool Bluetooth_Connect(uint8_t index);
void Bluetooth_Disconnect();

// ---------- 服务发现 ----------
bool Bluetooth_DiscoverServices();
void Bluetooth_ClearServices();

// ---------- 数据读写 (使用已知UUID) ----------
bool Bluetooth_ReadCharacteristic(const char* serviceUUID,
                                  const char* charUUID,
                                  std::string& outValue);
bool Bluetooth_WriteCharacteristic(const char* serviceUUID,
                                   const char* charUUID,
                                   const uint8_t* data, size_t len,
                                   bool response = true);
bool Bluetooth_SubscribeNotification(const char* serviceUUID,
                                     const char* charUUID,
                                     NimBLERemoteCharacteristic::notify_callback cb,
                                     bool notifications = true);

// ---------- 信息 ----------
void Bluetooth_GetInfo(BluetoothInfo_t* info);

} // namespace HAL

#endif