#include "HAL.h"

static constexpr uint8_t BACKLIGHT_PWM_CH = 0;
static constexpr uint32_t BACKLIGHT_PWM_FREQ = 5000;
static constexpr uint8_t BACKLIGHT_PWM_RES = 10;
static constexpr uint16_t BACKLIGHT_MAX = (1 << BACKLIGHT_PWM_RES) - 1;

static uint16_t s_backlightValue = BACKLIGHT_MAX;
static bool s_forceLit = false;
static bool s_backlightInited = false;

void HAL::Backlight_Init()
{
    Serial.println("Backlight_Init enter");

    ledcSetup(BACKLIGHT_PWM_CH, BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_RES);
    ledcAttachPin(CONFIG_SCREEN_BLK_PIN, BACKLIGHT_PWM_CH);

    s_backlightInited = true;

    Backlight_SetValue(BACKLIGHT_MAX);

    Serial.println("Backlight_Init success");
}

uint16_t HAL::Backlight_GetValue()
{
    return s_backlightValue;
}

void HAL::Backlight_SetValue(int16_t val)
{
    if(val < 0)
    {
        val = 0;
    }
    else if(val > BACKLIGHT_MAX)
    {
        val = BACKLIGHT_MAX;
    }

    s_backlightValue = (uint16_t)val;

    if(!s_backlightInited)
    {
        Serial.println("Backlight_SetValue skipped: PWM not inited");
        return;
    }

    if(s_forceLit)
    {
        ledcWrite(BACKLIGHT_PWM_CH, BACKLIGHT_MAX);
    }
    else
    {
        ledcWrite(BACKLIGHT_PWM_CH, s_backlightValue);
    }
}

void HAL::Backlight_SetGradual(uint16_t target, uint16_t time)
{
    (void)time;
    Backlight_SetValue(target);
}

void HAL::Backlight_ForceLit(bool en)
{
    s_forceLit = en;

    if(!s_backlightInited)
    {
        Serial.println("Backlight_ForceLit skipped: PWM not inited");
        return;
    }

    if(s_forceLit)
    {
        ledcWrite(BACKLIGHT_PWM_CH, BACKLIGHT_MAX);
    }
    else
    {
        ledcWrite(BACKLIGHT_PWM_CH, s_backlightValue);
    }
}