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

    /*
       总连接状态：
       心率或踏频任意一个连接成功，connected 都为 true。
       为了兼容你原来页面里的 connectedName / connectedAddress，保留这两个字段。
    */
    bool connected;
    char connectedName[40];
    char connectedAddress[24];

    /* 心率连接状态 */
    bool heartRateConnected;
    char heartRateName[40];
    char heartRateAddress[24];

    bool heartRateServiceFound;
    bool heartRateNotifyEnabled;
    bool heartRateValid;
    uint8_t heartRate;
    uint32_t heartRateLastTick;

    /* 踏频连接状态 */
    bool cadenceConnected;
    char cadenceName[40];
    char cadenceAddress[24];

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

/* 兼容旧接口 */
bool Bluetooth_Connect(uint8_t index);
void Bluetooth_Disconnect();

/* 分开连接：用于心率和踏频同时连接 */
bool Bluetooth_ConnectHeartRate(uint8_t index);
bool Bluetooth_ConnectCadence(uint8_t index);
void Bluetooth_DisconnectHeartRate();
void Bluetooth_DisconnectCadence();

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