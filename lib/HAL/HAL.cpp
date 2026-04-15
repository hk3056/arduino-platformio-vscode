#include "HAL.h"
#include "Version.h"
#include "Wire.h"
#include "MillisTaskManager/MillisTaskManager.h"

static MillisTaskManager taskManager;

#if CONFIG_SENSOR_ENABLE

static void HAL_Sensor_Init()
{
    Wire.begin(CONFIG_I2C_SDA_PIN, CONFIG_I2C_SCL_PIN);
    Wire.setClock(100000);
    delay(50);

    if(HAL::I2C_Scan() <= 0)
    {
        Serial.println("I2C: disable sensors");
        return;
    }

#if CONFIG_SENSOR_GY91_ENABLE
    if(HAL::GY91_Init())
    {
        taskManager.Register(HAL::GY91_Update, 1000);
    }
#endif

#if CONFIG_SENSOR_IMU_ENABLE
    if(HAL::IMU_Init())
    {
        taskManager.Register(HAL::IMU_Update, 1000);
    }
#endif

#if CONFIG_SENSOR_MAG_ENABLE
    if(HAL::MAG_Init())
    {
        taskManager.Register(HAL::MAG_Update, 1000);
    }
#endif

#if CONFIG_SENSOR_PHT_ENABLE
    if(HAL::PHT_Init())
    {
        taskManager.Register(HAL::PHT_Update, 1000);
    }
#endif
}

#endif

static void HAL_TimerInterrputUpdate()
{
    //HAL::Power_Update();
    HAL::Encoder_Update();
    HAL::Audio_Update();
}

void HAL::HAL_Init()
{
    Serial.begin(115200);
    Serial.println(VERSION_FIRMWARE_NAME);
    Serial.println("Version: " VERSION_SOFTWARE);
    Serial.println("Author: "  VERSION_AUTHOR_NAME);
    Serial.println("Project: " VERSION_PROJECT_LINK);

    //Memory_DumpInfo();

    Backlight_Init();
    Encoder_Init();
    Clock_Init();
    Buzz_init();
    GPS_Init();

#if CONFIG_SENSOR_ENABLE
    HAL_Sensor_Init();
#endif

    Audio_Init();
    SD_Init();
    Display_Init();

    taskManager.Register(Power_EventMonitor, 100);
    taskManager.Register(GPS_Update, 200);
    taskManager.Register(SD_Update, 500);
    taskManager.Register(Memory_DumpInfo, 1000);
}

void HAL::HAL_Update()
{
    HAL::Encoder_Update();
    HAL::Audio_Update();
    taskManager.Running(millis());
}