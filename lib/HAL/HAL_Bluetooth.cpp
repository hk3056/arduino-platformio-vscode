#include "HAL_Bluetooth.h"

#include <string.h>

namespace HAL
{

/* 本机蓝牙名字 */
static const char* BT_LOCAL_NAME = "XTrack-Meter";

/* 标准 BLE 心率服务 */
static const char* HR_SERVICE_UUID = "180D";
static const char* HR_MEASUREMENT_UUID = "2A37";

/* 超过 5 秒没有收到心率，就认为心率无效 */
static const uint32_t HR_TIMEOUT_MS = 5000;

static BluetoothInfo_t s_info = {};

static NimBLEScan* s_scan = nullptr;
static NimBLEClient* s_client = nullptr;

static NimBLEAdvertisedDevice* s_devices[HAL_BT_MAX_DEVICES] = { nullptr };

static bool s_initOk = false;
static bool s_nimbleInited = false;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* ========================= 工具函数 ========================= */

static void ResetHeartRateState()
{
    portENTER_CRITICAL(&s_mux);

    s_info.heartRateServiceFound = false;
    s_info.heartRateNotifyEnabled = false;
    s_info.heartRateValid = false;
    s_info.heartRate = 0;
    s_info.heartRateLastTick = 0;

    portEXIT_CRITICAL(&s_mux);
}

static bool ParseHeartRateMeasurement(const uint8_t* data, size_t len, uint8_t* outBpm)
{
    if (!data || !outBpm || len < 2)
    {
        return false;
    }

    uint8_t flags = data[0];

    /*
     * BLE 心率标准格式：
     * bit0 = 0：心率是 8 位，data[1]
     * bit0 = 1：心率是 16 位，data[1] + data[2]
     */
    bool is16Bit = flags & 0x01;

    uint16_t bpm = 0;

    if (is16Bit)
    {
        if (len < 3)
        {
            return false;
        }

        bpm = data[1] | ((uint16_t)data[2] << 8);
    }
    else
    {
        bpm = data[1];
    }

    /*
     * 你的心率模块无效时可能会上报 0。
     * 这里把明显不合理的数据过滤掉。
     */
    if (bpm < 40 || bpm > 220)
    {
        return false;
    }

    *outBpm = (uint8_t)bpm;
    return true;
}

static void HeartRateNotifyCallback(
    NimBLERemoteCharacteristic* chr,
    uint8_t* data,
    size_t length,
    bool isNotify
)
{
    (void)chr;
    (void)isNotify;

    uint8_t bpm = 0;
    bool valid = ParseHeartRateMeasurement(data, length, &bpm);

    portENTER_CRITICAL(&s_mux);

    if (valid)
    {
        s_info.heartRate = bpm;
        s_info.heartRateValid = true;
        s_info.heartRateLastTick = millis();
    }
    else
    {
        s_info.heartRate = 0;
        s_info.heartRateValid = false;
        s_info.heartRateLastTick = millis();
    }

    portEXIT_CRITICAL(&s_mux);

    Serial.printf(
        "[BT][HR] notify len=%d, valid=%d, bpm=%u\n",
        (int)length,
        valid ? 1 : 0,
        bpm
    );
}

static void ClearSavedDeviceCopies()
{
    for (int i = 0; i < HAL_BT_MAX_DEVICES; i++)
    {
        delete s_devices[i];
        s_devices[i] = nullptr;
    }
}

static int FindDeviceIndexByAddressNoLock(const char* address)
{
    if (!address || !address[0])
    {
        return -1;
    }

    for (int i = 0; i < s_info.deviceCount; i++)
    {
        if (strcmp(s_info.devices[i].address, address) == 0)
        {
            return i;
        }
    }

    return -1;
}

static void ClearDeviceList()
{
    ClearSavedDeviceCopies();

    portENTER_CRITICAL(&s_mux);

    memset(s_info.devices, 0, sizeof(s_info.devices));
    s_info.deviceCount = 0;

    portEXIT_CRITICAL(&s_mux);
}

static void SaveOrUpdateDevice(const NimBLEAdvertisedDevice* dev)
{
    if (!dev)
    {
        return;
    }

    std::string addr = dev->getAddress().toString();

    if (addr.empty())
    {
        return;
    }

    std::string name = dev->getName();

    if (name.empty())
    {
        name = "Unknown";
    }

    NimBLEAdvertisedDevice* copyDev = new NimBLEAdvertisedDevice(*dev);

    portENTER_CRITICAL(&s_mux);

    int index = FindDeviceIndexByAddressNoLock(addr.c_str());

    if (index < 0)
    {
        if (s_info.deviceCount >= HAL_BT_MAX_DEVICES)
        {
            portEXIT_CRITICAL(&s_mux);
            delete copyDev;
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
    s_devices[index] = copyDev;

    portEXIT_CRITICAL(&s_mux);

    Serial.printf(
        "[BT] found/update: %s / %s / RSSI=%d / count=%d\n",
        name.c_str(),
        addr.c_str(),
        dev->getRSSI(),
        s_info.deviceCount
    );
}

/* ========================= 扫描回调 ========================= */

class AdvCallbacks : public NimBLEScanCallbacks
{
public:
    void onResult(const NimBLEAdvertisedDevice* dev) override
    {
        SaveOrUpdateDevice(dev);
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override
    {
        portENTER_CRITICAL(&s_mux);
        s_info.scanning = false;
        portEXIT_CRITICAL(&s_mux);

        Serial.printf(
            "[BT] scan end, results=%d, saved=%d, reason=%d\n",
            results.getCount(),
            s_info.deviceCount,
            reason
        );
    }
};

static AdvCallbacks s_advCallbacks;

/* ========================= 客户端回调 ========================= */

class ClientCallbacks : public NimBLEClientCallbacks
{
public:
    void onConnect(NimBLEClient*) override
    {
        portENTER_CRITICAL(&s_mux);
        s_info.connected = true;
        portEXIT_CRITICAL(&s_mux);

        Serial.println("[BT] client connected callback");
    }

    void onDisconnect(NimBLEClient*, int reason) override
    {
        portENTER_CRITICAL(&s_mux);

        s_info.connected = false;
        s_info.connectedName[0] = '\0';
        s_info.connectedAddress[0] = '\0';

        s_info.heartRateServiceFound = false;
        s_info.heartRateNotifyEnabled = false;
        s_info.heartRateValid = false;
        s_info.heartRate = 0;
        s_info.heartRateLastTick = 0;

        portEXIT_CRITICAL(&s_mux);

        Serial.printf("[BT] disconnected, reason=%d\n", reason);
    }
};

static ClientCallbacks s_clientCallbacks;

/* ========================= 初始化 ========================= */

bool Bluetooth_Init()
{
    if (s_initOk)
    {
        return true;
    }

    memset(&s_info, 0, sizeof(s_info));

    s_info.enabled = false;
    s_info.scanning = false;
    s_info.connected = false;

    ClearSavedDeviceCopies();

    s_initOk = true;

    return true;
}

/* ========================= 蓝牙开关 ========================= */

bool Bluetooth_Enable(bool en)
{
    if (!s_initOk)
    {
        Bluetooth_Init();
    }

    if (en == s_info.enabled)
    {
        return true;
    }

    if (en)
    {
        if (!s_nimbleInited)
        {
            NimBLEDevice::init(BT_LOCAL_NAME);

            /*
             * 设置蓝牙发射功率。
             * 9 一般对应较高功率，便于扫描到心率模块。
             */
            NimBLEDevice::setPower(9);

            s_nimbleInited = true;
        }

        if (!s_scan)
        {
            s_scan = NimBLEDevice::getScan();

            if (s_scan)
            {
                s_scan->setScanCallbacks(&s_advCallbacks, false);
                s_scan->setActiveScan(true);
                s_scan->setDuplicateFilter(0);
                s_scan->setInterval(60);
                s_scan->setWindow(45);
            }
        }

        if (!s_client)
        {
            s_client = NimBLEDevice::createClient();

            if (s_client)
            {
                s_client->setClientCallbacks(&s_clientCallbacks, false);
                s_client->setConnectTimeout(5000);
            }
        }

        portENTER_CRITICAL(&s_mux);

        s_info.enabled = true;
        s_info.scanning = false;
        s_info.connected = false;

        portEXIT_CRITICAL(&s_mux);

        ResetHeartRateState();
        ClearDeviceList();

        Serial.println("[BT] enabled");

        return true;
    }

    Bluetooth_StopScan();
    Bluetooth_Disconnect();

    if (s_client)
    {
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

bool Bluetooth_IsEnabled()
{
    bool en = false;

    portENTER_CRITICAL(&s_mux);
    en = s_info.enabled;
    portEXIT_CRITICAL(&s_mux);

    return en;
}

/* ========================= 扫描 ========================= */

bool Bluetooth_StartScan(uint32_t scanMs)
{
    if (!s_info.enabled || !s_scan)
    {
        return false;
    }

    if (s_info.scanning)
    {
        return true;
    }

    ClearDeviceList();

    portENTER_CRITICAL(&s_mux);
    s_info.scanning = true;
    portEXIT_CRITICAL(&s_mux);

    s_scan->clearResults();

    if (scanMs == 0)
    {
        scanMs = 4000;
    }

    Serial.printf("[BT] start scan %lu ms\n", (unsigned long)scanMs);

    bool ok = s_scan->start(scanMs, false, true);

    if (!ok)
    {
        portENTER_CRITICAL(&s_mux);
        s_info.scanning = false;
        portEXIT_CRITICAL(&s_mux);

        Serial.println("[BT] scan start failed");
    }

    return ok;
}

void Bluetooth_StopScan()
{
    if (s_scan && s_info.scanning)
    {
        s_scan->stop();
    }

    portENTER_CRITICAL(&s_mux);
    s_info.scanning = false;
    portEXIT_CRITICAL(&s_mux);
}

/* ========================= 连接 ========================= */

bool Bluetooth_Connect(uint8_t index)
{
    if (!s_info.enabled || !s_client)
    {
        return false;
    }

    if (index >= s_info.deviceCount || !s_devices[index])
    {
        return false;
    }

    Bluetooth_StopScan();

    if (s_client->isConnected())
    {
        s_client->disconnect();
        delay(200);
    }

    ResetHeartRateState();

    char name[40] = { 0 };
    char address[24] = { 0 };

    portENTER_CRITICAL(&s_mux);

    strncpy(name, s_info.devices[index].name, sizeof(name) - 1);
    strncpy(address, s_info.devices[index].address, sizeof(address) - 1);

    portEXIT_CRITICAL(&s_mux);

    Serial.printf("[BT] connect to: %s / %s\n", name, address);

    bool ok = s_client->connect(s_devices[index]);

    if (!ok)
    {
        portENTER_CRITICAL(&s_mux);

        s_info.connected = false;
        s_info.connectedName[0] = '\0';
        s_info.connectedAddress[0] = '\0';

        portEXIT_CRITICAL(&s_mux);

        ResetHeartRateState();

        Serial.println("[BT] connect failed");

        return false;
    }

    portENTER_CRITICAL(&s_mux);

    strncpy(s_info.connectedName, name, sizeof(s_info.connectedName) - 1);
    strncpy(s_info.connectedAddress, address, sizeof(s_info.connectedAddress) - 1);

    s_info.connected = true;

    portEXIT_CRITICAL(&s_mux);

    Serial.println("[BT] connected");

    /*
     * 连接成功后自动订阅心率服务。
     * 如果连接的是踏频模块，这里订阅失败也没关系。
     */
    if (Bluetooth_SubscribeHeartRate())
    {
        Serial.println("[BT][HR] subscribe OK");
    }
    else
    {
        Serial.println("[BT][HR] subscribe failed or not heart rate device");
    }

    return true;
}

void Bluetooth_Disconnect()
{
    if (s_client && s_client->isConnected())
    {
        s_client->disconnect();
    }

    portENTER_CRITICAL(&s_mux);

    s_info.connected = false;
    s_info.connectedName[0] = '\0';
    s_info.connectedAddress[0] = '\0';

    s_info.heartRateServiceFound = false;
    s_info.heartRateNotifyEnabled = false;
    s_info.heartRateValid = false;
    s_info.heartRate = 0;
    s_info.heartRateLastTick = 0;

    portEXIT_CRITICAL(&s_mux);
}

/* ========================= 心率 ========================= */

bool Bluetooth_SubscribeHeartRate()
{
    if (!s_client || !s_client->isConnected())
    {
        return false;
    }

    NimBLERemoteService* service = s_client->getService(HR_SERVICE_UUID);

    if (!service)
    {
        portENTER_CRITICAL(&s_mux);
        s_info.heartRateServiceFound = false;
        s_info.heartRateNotifyEnabled = false;
        portEXIT_CRITICAL(&s_mux);

        Serial.println("[BT][HR] service 180D not found");

        return false;
    }

    portENTER_CRITICAL(&s_mux);
    s_info.heartRateServiceFound = true;
    portEXIT_CRITICAL(&s_mux);

    NimBLERemoteCharacteristic* chr = service->getCharacteristic(HR_MEASUREMENT_UUID);

    if (!chr)
    {
        portENTER_CRITICAL(&s_mux);
        s_info.heartRateNotifyEnabled = false;
        portEXIT_CRITICAL(&s_mux);

        Serial.println("[BT][HR] characteristic 2A37 not found");

        return false;
    }

    /*
     * 先读一次。
     * 有些模块刚连接时读到 0，是正常的，后面 notify 会更新。
     */
    if (chr->canRead())
    {
        std::string value = chr->readValue();

        uint8_t bpm = 0;
        bool valid = ParseHeartRateMeasurement(
            (const uint8_t*)value.data(),
            value.size(),
            &bpm
        );

        portENTER_CRITICAL(&s_mux);

        s_info.heartRate = valid ? bpm : 0;
        s_info.heartRateValid = valid;
        s_info.heartRateLastTick = millis();

        portEXIT_CRITICAL(&s_mux);

        Serial.printf("[BT][HR] read valid=%d, bpm=%u\n", valid ? 1 : 0, bpm);
    }

    if (!chr->canNotify() && !chr->canIndicate())
    {
        portENTER_CRITICAL(&s_mux);
        s_info.heartRateNotifyEnabled = false;
        portEXIT_CRITICAL(&s_mux);

        Serial.println("[BT][HR] characteristic can not notify/indicate");

        return false;
    }

    bool useNotify = chr->canNotify();

    bool ok = chr->subscribe(useNotify, HeartRateNotifyCallback);

    portENTER_CRITICAL(&s_mux);
    s_info.heartRateNotifyEnabled = ok;
    portEXIT_CRITICAL(&s_mux);

    return ok;
}

bool Bluetooth_IsHeartRateValid()
{
    bool valid = false;

    portENTER_CRITICAL(&s_mux);
    valid = s_info.heartRateValid;
    portEXIT_CRITICAL(&s_mux);

    return valid;
}

uint8_t Bluetooth_GetHeartRate()
{
    uint8_t bpm = 0;

    portENTER_CRITICAL(&s_mux);
    bpm = s_info.heartRate;
    portEXIT_CRITICAL(&s_mux);

    return bpm;
}

/* ========================= 服务发现 ========================= */

bool Bluetooth_DiscoverServices()
{
    if (!s_client || !s_client->isConnected())
    {
        return false;
    }

    return Bluetooth_SubscribeHeartRate();
}

void Bluetooth_ClearServices()
{
    ResetHeartRateState();
}

/* ========================= 通用读写接口 ========================= */

bool Bluetooth_ReadCharacteristic(
    const char* serviceUUID,
    const char* charUUID,
    std::string& outValue
)
{
    outValue.clear();

    if (!s_client || !s_client->isConnected())
    {
        return false;
    }

    if (!serviceUUID || !charUUID)
    {
        return false;
    }

    NimBLERemoteService* service = s_client->getService(serviceUUID);

    if (!service)
    {
        return false;
    }

    NimBLERemoteCharacteristic* chr = service->getCharacteristic(charUUID);

    if (!chr)
    {
        return false;
    }

    if (!chr->canRead())
    {
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
)
{
    if (!s_client || !s_client->isConnected())
    {
        return false;
    }

    if (!serviceUUID || !charUUID || !data || len == 0)
    {
        return false;
    }

    NimBLERemoteService* service = s_client->getService(serviceUUID);

    if (!service)
    {
        return false;
    }

    NimBLERemoteCharacteristic* chr = service->getCharacteristic(charUUID);

    if (!chr)
    {
        return false;
    }

    if (!chr->canWrite() && !chr->canWriteNoResponse())
    {
        return false;
    }

    return chr->writeValue(data, len, response);
}

bool Bluetooth_SubscribeNotification(
    const char* serviceUUID,
    const char* charUUID,
    NimBLERemoteCharacteristic::notify_callback cb,
    bool notifications
)
{
    if (!s_client || !s_client->isConnected())
    {
        return false;
    }

    if (!serviceUUID || !charUUID)
    {
        return false;
    }

    NimBLERemoteService* service = s_client->getService(serviceUUID);

    if (!service)
    {
        return false;
    }

    NimBLERemoteCharacteristic* chr = service->getCharacteristic(charUUID);

    if (!chr)
    {
        return false;
    }

    if (!chr->canNotify() && !chr->canIndicate())
    {
        return false;
    }

    return chr->subscribe(notifications, cb);
}

/* ========================= 信息获取 ========================= */

void Bluetooth_GetInfo(BluetoothInfo_t* info)
{
    if (!info)
    {
        return;
    }

    portENTER_CRITICAL(&s_mux);
    memcpy(info, &s_info, sizeof(BluetoothInfo_t));
    portEXIT_CRITICAL(&s_mux);
}

/* ========================= 循环更新 ========================= */

void Bluetooth_Update()
{
    if (!s_info.enabled)
    {
        return;
    }

    if (s_scan && s_info.scanning && !s_scan->isScanning())
    {
        portENTER_CRITICAL(&s_mux);
        s_info.scanning = false;
        portEXIT_CRITICAL(&s_mux);
    }

    if (s_client)
    {
        bool realConnected = s_client->isConnected();

        if (!realConnected && s_info.connected)
        {
            portENTER_CRITICAL(&s_mux);

            s_info.connected = false;
            s_info.connectedName[0] = '\0';
            s_info.connectedAddress[0] = '\0';

            s_info.heartRateServiceFound = false;
            s_info.heartRateNotifyEnabled = false;
            s_info.heartRateValid = false;
            s_info.heartRate = 0;
            s_info.heartRateLastTick = 0;

            portEXIT_CRITICAL(&s_mux);
        }
    }

    /*
     * 心率超时处理：
     * 手指拿开、模块停止发送、连接异常时，Ride 页面显示 ---
     */
    uint32_t now = millis();

    portENTER_CRITICAL(&s_mux);

    if (
        s_info.heartRateValid &&
        s_info.heartRateLastTick > 0 &&
        now - s_info.heartRateLastTick > HR_TIMEOUT_MS
    )
    {
        s_info.heartRateValid = false;
        s_info.heartRate = 0;
    }

    portEXIT_CRITICAL(&s_mux);
}

} // namespace HAL