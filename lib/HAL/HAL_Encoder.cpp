#include "HAL.h"
#include "ButtonEvent/ButtonEvent.h"
#include <Arduino.h>

static ButtonEvent KeyUp(300);
static ButtonEvent KeyDown(300);
static ButtonEvent KeyOk(300);
static ButtonEvent KeyBacklight(300);

static bool EncoderEnable = true;
static volatile int32_t EncoderDiff = 0;
static volatile bool EncoderPush = false;

static uint8_t BacklightLevelIndex = 3; // 4档亮度：关、低、中、高
static const uint16_t BacklightLevelTable[4] = { 0, 256, 614, 1023 };

static const uint32_t KEY_DEBOUNCE_MS = 25;

portMUX_TYPE kmux = portMUX_INITIALIZER_UNLOCKED;

typedef struct
{
    uint8_t pin;
    bool lastRawPressed;
    bool stablePressed;
    uint32_t lastChangeTick;
} KeyDebounce_t;

static KeyDebounce_t DebounceUp;
static KeyDebounce_t DebounceDown;
static KeyDebounce_t DebounceOk;
static KeyDebounce_t DebounceBacklight;

static void KeyDebounce_Init(KeyDebounce_t* key, uint8_t pin)
{
    key->pin = pin;

    // INPUT_PULLUP: 松开=HIGH, 按下=LOW
    bool pressed = (digitalRead(pin) == LOW);
    key->lastRawPressed = pressed;
    key->stablePressed = pressed;
    key->lastChangeTick = millis();
}

static bool KeyDebounce_Update(KeyDebounce_t* key)
{
    bool rawPressed = (digitalRead(key->pin) == LOW);

    if (rawPressed != key->lastRawPressed)
    {
        key->lastRawPressed = rawPressed;
        key->lastChangeTick = millis();
    }

    if ((millis() - key->lastChangeTick) >= KEY_DEBOUNCE_MS)
    {
        key->stablePressed = key->lastRawPressed;
    }

    return key->stablePressed;
}

/**
 * @brief 上键事件处理：模拟编码器逆时针
 */
static void KeyUp_Handler(ButtonEvent* btn, int event)
{
    (void)btn;

    portENTER_CRITICAL(&kmux);
    if (EncoderEnable && event == ButtonEvent::EVENT_PRESSED)
    {
        EncoderDiff = -1;
        HAL::Buzz_Tone(1800, 5);
    }
    portEXIT_CRITICAL(&kmux);
}

/**
 * @brief 下键事件处理：模拟编码器顺时针
 */
static void KeyDown_Handler(ButtonEvent* btn, int event)
{
    (void)btn;

    portENTER_CRITICAL(&kmux);
    if (EncoderEnable && event == ButtonEvent::EVENT_PRESSED)
    {
        EncoderDiff = 1;
        HAL::Buzz_Tone(2000, 5);
    }
    portEXIT_CRITICAL(&kmux);
}

/**
 * @brief 确认键事件处理：替代原编码器按下
 */
static void KeyOk_Handler(ButtonEvent* btn, int event)
{
    (void)btn;

    portENTER_CRITICAL(&kmux);
    if (!EncoderEnable)
    {
        portEXIT_CRITICAL(&kmux);
        return;
    }

    if (event == ButtonEvent::EVENT_PRESSED)
    {
        EncoderPush = true;
        HAL::Buzz_Tone(2200, 5);
    }
    else if (event == ButtonEvent::EVENT_RELEASED)
    {
        EncoderPush = false;
    }
    portEXIT_CRITICAL(&kmux);
}

/**
 * @brief 背光键事件处理：4档循环切换
 */
static void KeyBacklight_Handler(ButtonEvent* btn, int event)
{
    (void)btn;

    if (event == ButtonEvent::EVENT_PRESSED)
    {
        portENTER_CRITICAL(&kmux);
        BacklightLevelIndex++;
        if (BacklightLevelIndex >= 4)
        {
            BacklightLevelIndex = 0;
        }
        uint16_t level = BacklightLevelTable[BacklightLevelIndex];
        portEXIT_CRITICAL(&kmux);

        HAL::Backlight_SetValue(level);
        HAL::Buzz_Tone(2400, 5);
    }
}

/**
 * @brief 输入初始化
 */
void HAL::Encoder_Init()
{
    pinMode(CONFIG_KEY_UP_PIN, INPUT_PULLUP);
    pinMode(CONFIG_KEY_DOWN_PIN, INPUT_PULLUP);
    pinMode(CONFIG_KEY_OK_PIN, INPUT_PULLUP);
    pinMode(CONFIG_KEY_BACKLIGHT_PIN, INPUT_PULLUP);

    KeyDebounce_Init(&DebounceUp, CONFIG_KEY_UP_PIN);
    KeyDebounce_Init(&DebounceDown, CONFIG_KEY_DOWN_PIN);
    KeyDebounce_Init(&DebounceOk, CONFIG_KEY_OK_PIN);
    KeyDebounce_Init(&DebounceBacklight, CONFIG_KEY_BACKLIGHT_PIN);

    KeyUp.EventAttach(KeyUp_Handler);
    KeyDown.EventAttach(KeyDown_Handler);
    KeyOk.EventAttach(KeyOk_Handler);
    KeyBacklight.EventAttach(KeyBacklight_Handler);

    // 默认高亮
    HAL::Backlight_SetValue(BacklightLevelTable[BacklightLevelIndex]);
}

/**
 * @brief 输入轮询更新
 */
void HAL::Encoder_Update()
{
    KeyUp.EventMonitor(KeyDebounce_Update(&DebounceUp));
    KeyDown.EventMonitor(KeyDebounce_Update(&DebounceDown));
    KeyOk.EventMonitor(KeyDebounce_Update(&DebounceOk));
    KeyBacklight.EventMonitor(KeyDebounce_Update(&DebounceBacklight));
}

/**
 * @brief 获取方向增量
 * 上键返回 -1，下键返回 +1
 */
int32_t HAL::Encoder_GetDiff()
{
    portENTER_CRITICAL(&kmux);
    int32_t diff = EncoderDiff;
    EncoderDiff = 0;
    portEXIT_CRITICAL(&kmux);
    return diff;
}

/**
 * @brief 获取确认键当前是否按下
 */
bool HAL::Encoder_GetIsPush()
{
    if (!EncoderEnable)
    {
        return false;
    }
    return EncoderPush;
}

/**
 * @brief 使能/禁用输入
 */
void HAL::Encoder_SetEnable(bool en)
{
    portENTER_CRITICAL(&kmux);
    EncoderEnable = en;
    if (!en)
    {
        EncoderPush = false;
        EncoderDiff = 0;
    }
    portEXIT_CRITICAL(&kmux);
}