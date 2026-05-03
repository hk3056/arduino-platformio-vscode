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
static bool s_nimbleInited = false;  // 全局跟踪
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;   // 新增互斥锁


class AdvCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        if (!dev) return;
        // 移除 isConnectable 限制，显示所有设备
        std::string addr = dev->getAddress().toString();
        if (addr.empty()) return;

        portENTER_CRITICAL(&s_mux);
        int cnt = s_info.deviceCount;
        bool exists = false;
        for (int i = 0; i < cnt; i++) {
            if (addr == s_info.devices[i].address) {
                exists = true;
                break;
            }
        }
        if (exists || cnt >= 8) {
            portEXIT_CRITICAL(&s_mux);
            return;
        }

        uint8_t idx = cnt;
        std::string name = dev->getName().empty() ? "Unknown" : dev->getName();
        strncpy(s_info.devices[idx].name, name.c_str(), sizeof(s_info.devices[idx].name) - 1);
        strncpy(s_info.devices[idx].address, addr.c_str(), sizeof(s_info.devices[idx].address) - 1);
        s_info.devices[idx].rssi = dev->getRSSI();

        delete s_devices[idx];
        s_devices[idx] = new NimBLEAdvertisedDevice(*dev);
        s_info.deviceCount++;
        portEXIT_CRITICAL(&s_mux);
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        (void)results; (void)reason;
        s_info.scanning = false;
    }
};
static AdvCallbacks s_advCallbacks;

// ---------- 客户端回调 ----------
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        (void)pClient;
        s_info.connected = true;
    }
    void onDisconnect(NimBLEClient* pClient, int reason) override {
        (void)pClient; (void)reason;
        s_info.connected = false;
        s_info.connectedName[0] = '\0';
        s_info.connectedAddress[0] = '\0';
    }
    bool onConnParamsUpdateRequest(NimBLEClient* pClient,
                                   const ble_gap_upd_params* params) override {
        (void)pClient; (void)params;
        return true;
    }
};
static ClientCallbacks s_clientCallbacks;

// ===================== 基础管理 =====================
bool Bluetooth_Init() {
    if (s_initOk) return true;
    memset(&s_info, 0, sizeof(s_info));
    s_info.enabled = false;
    s_info.scanning = false;
    s_info.connected = false;
    return true;
}

bool Bluetooth_Enable(bool en) {
    if (en == s_info.enabled) return true;
    if (en) {
        if (!s_nimbleInited) {
            NimBLEDevice::init("");
            NimBLEDevice::setPower(ESP_PWR_LVL_P9);
            s_nimbleInited = true;
        }

        if (!s_scan) {
            s_scan = NimBLEDevice::getScan();
            if (s_scan) {
                s_scan->setScanCallbacks(&s_advCallbacks, false);
                s_scan->setActiveScan(true);
                s_scan->setInterval(45);
                s_scan->setWindow(15);
                s_scan->setDuplicateFilter(2);
            }
        }

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

bool Bluetooth_IsEnabled() { return s_info.enabled; }

// ===================== 扫描 =====================
bool Bluetooth_StartScan(uint32_t scanMs) {
    if (!s_info.enabled || !s_scan) return false;
    if (s_info.scanning) return true;

    // 清空旧结果
    for (int i = 0; i < 8; i++) {
        delete s_devices[i];
        s_devices[i] = nullptr;
    }
    memset(s_info.devices, 0, sizeof(s_info.devices));
    s_info.deviceCount = 0;
    s_info.scanning = true;
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

    strncpy(s_info.connectedName, s_info.devices[index].name,
            sizeof(s_info.connectedName) - 1);
    strncpy(s_info.connectedAddress, s_info.devices[index].address,
            sizeof(s_info.connectedAddress) - 1);
    s_info.connected = true;

    // 连接后自动发现服务（不阻塞）
    // 实际使用中可调用 Bluetooth_DiscoverServices() 按需触发
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

// ===================== 服务发现 =====================
bool Bluetooth_DiscoverServices() {
    if (!s_client || !s_client->isConnected()) return false;
    // NimBLE 不同版本返回不同：这里以 vector 对象处理（直接判断是否为空）
    auto services = s_client->getServices(true);
    return !services.empty();
}

void Bluetooth_ClearServices() {
   
    if (s_client) {
        s_client->deleteServices();
    }
}

// ===================== 数据读写 =====================
static NimBLERemoteService* getRemoteService(const char* serviceUUID) {
    if (!s_client || !s_client->isConnected()) return nullptr;
    NimBLERemoteService* svc = s_client->getService(NimBLEUUID(serviceUUID));
    if (!svc) {
        // 尝试强制发现
        s_client->getServices(true);
        svc = s_client->getService(NimBLEUUID(serviceUUID));
    }
    return svc;
}

static NimBLERemoteCharacteristic* getRemoteChar(const char* serviceUUID,
                                                 const char* charUUID) {
    NimBLERemoteService* svc = getRemoteService(serviceUUID);
    if (!svc) return nullptr;
    return svc->getCharacteristic(NimBLEUUID(charUUID));
}

bool Bluetooth_ReadCharacteristic(const char* serviceUUID,
                                  const char* charUUID,
                                  std::string& outValue) {
    auto ch = getRemoteChar(serviceUUID, charUUID);
    if (!ch || !ch->canRead()) return false;
    outValue = ch->readValue();
    return true;
}

bool Bluetooth_WriteCharacteristic(const char* serviceUUID,
                                   const char* charUUID,
                                   const uint8_t* data, size_t len,
                                   bool response) {
    auto ch = getRemoteChar(serviceUUID, charUUID);
    if (!ch || !ch->canWrite()) return false;
    return ch->writeValue(data, len, response);
}

bool Bluetooth_SubscribeNotification(const char* serviceUUID,
                                     const char* charUUID,
                                     NimBLERemoteCharacteristic::notify_callback cb,
                                     bool notifications) {
    auto ch = getRemoteChar(serviceUUID, charUUID);
    if (!ch || !ch->canNotify()) return false;
    return ch->subscribe(notifications, cb);
}

// ===================== 信息 =====================
void Bluetooth_GetInfo(BluetoothInfo_t* info) {
    if (!info) return;
    memcpy(info, &s_info, sizeof(BluetoothInfo_t));
}

// ===================== 周期任务 =====================
void Bluetooth_Update() {
    // 可在此处理超时重扫等逻辑
}

} // namespace HAL