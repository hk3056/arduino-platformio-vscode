#include "HAL_Bluetooth.h"

namespace HAL {

// ---------- 静态变量 ----------
static BluetoothInfo_t s_info = {};
static NimBLEScan* s_scan = nullptr;
static NimBLEClient* s_client = nullptr;

static NimBLEAdvertisedDevice* s_devices[HAL_BT_MAX_DEVICES] = { nullptr };

static bool s_initOk = false;
static bool s_nimbleInited = false;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static const char* BT_LOCAL_NAME = "ESP32_BLE";

// ---------- 工具函数 ----------
static void ClearSavedDeviceCopies() {
    for (int i = 0; i < HAL_BT_MAX_DEVICES; i++) {
        delete s_devices[i];
        s_devices[i] = nullptr;
    }
}

static void ClearDeviceList() {
    portENTER_CRITICAL(&s_mux);

    memset(s_info.devices, 0, sizeof(s_info.devices));
    s_info.deviceCount = 0;

    portEXIT_CRITICAL(&s_mux);

    ClearSavedDeviceCopies();
}

static int FindDeviceIndexByAddress(const char* address) {
    if (!address || !address[0]) return -1;

    for (int i = 0; i < s_info.deviceCount; i++) {
        if (strcmp(s_info.devices[i].address, address) == 0) {
            return i;
        }
    }

    return -1;
}

static void SaveOrUpdateDevice(const NimBLEAdvertisedDevice* dev) {
    if (!dev) return;

    std::string addr = dev->getAddress().toString();
    if (addr.empty()) return;

    std::string name = dev->getName();
    if (name.empty()) {
        name = "Unknown";
    }

    portENTER_CRITICAL(&s_mux);

    int index = FindDeviceIndexByAddress(addr.c_str());

    if (index < 0) {
        if (s_info.deviceCount >= HAL_BT_MAX_DEVICES) {
            portEXIT_CRITICAL(&s_mux);
            return;
        }

        index = s_info.deviceCount;
        s_info.deviceCount++;
    }

    BluetoothDeviceItem_t* item = &s_info.devices[index];

    memset(item, 0, sizeof(BluetoothDeviceItem_t));

    strncpy(item->name, name.c_str(), sizeof(item->name) - 1);
    strncpy(item->address, addr.c_str(), sizeof(item->address) - 1);
    item->rssi = dev->getRSSI();

    delete s_devices[index];
    s_devices[index] = new NimBLEAdvertisedDevice(*dev);

    portEXIT_CRITICAL(&s_mux);

    Serial.printf("[BT] found/update: %s / %s / RSSI=%d / count=%d\n",
                  item->name,
                  item->address,
                  item->rssi,
                  s_info.deviceCount);
}

// ---------- 扫描回调 ----------
class AdvCallbacks : public NimBLEScanCallbacks {
public:
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        SaveOrUpdateDevice(dev);
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        portENTER_CRITICAL(&s_mux);
        s_info.scanning = false;
        portEXIT_CRITICAL(&s_mux);

        Serial.printf("[BT] scan end, results=%d, saved=%d, reason=%d\n",
                      results.getCount(),
                      s_info.deviceCount,
                      reason);
    }
};

static AdvCallbacks s_advCallbacks;

// ---------- 客户端回调 ----------
class ClientCallbacks : public NimBLEClientCallbacks {
public:
    void onConnect(NimBLEClient*) override {
        portENTER_CRITICAL(&s_mux);
        s_info.connected = true;
        portEXIT_CRITICAL(&s_mux);
    }

    void onDisconnect(NimBLEClient*, int) override {
        portENTER_CRITICAL(&s_mux);

        s_info.connected = false;
        s_info.connectedName[0] = '\0';
        s_info.connectedAddress[0] = '\0';

        portEXIT_CRITICAL(&s_mux);
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

    ClearSavedDeviceCopies();

    s_initOk = true;

    return true;
}

// ===================== 开关 =====================
bool Bluetooth_Enable(bool en) {
    if (!s_initOk) {
        Bluetooth_Init();
    }

    if (en == s_info.enabled) {
        return true;
    }

    if (en) {
        if (!s_nimbleInited) {
            NimBLEDevice::init(BT_LOCAL_NAME);
            NimBLEDevice::setPower(ESP_PWR_LVL_P9);

            s_nimbleInited = true;

            /*
             * 让设备自身也可以被手机扫描到。
             * 这不影响扫描周围 BLE 设备。
             */
            NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
            if (adv) {
                NimBLEAdvertisementData advData;
                advData.setName(BT_LOCAL_NAME);

                NimBLEAdvertisementData scanResp;
                scanResp.setName(BT_LOCAL_NAME);

                adv->setAdvertisementData(advData);
                adv->setScanResponseData(scanResp);
                adv->start();
            }
        }

        if (!s_scan) {
            s_scan = NimBLEDevice::getScan();

            if (s_scan) {
                s_scan->setScanCallbacks(&s_advCallbacks, false);
                s_scan->setActiveScan(true);

                /*
                 * 关键：
                 * 0 = 不过滤重复广播。
                 * 这样同一个设备多次广播时可以更新 RSSI/名称。
                 */
                s_scan->setDuplicateFilter(0);

                /*
                 * 原来 window=15 太短，容易漏设备。
                 * 这里增大扫描窗口，提高扫到多个设备的概率。
                 */
                s_scan->setInterval(60);
                s_scan->setWindow(45);
            }
        }

        if (!s_client) {
            s_client = NimBLEDevice::createClient();

            if (s_client) {
                s_client->setClientCallbacks(&s_clientCallbacks, false);
            }
        }

        portENTER_CRITICAL(&s_mux);

        s_info.enabled = true;
        s_info.scanning = false;
        s_info.connected = false;

        portEXIT_CRITICAL(&s_mux);

        ClearDeviceList();

        Serial.println("[BT] enabled");

        return true;
    }

    Bluetooth_StopScan();
    Bluetooth_Disconnect();

    if (s_client) {
        NimBLEDevice::deleteClient(s_client);
        s_client = nullptr;
    }

    ClearDeviceList();

    portENTER_CRITICAL(&s_mux);

    memset(&s_info, 0, sizeof(s_info));
    s_info.enabled = false;

    portEXIT_CRITICAL(&s_mux);

    Serial.println("[BT] disabled");

    return true;
}

bool Bluetooth_IsEnabled() {
    return s_info.enabled;
}

// ===================== 扫描 =====================
bool Bluetooth_StartScan(uint32_t scanMs) {
    if (!s_info.enabled || !s_scan) {
        return false;
    }

    if (s_info.scanning) {
        return true;
    }

    /*
     * 关键修改：
     * 不再每次扫描前清空 s_info.devices。
     * 这样多次扫描可以慢慢累积设备，不会出现“刚扫到 1 个就永远只有 1 个”的情况。
     */
    portENTER_CRITICAL(&s_mux);
    s_info.scanning = true;
    portEXIT_CRITICAL(&s_mux);

    s_scan->clearResults();

    uint32_t seconds = (scanMs + 999) / 1000;
    if (seconds == 0) {
        seconds = 1;
    }

    Serial.printf("[BT] start scan %lu ms, current saved=%d\n",
                  (unsigned long)scanMs,
                  s_info.deviceCount);

    bool ok = s_scan->start(seconds, false, true);

    if (!ok) {
        portENTER_CRITICAL(&s_mux);
        s_info.scanning = false;
        portEXIT_CRITICAL(&s_mux);
    }

    return ok;
}

void Bluetooth_StopScan() {
    if (s_scan && s_info.scanning) {
        s_scan->stop();
    }

    portENTER_CRITICAL(&s_mux);
    s_info.scanning = false;
    portEXIT_CRITICAL(&s_mux);
}

// ===================== 连接 =====================
bool Bluetooth_Connect(uint8_t index) {
    if (!s_info.enabled || !s_client) {
        return false;
    }

    if (index >= s_info.deviceCount || !s_devices[index]) {
        return false;
    }

    Bluetooth_StopScan();

    if (s_client->isConnected()) {
        s_client->disconnect();
    }

    Serial.printf("[BT] connect to: %s / %s\n",
                  s_info.devices[index].name,
                  s_info.devices[index].address);

    bool ok = s_client->connect(s_devices[index]);

    if (!ok) {
        portENTER_CRITICAL(&s_mux);
        s_info.connected = false;
        s_info.connectedName[0] = '\0';
        s_info.connectedAddress[0] = '\0';
        portEXIT_CRITICAL(&s_mux);

        Serial.println("[BT] connect failed");

        return false;
    }

    portENTER_CRITICAL(&s_mux);

    strncpy(s_info.connectedName,
            s_info.devices[index].name,
            sizeof(s_info.connectedName) - 1);

    strncpy(s_info.connectedAddress,
            s_info.devices[index].address,
            sizeof(s_info.connectedAddress) - 1);

    s_info.connected = true;

    portEXIT_CRITICAL(&s_mux);

    Serial.println("[BT] connected");

    return true;
}

void Bluetooth_Disconnect() {
    if (s_client && s_client->isConnected()) {
        s_client->disconnect();
    }

    portENTER_CRITICAL(&s_mux);

    s_info.connected = false;
    s_info.connectedName[0] = '\0';
    s_info.connectedAddress[0] = '\0';

    portEXIT_CRITICAL(&s_mux);
}

// ===================== 服务发现 =====================
bool Bluetooth_DiscoverServices() {
    if (!s_client || !s_client->isConnected()) {
        return false;
    }

    /*
     * 这里先保留成基础可用版本。
     * 后续如果你要做心率、码表、传感器数据读取，再根据具体 UUID 完善。
     */
    return true;
}

void Bluetooth_ClearServices() {
    /*
     * 当前没有额外缓存服务列表，所以这里留空。
     */
}

// ===================== 数据读写 =====================
bool Bluetooth_ReadCharacteristic(
    const char* serviceUUID,
    const char* charUUID,
    std::string& outValue
) {
    outValue.clear();

    if (!s_client || !s_client->isConnected()) {
        return false;
    }

    if (!serviceUUID || !charUUID) {
        return false;
    }

    NimBLERemoteService* service = s_client->getService(serviceUUID);
    if (!service) {
        return false;
    }

    NimBLERemoteCharacteristic* chr = service->getCharacteristic(charUUID);
    if (!chr) {
        return false;
    }

    if (!chr->canRead()) {
        return false;
    }

    outValue = chr->readValue();

    return true;
}

bool Bluetooth_WriteCharacteristic(
    const char* serviceUUID,
    const char* charUUID,
    const uint8_t* data,
    size_t len,
    bool response
) {
    if (!s_client || !s_client->isConnected()) {
        return false;
    }

    if (!serviceUUID || !charUUID || !data || len == 0) {
        return false;
    }

    NimBLERemoteService* service = s_client->getService(serviceUUID);
    if (!service) {
        return false;
    }

    NimBLERemoteCharacteristic* chr = service->getCharacteristic(charUUID);
    if (!chr) {
        return false;
    }

    if (!chr->canWrite() && !chr->canWriteNoResponse()) {
        return false;
    }

    return chr->writeValue(data, len, response);
}

bool Bluetooth_SubscribeNotification(
    const char* serviceUUID,
    const char* charUUID,
    NimBLERemoteCharacteristic::notify_callback cb,
    bool notifications
) {
    if (!s_client || !s_client->isConnected()) {
        return false;
    }

    if (!serviceUUID || !charUUID) {
        return false;
    }

    NimBLERemoteService* service = s_client->getService(serviceUUID);
    if (!service) {
        return false;
    }

    NimBLERemoteCharacteristic* chr = service->getCharacteristic(charUUID);
    if (!chr) {
        return false;
    }

    if (!chr->canNotify() && !chr->canIndicate()) {
        return false;
    }

    return chr->subscribe(notifications, cb);
}

// ===================== 信息 =====================
void Bluetooth_GetInfo(BluetoothInfo_t* info) {
    if (!info) return;

    portENTER_CRITICAL(&s_mux);
    memcpy(info, &s_info, sizeof(BluetoothInfo_t));
    portEXIT_CRITICAL(&s_mux);
}

// ===================== 循环 =====================
void Bluetooth_Update() {
    if (!s_info.enabled) return;

    if (s_scan && s_info.scanning && !s_scan->isScanning()) {
        portENTER_CRITICAL(&s_mux);
        s_info.scanning = false;
        portEXIT_CRITICAL(&s_mux);
    }

    if (s_client) {
        bool realConnected = s_client->isConnected();

        if (!realConnected && s_info.connected) {
            portENTER_CRITICAL(&s_mux);
            s_info.connected = false;
            s_info.connectedName[0] = '\0';
            s_info.connectedAddress[0] = '\0';
            portEXIT_CRITICAL(&s_mux);
        }
    }
}

} // namespace HAL