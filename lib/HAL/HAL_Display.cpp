#include "HAL.h"
#include "TFT_eSPI.h"

static TFT_eSPI screen = TFT_eSPI(CONFIG_SCREEN_HOR_RES, CONFIG_SCREEN_VER_RES);
static HAL::Display_CallbackFunc_t Disp_Callback = nullptr;

void HAL::Display_Init()
{
    Serial.println("Display_Init enter");

    screen.init();
    Serial.println("screen.init done");

    screen.setRotation(0);
    screen.invertDisplay(true);

    screen.fillScreen(TFT_BLACK);

    screen.setTextWrap(true);
    screen.setTextSize(1);
    screen.setCursor(0, 0);
    screen.setTextFont(0);
    screen.setTextColor(TFT_WHITE, TFT_BLUE);

    HAL::Backlight_SetGradual(1000, 1000);
    Serial.println("Display_Init success");
}

void HAL::Display_DumpCrashInfo(const char* info)
{
    HAL::Backlight_ForceLit(true);
    Serial.println("Display Crashed.");

    screen.fillScreen(TFT_BLUE);
    screen.setTextColor(TFT_WHITE);
    screen.setTextFont(0);
    screen.setTextSize(1);
    screen.setCursor(0, 0);
    screen.println(info);
    screen.print("Press KEY to reboot..");
}

void HAL::Display_SetAddrWindow(int16_t x, int16_t y, int16_t w, int16_t h)
{
    screen.setAddrWindow(x, y, w, h);
}

void HAL::Display_SendPixels(const uint16_t* pixels, uint32_t len)
{
    screen.startWrite();
    screen.pushPixels((uint16_t*)pixels, len);
    screen.endWrite();

    if (Disp_Callback) {
        Disp_Callback();
    }
}

void HAL::Display_PushRect(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels)
{
    screen.startWrite();
    screen.setAddrWindow(x, y, w, h);
    screen.pushPixels((uint16_t*)pixels, (uint32_t)w * h);
    screen.endWrite();

    if (Disp_Callback) {
        Disp_Callback();
    }
}

void HAL::Display_SetSendFinishCallback(Display_CallbackFunc_t func)
{
    Disp_Callback = func;
}