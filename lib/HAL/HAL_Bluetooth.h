#ifndef __HAL_BLUETOOTH_H
#define __HAL_BLUETOOTH_H

#include "HAL.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string>

#ifndef HAL_BT_MAX_DEVICES
#define HAL_BT_MAX_DEVICES 8
#endif

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

    /* 心率状态 */
    bool heartRateServiceFound;
    bool heartRateNotifyEnabled;
    bool heartRateValid;
    uint8_t heartRate;
    uint32_t heartRateLastTick;

    /* 踏频状态 */
    bool cadenceServiceFound;
    bool cadenceNotifyEnabled;
    bool cadenceValid;
    uint16_t cadenceRpm;
    uint16_t cadenceCrankRevCount;
    uint16_t cadenceLastEventTime;
    uint32_t cadenceLastTick;

    uint8_t deviceCount;
    BluetoothDeviceItem_t devices[HAL_BT_MAX_DEVICES];
};

/* 基础管理 */
bool Bluetooth_Init();
void Bluetooth_Update();
bool Bluetooth_Enable(bool en);
bool Bluetooth_IsEnabled();

/* 扫描 */
bool Bluetooth_StartScan(uint32_t scanMs = 4000);
void Bluetooth_StopScan();

/* 连接 */
bool Bluetooth_Connect(uint8_t index);
void Bluetooth_Disconnect();

/* 心率 */
bool Bluetooth_SubscribeHeartRate();
bool Bluetooth_IsHeartRateValid();
uint8_t Bluetooth_GetHeartRate();

/* 踏频 */
bool Bluetooth_SubscribeCadence();
bool Bluetooth_IsCadenceValid();
uint16_t Bluetooth_GetCadenceRpm();

/* 服务发现 */
bool Bluetooth_DiscoverServices();
void Bluetooth_ClearServices();

/* 数据读写 */
bool Bluetooth_ReadCharacteristic(
    const char* serviceUUID,
    const char* charUUID,
    std::string& outValue
);

bool Bluetooth_WriteCharacteristic(
    const char* serviceUUID,
    const char* charUUID,
    const uint8_t* data,
    size_t len,
    bool response = true
);

bool Bluetooth_SubscribeNotification(
    const char* serviceUUID,
    const char* charUUID,
    NimBLERemoteCharacteristic::notify_callback cb,
    bool notifications = true
);

/* 信息 */
void Bluetooth_GetInfo(BluetoothInfo_t* info);

} // namespace HAL

#endif