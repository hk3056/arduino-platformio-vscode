#include "HAL_Bluetooth.h"
#include <NimBLEDevice.h>
#include <string.h>

namespace HAL
{

static BluetoothInfo_t s_info = {};
static NimBLEScan* s_scan = nullptr;
static NimBLEClient* s_client = nullptr;
static NimBLEAdvertisedDevice* s_devices[8] = { nullptr };
static bool s_initOk = false;

class AdvCallbacks : public NimBLEScanCallbacks
{
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override
    {
        if (!advertisedDevice) return;

        for (int i = 0; i < s_info.deviceCount; i++)
        {
            if (strcmp(s_info.devices[i].address, advertisedDevice->getAddress().toString().c_str()) == 0)
            {
                return;
            }
        }

        if (s_info.deviceCount >= 8) return;

        uint8_t idx = s_info.deviceCount;

        std::string name = advertisedDevice->getName();
        std::string addr = advertisedDevice->getAddress().toString();

        strncpy(s_info.devices[idx].name, name.empty() ? "Unknown Device" : name.c_str(), sizeof(s_info.devices[idx].name) - 1);
        strncpy(s_info.devices[idx].address, addr.c_str(), sizeof(s_info.devices[idx].address) - 1);
        s_info.devices[idx].rssi = advertisedDevice->getRSSI();

        s_devices[idx] = new NimBLEAdvertisedDevice(*advertisedDevice);
        s_info.deviceCount++;
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override
    {
        (void)results;
        (void)reason;
        s_info.scanning = false;
    }
};

static AdvCallbacks s_advCallbacks;

class ClientCallbacks : public NimBLEClientCallbacks
{
    void onConnect(NimBLEClient* pClient) override
    {
        (void)pClient;
        s_info.connected = true;
    }

    void onDisconnect(NimBLEClient* pClient, int reason) override
    {
        (void)pClient;
        (void)reason;
        s_info.connected = false;
        s_info.connectedName[0] = '\0';
        s_info.connectedAddress[0] = '\0';
    }

    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) override
    {
        (void)pClient;
        (void)params;
        return true;
    }
};

static ClientCallbacks s_clientCallbacks;

bool Bluetooth_Init()
{
    if (s_initOk) return true;

    memset(&s_info, 0, sizeof(s_info));
    s_info.enabled = false;
    s_info.scanning = false;
    s_info.connected = false;

    return true;
}

bool Bluetooth_Enable(bool en)
{
    if (en == s_info.enabled) return true;

    if (en)
    {
        NimBLEDevice::init("");
        NimBLEDevice::setPower(ESP_PWR_LVL_P9);

        s_scan = NimBLEDevice::getScan();
        s_scan->setScanCallbacks(&s_advCallbacks, false);
        s_scan->setActiveScan(true);
        s_scan->setInterval(45);
        s_scan->setWindow(15);

        s_client = NimBLEDevice::createClient();
        s_client->setClientCallbacks(&s_clientCallbacks, false);

        s_info.enabled = true;
        s_initOk = true;
        return true;
    }
    else
    {
        Bluetooth_StopScan();
        Bluetooth_Disconnect();

        if (s_client)
        {
            NimBLEDevice::deleteClient(s_client);
            s_client = nullptr;
        }

        for (int i = 0; i < 8; i++)
        {
            delete s_devices[i];
            s_devices[i] = nullptr;
        }

        memset(&s_info, 0, sizeof(s_info));
        NimBLEDevice::deinit(true);

        s_info.enabled = false;
        return true;
    }
}

bool Bluetooth_IsEnabled()
{
    return s_info.enabled;
}

bool Bluetooth_StartScan(uint32_t scanMs)
{
    if (!s_info.enabled || !s_scan) return false;
    if (s_info.scanning) return true;

    for (int i = 0; i < 8; i++)
    {
        delete s_devices[i];
        s_devices[i] = nullptr;
    }
    memset(s_info.devices, 0, sizeof(s_info.devices));
    s_info.deviceCount = 0;

    s_info.scanning = true;
    s_scan->start(scanMs / 1000.0f, false, true);
    return true;
}

void Bluetooth_StopScan()
{
    if (s_scan && s_info.scanning)
    {
        s_scan->stop();
        s_info.scanning = false;
    }
}

bool Bluetooth_Connect(uint8_t index)
{
    if (!s_info.enabled || !s_client) return false;
    if (index >= s_info.deviceCount || s_devices[index] == nullptr) return false;

    Bluetooth_StopScan();

    if (s_client->isConnected())
    {
        s_client->disconnect();
    }

    if (!s_client->connect(s_devices[index]))
    {
        s_info.connected = false;
        return false;
    }

    strncpy(s_info.connectedName, s_info.devices[index].name, sizeof(s_info.connectedName) - 1);
    strncpy(s_info.connectedAddress, s_info.devices[index].address, sizeof(s_info.connectedAddress) - 1);
    s_info.connected = true;
    return true;
}

void Bluetooth_Disconnect()
{
    if (s_client && s_client->isConnected())
    {
        s_client->disconnect();
    }
    s_info.connected = false;
    s_info.connectedName[0] = '\0';
    s_info.connectedAddress[0] = '\0';
}

void Bluetooth_GetInfo(BluetoothInfo_t* info)
{
    if (!info) return;
    memcpy(info, &s_info, sizeof(BluetoothInfo_t));
}

void Bluetooth_Update()
{
    // 第一版先留空，状态主要靠回调更新
}

}