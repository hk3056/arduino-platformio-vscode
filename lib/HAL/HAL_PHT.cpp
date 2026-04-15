#include "HAL.h"
#include "Seeed_BMP280.h"

static BMP280 bmp;
static HAL::CommitFunc_t CommitFunc = nullptr;
static void* UserData = nullptr;

bool HAL::PHT_Init()
{
    Serial.print("PHT(BMP280): init...");

    bool success = bmp.init();

    Serial.println(success ? "success" : "failed");

    return success;
}

void HAL::PHT_SetCommitCallback(CommitFunc_t func, void* userData)
{
    CommitFunc = func;
    UserData = userData;
}

void HAL::PHT_Update()
{
    PHT_Info_t phtInfo = {};

    phtInfo.pressure = bmp.getPressure();
    phtInfo.temperature = bmp.getTemperature();
    phtInfo.humidity = 0;   // BMP280 无湿度功能

   

    if (CommitFunc)
    {
        CommitFunc(&phtInfo, UserData);
    }
}