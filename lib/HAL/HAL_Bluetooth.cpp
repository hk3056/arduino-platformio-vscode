#include "HAL_Bluetooth.h"
#include <cstring>

namespace HAL
{

/* =========================
   本机蓝牙名字
   ========================= */
static const char* BT_LOCAL_NAME = "XTrack-Meter";

/* 标准 BLE 心率服务 */
static const char* HR_SERVICE_UUID = "180D";
static const char* HR_MEASUREMENT_UUID = "2A37";

/* 标准 BLE 踏频 CSC 服务 */
static const char* CSC_SERVICE_UUID = "1816";
static const char* CSC_MEASUREMENT_UUID = "2A5B";

static const uint32_t HR_TIMEOUT_MS = 5000;
static const uint32_t CADENCE_TIMEOUT_MS = 5000;

static BluetoothInfo_t s_info = {};

static NimBLEScan* s_scan = nullptr;

/*
   两个客户端：
   s_hrClient 专门连接心率模块；
   s_cadClient 专门连接踏频模块。
*/
static NimBLEClient* s_hrClient = nullptr;
static NimBLEClient* s_cadClient = nullptr;

static NimBLEAdvertisedDevice* s_devices[HAL_BT_MAX_DEVICES] = { nullptr };

static bool s_initOk = false;
static bool s_nimbleInited = false;

static bool s_cadenceBaseValid = false;
static uint16_t s_lastCrankRev = 0;
static uint16_t s_lastCrankEventTime = 0;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* =========================
   状态工具
   ========================= */

static void UpdateConnectedStateLocked()
{
    s_info.connected = s_info.heartRateConnected || s_info.cadenceConnected;

    if (s_info.heartRateConnected)
    {
        strncpy(
            s_info.connectedName,
            s_info.heartRateName,
            sizeof(s_info.connectedName) - 1
        );

        strncpy(
            s_info.connectedAddress,
            s_info.heartRateAddress,
            sizeof(s_info.connectedAddress) - 1
        );
    }
    else if (s_info.cadenceConnected)
    {
        strncpy(
            s_info.connectedName,
            s_info.cadenceName,
            sizeof(s_info.connectedName) - 1
        );

        strncpy(
            s_info.connectedAddress,
            s_info.cadenceAddress,
            sizeof(s_info.connectedAddress) - 1
        );
    }
    else
    {
        s_info.connectedName[0] = '\0';
        s_info.connectedAddress[0] = '\0';
    }
}

static void ClearHeartRateLocked()
{
    s_info.heartRateConnected = false;
    s_info.heartRateName[0] = '\0';
    s_info.heartRateAddress[0] = '\0';

    s_info.heartRateServiceFound = false;
    s_info.heartRateNotifyEnabled = false;
    s_info.heartRateValid = false;
    s_info.heartRate = 0;
    s_info.heartRateLastTick = 0;

    UpdateConnectedStateLocked();
}

static void ClearCadenceLocked()
{
    s_info.cadenceConnected = false;
    s_info.cadenceName[0] = '\0';
    s_info.cadenceAddress[0] = '\0';

    s_info.cadenceServiceFound = false;
    s_info.cadenceNotifyEnabled = false;
    s_info.cadenceValid = false;
    s_info.cadenceRpm = 0;
    s_info.cadenceCrankRevCount = 0;
    s_info.cadenceLastEventTime = 0;
    s_info.cadenceLastTick = 0;

    s_cadenceBaseValid = false;
    s_lastCrankRev = 0;
    s_lastCrankEventTime = 0;

    UpdateConnectedStateLocked();
}

static void ResetHeartRateState()
{
    portENTER_CRITICAL(&s_mux);
    ClearHeartRateLocked();
    portEXIT_CRITICAL(&s_mux);
}

static void ResetCadenceState()
{
    portENTER_CRITICAL(&s_mux);
    ClearCadenceLocked();
    portEXIT_CRITICAL(&s_mux);
}

/* =========================
   心率解析
   ========================= */

static bool ParseHeartRateMeasurement(const uint8_t* data, size_t len, uint8_t* outBpm)
{
    if (!data || !outBpm || len < 2)
    {
        return false;
    }

    uint8_t flags = data[0];
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

/* =========================
   踏频 CSC 解析
   ========================= */

static bool ParseCSCMeasurement(
    const uint8_t* data,
    size_t len,
    uint16_t* outCrankRev,
    uint16_t* outCrankEventTime
)
{
    if (!data || !outCrankRev || !outCrankEventTime || len < 1)
    {
        return false;
    }

    uint8_t flags = data[0];

    bool wheelPresent = flags & 0x01;
    bool crankPresent = flags & 0x02;

    size_t offset = 1;

    /*
       如果包里带车轮数据，跳过车轮部分。
       Wheel data:
       uint32_t cumulative wheel revolutions
       uint16_t last wheel event time
    */
    if (wheelPresent)
    {
        if (len < offset + 6)
        {
            return false;
        }

        offset += 6;
    }

    if (!crankPresent)
    {
        return false;
    }

    if (len < offset + 4)
    {
        return false;
    }

    uint16_t crankRev =
        data[offset] |
        ((uint16_t)data[offset + 1] << 8);

    uint16_t crankEventTime =
        data[offset + 2] |
        ((uint16_t)data[offset + 3] << 8);

    *outCrankRev = crankRev;
    *outCrankEventTime = crankEventTime;

    return true;
}

static void CadenceNotifyCallback(
    NimBLERemoteCharacteristic* chr,
    uint8_t* data,
    size_t length,
    bool isNotify
)
{
    (void)chr;
    (void)isNotify;

    uint16_t crankRev = 0;
    uint16_t crankEventTime = 0;

    bool ok = ParseCSCMeasurement(
        data,
        length,
        &crankRev,
        &crankEventTime
    );

    if (!ok)
    {
        portENTER_CRITICAL(&s_mux);

        s_info.cadenceValid = false;
        s_info.cadenceRpm = 0;
        s_info.cadenceLastTick = millis();

        portEXIT_CRITICAL(&s_mux);

        Serial.printf("[BT][CAD] parse failed, len=%d\n", (int)length);
        return;
    }

    uint16_t rpm = 0;
    bool rpmValid = false;

    if (s_cadenceBaseValid)
    {
        uint16_t deltaRev = crankRev - s_lastCrankRev;
        uint16_t deltaTime = crankEventTime - s_lastCrankEventTime;

        /*
           CSC 协议中 crankEventTime 单位是 1/1024 秒。
           rpm = deltaRev * 60 * 1024 / deltaTime
        */
        if (deltaRev > 0 && deltaTime > 0)
        {
            uint32_t rpmCalc =
                ((uint32_t)deltaRev * 60UL * 1024UL) / deltaTime;

            if (rpmCalc <= 250)
            {
                rpm = (uint16_t)rpmCalc;
                rpmValid = true;
            }
        }
    }

    s_lastCrankRev = crankRev;
    s_lastCrankEventTime = crankEventTime;
    s_cadenceBaseValid = true;

    portENTER_CRITICAL(&s_mux);

    s_info.cadenceCrankRevCount = crankRev;
    s_info.cadenceLastEventTime = crankEventTime;
    s_info.cadenceLastTick = millis();

    if (rpmValid)
    {
        s_info.cadenceRpm = rpm;
        s_info.cadenceValid = true;
    }

    portEXIT_CRITICAL(&s_mux);

    Serial.printf(
        "[BT][CAD] notify len=%d, rev=%u, event=%u, valid=%d, rpm=%u\n",
        (int)length,
        crankRev,
        crankEventTime,
        rpmValid ? 1 : 0,
        rpm
    );
}

/* =========================
   扫描设备列表
   ========================= */

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

    char logName[40] = { 0 };
    char logAddr[24] = { 0 };
    int logRssi = dev->getRSSI();
    int logCount = 0;

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

    strncpy(logName, item->name, sizeof(logName) - 1);
    strncpy(logAddr, item->address, sizeof(logAddr) - 1);

    logRssi = item->rssi;
    logCount = s_info.deviceCount;

    portEXIT_CRITICAL(&s_mux);

    Serial.printf(
        "[BT] found/update: %s / %s / RSSI=%d / count=%d\n",
        logName,
        logAddr,
        logRssi,
        logCount
    );
}

/* =========================
   扫描回调
   ========================= */

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

/* =========================
   两个客户端回调
   ========================= */

class HeartRateClientCallbacks : public NimBLEClientCallbacks
{
public:
    void onConnect(NimBLEClient* client) override
    {
        (void)client;
        Serial.println("[BT][HR] client connected callback");
    }

    void onDisconnect(NimBLEClient* client, int reason) override
    {
        (void)client;

        portENTER_CRITICAL(&s_mux);
        ClearHeartRateLocked();
        portEXIT_CRITICAL(&s_mux);

        Serial.printf("[BT][HR] disconnected, reason=%d\n", reason);
    }
};

class CadenceClientCallbacks : public NimBLEClientCallbacks
{
public:
    void onConnect(NimBLEClient* client) override
    {
        (void)client;
        Serial.println("[BT][CAD] client connected callback");
    }

    void onDisconnect(NimBLEClient* client, int reason) override
    {
        (void)client;

        portENTER_CRITICAL(&s_mux);
        ClearCadenceLocked();
        portEXIT_CRITICAL(&s_mux);

        Serial.printf("[BT][CAD] disconnected, reason=%d\n", reason);
    }
};

static HeartRateClientCallbacks s_hrClientCallbacks;
static CadenceClientCallbacks s_cadClientCallbacks;

/* =========================
   初始化
   ========================= */

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

    s_cadenceBaseValid = false;
    s_lastCrankRev = 0;
    s_lastCrankEventTime = 0;

    ClearSavedDeviceCopies();

    s_initOk = true;

    return true;
}

/* =========================
   蓝牙开关
   ========================= */

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

        if (!s_hrClient)
        {
            s_hrClient = NimBLEDevice::createClient();

            if (s_hrClient)
            {
                s_hrClient->setClientCallbacks(&s_hrClientCallbacks, false);
                s_hrClient->setConnectTimeout(5000);
            }
        }

        if (!s_cadClient)
        {
            s_cadClient = NimBLEDevice::createClient();

            if (s_cadClient)
            {
                s_cadClient->setClientCallbacks(&s_cadClientCallbacks, false);
                s_cadClient->setConnectTimeout(5000);
            }
        }

        portENTER_CRITICAL(&s_mux);

        s_info.enabled = true;
        s_info.scanning = false;
        s_info.connected = false;

        ClearHeartRateLocked();
        ClearCadenceLocked();

        portEXIT_CRITICAL(&s_mux);

        ClearDeviceList();

        Serial.println("[BT] enabled");

        return true;
    }

    Bluetooth_StopScan();
    Bluetooth_Disconnect();

    if (s_hrClient)
    {
        NimBLEDevice::deleteClient(s_hrClient);
        s_hrClient = nullptr;
    }

    if (s_cadClient)
    {
        NimBLEDevice::deleteClient(s_cadClient);
        s_cadClient = nullptr;
    }

    ClearDeviceList();

    portENTER_CRITICAL(&s_mux);

    memset(&s_info, 0, sizeof(s_info));
    s_info.enabled = false;

    s_cadenceBaseValid = false;
    s_lastCrankRev = 0;
    s_lastCrankEventTime = 0;

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

/* =========================
   扫描
   ========================= */

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

/* =========================
   分开连接
   ========================= */

bool Bluetooth_ConnectHeartRate(uint8_t index)
{
    if (!s_info.enabled || !s_hrClient)
    {
        return false;
    }

    if (index >= s_info.deviceCount || !s_devices[index])
    {
        return false;
    }

    Bluetooth_StopScan();

    if (s_hrClient->isConnected())
    {
        s_hrClient->disconnect();
        delay(200);
    }

    ResetHeartRateState();

    char name[40] = { 0 };
    char address[24] = { 0 };

    portENTER_CRITICAL(&s_mux);

    strncpy(name, s_info.devices[index].name, sizeof(name) - 1);
    strncpy(address, s_info.devices[index].address, sizeof(address) - 1);

    portEXIT_CRITICAL(&s_mux);

    Serial.printf("[BT][HR] connect to: %s / %s\n", name, address);

    bool ok = s_hrClient->connect(s_devices[index]);

    if (!ok)
    {
        ResetHeartRateState();
        Serial.println("[BT][HR] connect failed");
        return false;
    }

    portENTER_CRITICAL(&s_mux);

    s_info.heartRateConnected = true;

    strncpy(
        s_info.heartRateName,
        name,
        sizeof(s_info.heartRateName) - 1
    );

    strncpy(
        s_info.heartRateAddress,
        address,
        sizeof(s_info.heartRateAddress) - 1
    );

    UpdateConnectedStateLocked();

    portEXIT_CRITICAL(&s_mux);

    if (!Bluetooth_SubscribeHeartRate())
    {
        Serial.println("[BT][HR] subscribe failed");

        if (s_hrClient->isConnected())
        {
            s_hrClient->disconnect();
        }

        ResetHeartRateState();

        return false;
    }

    Serial.println("[BT][HR] subscribe OK");

    return true;
}

bool Bluetooth_ConnectCadence(uint8_t index)
{
    if (!s_info.enabled || !s_cadClient)
    {
        return false;
    }

    if (index >= s_info.deviceCount || !s_devices[index])
    {
        return false;
    }

    Bluetooth_StopScan();

    if (s_cadClient->isConnected())
    {
        s_cadClient->disconnect();
        delay(200);
    }

    ResetCadenceState();

    char name[40] = { 0 };
    char address[24] = { 0 };

    portENTER_CRITICAL(&s_mux);

    strncpy(name, s_info.devices[index].name, sizeof(name) - 1);
    strncpy(address, s_info.devices[index].address, sizeof(address) - 1);

    portEXIT_CRITICAL(&s_mux);

    Serial.printf("[BT][CAD] connect to: %s / %s\n", name, address);

    bool ok = s_cadClient->connect(s_devices[index]);

    if (!ok)
    {
        ResetCadenceState();
        Serial.println("[BT][CAD] connect failed");
        return false;
    }

    portENTER_CRITICAL(&s_mux);

    s_info.cadenceConnected = true;

    strncpy(
        s_info.cadenceName,
        name,
        sizeof(s_info.cadenceName) - 1
    );

    strncpy(
        s_info.cadenceAddress,
        address,
        sizeof(s_info.cadenceAddress) - 1
    );

    UpdateConnectedStateLocked();

    portEXIT_CRITICAL(&s_mux);

    if (!Bluetooth_SubscribeCadence())
    {
        Serial.println("[BT][CAD] subscribe failed");

        if (s_cadClient->isConnected())
        {
            s_cadClient->disconnect();
        }

        ResetCadenceState();

        return false;
    }

    Serial.println("[BT][CAD] subscribe OK");

    return true;
}

/*
   兼容旧接口。
   如果旧代码还调用 Bluetooth_Connect(index)，这里会先尝试心率，再尝试踏频。
*/
bool Bluetooth_Connect(uint8_t index)
{
    if (Bluetooth_ConnectHeartRate(index))
    {
        return true;
    }

    delay(100);

    if (Bluetooth_ConnectCadence(index))
    {
        return true;
    }

    return false;
}

void Bluetooth_DisconnectHeartRate()
{
    if (s_hrClient && s_hrClient->isConnected())
    {
        s_hrClient->disconnect();
    }

    ResetHeartRateState();
}

void Bluetooth_DisconnectCadence()
{
    if (s_cadClient && s_cadClient->isConnected())
    {
        s_cadClient->disconnect();
    }

    ResetCadenceState();
}

void Bluetooth_Disconnect()
{
    Bluetooth_DisconnectHeartRate();
    Bluetooth_DisconnectCadence();
}

/* =========================
   心率
   ========================= */

bool Bluetooth_SubscribeHeartRate()
{
    if (!s_hrClient || !s_hrClient->isConnected())
    {
        return false;
    }

    NimBLERemoteService* service = s_hrClient->getService(HR_SERVICE_UUID);

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

    NimBLERemoteCharacteristic* chr =
        service->getCharacteristic(HR_MEASUREMENT_UUID);

    if (!chr)
    {
        portENTER_CRITICAL(&s_mux);
        s_info.heartRateNotifyEnabled = false;
        portEXIT_CRITICAL(&s_mux);

        Serial.println("[BT][HR] characteristic 2A37 not found");

        return false;
    }

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

/* =========================
   踏频
   ========================= */

bool Bluetooth_SubscribeCadence()
{
    if (!s_cadClient || !s_cadClient->isConnected())
    {
        return false;
    }

    NimBLERemoteService* service = s_cadClient->getService(CSC_SERVICE_UUID);

    if (!service)
    {
        portENTER_CRITICAL(&s_mux);

        s_info.cadenceServiceFound = false;
        s_info.cadenceNotifyEnabled = false;

        portEXIT_CRITICAL(&s_mux);

        Serial.println("[BT][CAD] service 1816 not found");

        return false;
    }

    portENTER_CRITICAL(&s_mux);
    s_info.cadenceServiceFound = true;
    portEXIT_CRITICAL(&s_mux);

    NimBLERemoteCharacteristic* chr =
        service->getCharacteristic(CSC_MEASUREMENT_UUID);

    if (!chr)
    {
        portENTER_CRITICAL(&s_mux);
        s_info.cadenceNotifyEnabled = false;
        portEXIT_CRITICAL(&s_mux);

        Serial.println("[BT][CAD] characteristic 2A5B not found");

        return false;
    }

    if (!chr->canNotify() && !chr->canIndicate())
    {
        portENTER_CRITICAL(&s_mux);
        s_info.cadenceNotifyEnabled = false;
        portEXIT_CRITICAL(&s_mux);

        Serial.println("[BT][CAD] characteristic can not notify/indicate");

        return false;
    }

    bool useNotify = chr->canNotify();
    bool ok = chr->subscribe(useNotify, CadenceNotifyCallback);

    portENTER_CRITICAL(&s_mux);

    s_info.cadenceNotifyEnabled = ok;
    s_info.cadenceValid = false;
    s_info.cadenceRpm = 0;
    s_info.cadenceCrankRevCount = 0;
    s_info.cadenceLastEventTime = 0;
    s_info.cadenceLastTick = millis();

    s_cadenceBaseValid = false;
    s_lastCrankRev = 0;
    s_lastCrankEventTime = 0;

    portEXIT_CRITICAL(&s_mux);

    return ok;
}

bool Bluetooth_IsCadenceValid()
{
    bool valid = false;

    portENTER_CRITICAL(&s_mux);
    valid = s_info.cadenceValid;
    portEXIT_CRITICAL(&s_mux);

    return valid;
}

uint16_t Bluetooth_GetCadenceRpm()
{
    uint16_t rpm = 0;

    portENTER_CRITICAL(&s_mux);
    rpm = s_info.cadenceRpm;
    portEXIT_CRITICAL(&s_mux);

    return rpm;
}

/* =========================
   服务发现
   ========================= */

bool Bluetooth_DiscoverServices()
{
    bool ok = false;

    if (s_hrClient && s_hrClient->isConnected())
    {
        ok = Bluetooth_SubscribeHeartRate() || ok;
    }

    if (s_cadClient && s_cadClient->isConnected())
    {
        ok = Bluetooth_SubscribeCadence() || ok;
    }

    return ok;
}

void Bluetooth_ClearServices()
{
    ResetHeartRateState();
    ResetCadenceState();
}

/* =========================
   通用读写接口
   ========================= */

static NimBLEClient* FindClientByService(const char* serviceUUID)
{
    if (!serviceUUID)
    {
        return nullptr;
    }

    if (s_hrClient && s_hrClient->isConnected())
    {
        NimBLERemoteService* service = s_hrClient->getService(serviceUUID);

        if (service)
        {
            return s_hrClient;
        }
    }

    if (s_cadClient && s_cadClient->isConnected())
    {
        NimBLERemoteService* service = s_cadClient->getService(serviceUUID);

        if (service)
        {
            return s_cadClient;
        }
    }

    return nullptr;
}

bool Bluetooth_ReadCharacteristic(
    const char* serviceUUID,
    const char* charUUID,
    std::string& outValue
)
{
    outValue.clear();

    if (!serviceUUID || !charUUID)
    {
        return false;
    }

    NimBLEClient* client = FindClientByService(serviceUUID);

    if (!client || !client->isConnected())
    {
        return false;
    }

    NimBLERemoteService* service = client->getService(serviceUUID);

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
    if (!serviceUUID || !charUUID || !data || len == 0)
    {
        return false;
    }

    NimBLEClient* client = FindClientByService(serviceUUID);

    if (!client || !client->isConnected())
    {
        return false;
    }

    NimBLERemoteService* service = client->getService(serviceUUID);

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
    if (!serviceUUID || !charUUID)
    {
        return false;
    }

    NimBLEClient* client = FindClientByService(serviceUUID);

    if (!client || !client->isConnected())
    {
        return false;
    }

    NimBLERemoteService* service = client->getService(serviceUUID);

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

/* =========================
   信息获取
   ========================= */

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

/* =========================
   循环更新
   ========================= */

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

    if (s_hrClient)
    {
        bool realConnected = s_hrClient->isConnected();

        portENTER_CRITICAL(&s_mux);

        if (!realConnected && s_info.heartRateConnected)
        {
            ClearHeartRateLocked();
        }

        portEXIT_CRITICAL(&s_mux);
    }

    if (s_cadClient)
    {
        bool realConnected = s_cadClient->isConnected();

        portENTER_CRITICAL(&s_mux);

        if (!realConnected && s_info.cadenceConnected)
        {
            ClearCadenceLocked();
        }

        portEXIT_CRITICAL(&s_mux);
    }

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

    if (
        s_info.cadenceValid &&
        s_info.cadenceLastTick > 0 &&
        now - s_info.cadenceLastTick > CADENCE_TIMEOUT_MS
    )
    {
        s_info.cadenceValid = false;
        s_info.cadenceRpm = 0;

        s_cadenceBaseValid = false;
        s_lastCrankRev = 0;
        s_lastCrankEventTime = 0;
    }

    UpdateConnectedStateLocked();

    portEXIT_CRITICAL(&s_mux);
}

} // namespace HAL