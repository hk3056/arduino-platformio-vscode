#include "HAL.h"
#include "Config/Config.h"
#include "SPI.h"
#include "SdFat.h"

// SdFat 推荐用 SD_SCK_MHZ() 配置 SPI 时钟
#define SD_SPI_CLOCK   SD_SCK_MHZ(20)

static bool SD_IsReady = false;
static uint64_t SD_CardSize = 0;

static HAL::SD_CallbackFunction_t SD_EventCallback = nullptr;

/* 必须做成全局/静态，不能放在 SD_Init() 里当局部变量 */
static SPIClass sdSPI(CONFIG_SD_SPI);

/* SdFat v2，推荐用 SdFs，兼容 FAT/exFAT */
static SdFs sd;

/*
 * User provided date time callback function.
 * See FsDateTime::setCallback() for usage.
 */
static void SD_GetDateTime(uint16_t* date, uint16_t* time)
{
    HAL::Clock_Info_t clock;
    HAL::Clock_GetInfo(&clock);

    // FAT 时间戳从 1980 年开始有效
    *date = FAT_DATE(clock.year, clock.month, clock.day);
    *time = FAT_TIME(clock.hour, clock.minute, clock.second);
}

static bool SD_CheckDir(const char* path)
{
    bool retval = true;

    if (!sd.exists(path))
    {
        Serial.printf("SD: Auto create path \"%s\"...", path);
        retval = sd.mkdir(path);
        Serial.println(retval ? "success" : "failed");
    }

    return retval;
}
static bool SD_TestOpenFile(const char* path)
{
    if (!SD_IsReady)
    {
        Serial.println("SD test: card not ready");
        return false;
    }

    Serial.printf("SD test open: %s\r\n", path);
    Serial.printf("exists: %d\r\n", sd.exists(path) ? 1 : 0);

    FsFile file;
    if (!file.open(path, O_RDONLY))
    {
        Serial.println("open failed");
        return false;
    }

    Serial.printf("open ok, size=%lu\r\n", (unsigned long)file.fileSize());
    file.close();
    return true;
}
bool HAL::SD_Init()
{
    bool retval = true;

    Serial.print("SD: init...");

    sdSPI.begin(CONFIG_SD_SCK_PIN,
                CONFIG_SD_MISO_PIN,
                CONFIG_SD_MOSI_PIN,
                CONFIG_SD_CS_PIN);

    retval = sd.begin(SdSpiConfig(CONFIG_SD_CS_PIN, SHARED_SPI, SD_SPI_CLOCK, &sdSPI));

    SD_IsReady = retval;

    if (retval)
    {
        SD_CardSize = (uint64_t)sd.card()->sectorCount() * 512ULL;
        FsDateTime::setCallback(SD_GetDateTime);
        SD_CheckDir(CONFIG_TRACK_RECORD_FILE_DIR_NAME);

        Serial.printf(
            "success, Type: %s, Size: %.2f GB\r\n",
            SD_GetTypeName(),
            SD_GetCardSizeMB() / 1024.0f
        );

        SD_TestOpenFile("/MAP/16/52763/24635/tile.bin");   // 换成你确定存在的瓦片
    }
    else
    {
        Serial.printf("failed.\r\n");
    }

    return retval;
}
bool HAL::SD_GetReady()
{
    return SD_IsReady;
}

float HAL::SD_GetCardSizeMB()
{
    return (float)SD_CardSize / (1024.0f * 1024.0f);
}

const char* HAL::SD_GetTypeName()
{
    const char* type = "Unknown";

    if (!SD_CardSize)
    {
        return type;
    }

    switch (sd.card()->type())
    {
    case SD_CARD_TYPE_SD1:
    case SD_CARD_TYPE_SD2:
        type = "SDSC";
        break;

    case SD_CARD_TYPE_SDHC:
        // 32GB 及以下通常归为 SDHC，再大一般视作 SDXC
        type = (SD_CardSize <= (32ULL * 1024ULL * 1024ULL * 1024ULL)) ? "SDHC" : "SDXC";
        break;

    default:
        break;
    }

    return type;
}
static void SD_Check(bool isInsert)
{
    if (isInsert)
    {
        bool ret = HAL::SD_Init();

        if (ret && SD_EventCallback)
        {
            SD_EventCallback(true);
        }

        HAL::Audio_PlayMusic(ret ? "DeviceInsert" : "Error");
    }
    else
    {
        SD_IsReady = false;
        SD_CardSize = 0;

        if (SD_EventCallback)
        {
            SD_EventCallback(false);
        }

        HAL::Audio_PlayMusic("DevicePullout");
    }
}

void HAL::SD_SetEventCallback(SD_CallbackFunction_t callback)
{
    SD_EventCallback = callback;
}

void HAL::SD_Update()
{
    /// bool isInsert = (digitalRead(CONFIG_SD_CD_PIN) == LOW);
    /// CM_VALUE_MONITOR(isInsert, SD_Check(isInsert));
}
