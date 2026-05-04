#include "HAL_Bluetooth.h"
#include <esp_task_wdt.h>
#include <string>

namespace HAL {

// ---------- 静态变量 ----------
static BluetoothInfo_t s_info = {};
static NimBLEScan*     s_scan = nullptr;
static NimBLEClient*   s_client = nullptr;
static NimBLEAdvertisedDevice* s_devices[8] = { nullptr };
static bool s_initOk = false;
static bool s_nimbleInited = false;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

// ---------- 扫描回调 ----------
class AdvCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        if (!dev) return;

        std::string addr = dev->getAddress().toString();
        if (addr.empty()) return;

        portENTER_CRITICAL(&s_mux);

        int cnt = s_info.deviceCount;

        // 去重
        for (int i = 0; i < cnt; i++) {
            if (addr == s_info.devices[i].address) {
                portEXIT_CRITICAL(&s_mux);
                return;
            }
        }

        if (cnt >= 8) {
            portEXIT_CRITICAL(&s_mux);
            return;
        }

        uint8_t idx = cnt;

        std::string name = dev->getName();
        if (name.empty()) name = "Unknown";

        strncpy(s_info.devices[idx].name, name.c_str(),
                sizeof(s_info.devices[idx].name) - 1);

        strncpy(s_info.devices[idx].address, addr.c_str(),
                sizeof(s_info.devices[idx].address) - 1);

        s_info.devices[idx].rssi = dev->getRSSI();

        delete s_devices[idx];
        s_devices[idx] = new NimBLEAdvertisedDevice(*dev);

        s_info.deviceCount++;

        portEXIT_CRITICAL(&s_mux);
    }

    void onScanEnd(const NimBLEScanResults&, int) override {
        s_info.scanning = false;
    }
};

static AdvCallbacks s_advCallbacks;

// ---------- 客户端回调 ----------
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient*) override {
        s_info.connected = true;
    }

    void onDisconnect(NimBLEClient*, int) override {
        s_info.connected = false;
        s_info.connectedName[0] = '\0';
        s_info.connectedAddress[0] = '\0';
    }
};

static ClientCallbacks s_clientCallbacks;

// ===================== 初始化 =====================
bool Bluetooth_Init() {
    if (s_initOk) return true;

    memset(&s_info, 0, sizeof(s_info));

    s_info.enabled = false;
    s_info.scanning = false;
    s_info.connected = false;

    s_initOk = true;
    return true;
}

// ===================== 开关 =====================
bool Bluetooth_Enable(bool en) {
    if (en == s_info.enabled) return true;

    if (en) {

        if (!s_nimbleInited) {

            // ✅ 必须有名字（否则手机扫不到）
            NimBLEDevice::init("ESP32_BLE");

            NimBLEDevice::setPower(ESP_PWR_LVL_P9);
            s_nimbleInited = true;

            // ✅ 正确的 NimBLE 广播写法
            NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();

            NimBLEAdvertisementData advData;
            advData.setName("ESP32_BLE");

            NimBLEAdvertisementData scanResp;
            scanResp.setName("ESP32_BLE");

            adv->setAdvertisementData(advData);
            adv->setScanResponseData(scanResp);

            adv->start();   // 🚨 必须启动
        }

        // ---------- 扫描 ----------
        if (!s_scan) {
            s_scan = NimBLEDevice::getScan();
            if (s_scan) {
                s_scan->setScanCallbacks(&s_advCallbacks, false);
                s_scan->setActiveScan(true);

                // ✅ 关键修复：否则只看到1个设备
                s_scan->setDuplicateFilter(0);

                s_scan->setInterval(45);
                s_scan->setWindow(15);
            }
        }

        // ---------- 客户端 ----------
        if (!s_client) {
            s_client = NimBLEDevice::createClient();
            if (s_client) {
                s_client->setClientCallbacks(&s_clientCallbacks, false);
            }
        }

        s_info.enabled = true;
        return true;

    } else {

        Bluetooth_StopScan();
        Bluetooth_Disconnect();

        if (s_client) {
            NimBLEDevice::deleteClient(s_client);
            s_client = nullptr;
        }

        memset(&s_info, 0, sizeof(s_info));
        s_info.enabled = false;
        return true;
    }
}

bool Bluetooth_IsEnabled() {
    return s_info.enabled;
}

// ===================== 扫描 =====================
bool Bluetooth_StartScan(uint32_t scanMs) {
    if (!s_info.enabled || !s_scan) return false;
    if (s_info.scanning) return true;

    for (int i = 0; i < 8; i++) {
        delete s_devices[i];
        s_devices[i] = nullptr;
    }

    memset(s_info.devices, 0, sizeof(s_info.devices));
    s_info.deviceCount = 0;

    s_info.scanning = true;

    // ✅ 防止缓存设备
    s_scan->clearResults();

    s_scan->start(scanMs / 1000.0f, false, true);

    return true;
}

void Bluetooth_StopScan() {
    if (s_scan && s_info.scanning) {
        s_scan->stop();
        s_info.scanning = false;
    }
}

// ===================== 连接 =====================
bool Bluetooth_Connect(uint8_t index) {
    if (!s_info.enabled || !s_client) return false;
    if (index >= s_info.deviceCount || !s_devices[index]) return false;

    Bluetooth_StopScan();

    if (s_client->isConnected()) {
        s_client->disconnect();
    }

    if (!s_client->connect(s_devices[index])) {
        s_info.connected = false;
        return false;
    }

    strncpy(s_info.connectedName,
            s_info.devices[index].name,
            sizeof(s_info.connectedName) - 1);

    strncpy(s_info.connectedAddress,
            s_info.devices[index].address,
            sizeof(s_info.connectedAddress) - 1);

    s_info.connected = true;
    return true;
}

void Bluetooth_Disconnect() {
    if (s_client && s_client->isConnected()) {
        s_client->disconnect();
    }

    s_info.connected = false;
    s_info.connectedName[0] = '\0';
    s_info.connectedAddress[0] = '\0';
}

// ===================== 信息 =====================
void Bluetooth_GetInfo(BluetoothInfo_t* info) {
    if (!info) return;
    memcpy(info, &s_info, sizeof(BluetoothInfo_t));
}

// ===================== 循环 =====================
void Bluetooth_Update() {}

} // namespace HAL