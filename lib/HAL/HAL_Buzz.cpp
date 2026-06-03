#include "HAL.h"

#define BUZZ_CHANNEL 1

static bool IsEnable = true;
static bool IsInited = false;

/**
 * @brief 关闭蜂鸣器输出
 */
static void Buzz_StopOutput()
{
    noTone(CONFIG_BUZZ_PIN);
    digitalWrite(CONFIG_BUZZ_PIN, LOW);
}

/**
 * @brief 蜂鸣器初始化
 */
void HAL::Buzz_init()
{
    pinMode(CONFIG_BUZZ_PIN, OUTPUT);

    /*
     * ESP32 Arduino tone() 默认会占用一个 PWM 通道。
     * 项目中背光已经使用通道 0，所以蜂鸣器固定使用通道 1。
     */
    setToneChannel(BUZZ_CHANNEL);

    Buzz_StopOutput();
    IsEnable = true;
    IsInited = true;
}

/**
 * @brief 使能或关闭蜂鸣器
 */
void HAL::Buzz_SetEnable(bool en)
{
    IsEnable = en;

    if (!IsEnable)
    {
        Buzz_StopOutput();
    }
}

/**
 * @brief 蜂鸣器发声
 * @param freq      频率，单位 Hz。freq 为 0 时关闭蜂鸣器
 * @param duration  持续时间，单位 ms。
 *                  duration > 0：响指定时间
 *                  duration <= 0：持续响，直到下一次调用 Buzz_Tone(0)
 */
void HAL::Buzz_Tone(uint32_t freq, int32_t duration)
{
    if (!IsInited)
    {
        Buzz_init();
    }

    if (!IsEnable)
    {
        return;
    }

    if (freq == 0)
    {
        Buzz_StopOutput();
        return;
    }

    if (duration > 0)
    {
        tone(CONFIG_BUZZ_PIN, freq, (uint32_t)duration);
    }
    else
    {
        /*
         * 用于音乐播放。
         * 音符持续响，直到 TonePlayer 播放下一个音符或结束时传入 freq=0。
         */
        tone(CONFIG_BUZZ_PIN, freq, 0);
    }
}