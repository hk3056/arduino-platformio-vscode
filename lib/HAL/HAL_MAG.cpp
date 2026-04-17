#include "HAL.h"
#include "Config/Config.h"
#include "Wire.h"
#include <math.h>

static HAL::CommitFunc_t CommitFunc = nullptr;
static void* UserData = nullptr;

namespace
{
    constexpr uint8_t MAG_ADDR       = 0x2C;

    constexpr uint8_t REG_CHIP_ID    = 0x00;
    constexpr uint8_t REG_X_L        = 0x01;
    constexpr uint8_t REG_STATUS     = 0x09;
    constexpr uint8_t REG_CTRL1      = 0x0A;
    constexpr uint8_t REG_CTRL2      = 0x0B;

    /* 安装偏移角，后面如果发现方向整体偏 90/180 度，就改这里 */
    constexpr float MAG_HEADING_OFFSET_DEG = 0.0f;

    static bool s_ready = false;
    static bool s_hasHeading = false;

    static int16_t s_x = 0;
    static int16_t s_y = 0;
    static int16_t s_z = 0;
    static float   s_headingDeg = 0.0f;

    static bool i2cWriteReg(uint8_t reg, uint8_t value)
    {
        Wire.beginTransmission(MAG_ADDR);
        Wire.write(reg);
        Wire.write(value);
        return (Wire.endTransmission() == 0);
    }

    static bool i2cReadRegs(uint8_t reg, uint8_t* buf, uint8_t len)
    {
        Wire.beginTransmission(MAG_ADDR);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0)
        {
            return false;
        }

        uint8_t n = Wire.requestFrom((int)MAG_ADDR, (int)len);
        if (n != len)
        {
            return false;
        }

        for (uint8_t i = 0; i < len; i++)
        {
            buf[i] = Wire.read();
        }
        return true;
    }

    static bool i2cReadReg(uint8_t reg, uint8_t& value)
    {
        return i2cReadRegs(reg, &value, 1);
    }

    static float norm360(float deg)
    {
        while (deg < 0.0f) deg += 360.0f;
        while (deg >= 360.0f) deg -= 360.0f;
        return deg;
    }

    static float calcHeadingDeg(int16_t x, int16_t y)
    {
        float deg = atan2f((float)y, (float)x) * 180.0f / 3.1415926f;
        return norm360(deg);
    }

    static float smoothHeading(float lastDeg, float nowDeg, float alpha)
    {
        float diff = nowDeg - lastDeg;
        while (diff > 180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        return norm360(lastDeg + diff * alpha);
    }
}

bool HAL::MAG_Init()
{
    Wire.begin(CONFIG_I2C_SDA_PIN, CONFIG_I2C_SCL_PIN);
    delay(10);

    s_ready = false;
    s_hasHeading = false;
    s_headingDeg = 0.0f;

    Wire.beginTransmission(MAG_ADDR);
    if (Wire.endTransmission() != 0)
    {
        Serial.println("MAG: not found @0x2C");
        return false;
    }

    uint8_t chipId = 0x00;
    if (i2cReadReg(REG_CHIP_ID, chipId))
    {
        Serial.printf("MAG: CHIPID=0x%02X\r\n", chipId);
    }

    // CTRL1 (0x0A):
    // OSR2=00(1), OSR1=00(8), ODR=01(50Hz), MODE=11(continuous)
    if (!i2cWriteReg(REG_CTRL1, 0x07))
    {
        Serial.println("MAG: write CTRL1 failed");
        return false;
    }

    // CTRL2 (0x0B):
    // SOFT_RST=0, SELF_TEST=0, RNG=11(2G), SET/RESET MODE=00(set/reset on)
    if (!i2cWriteReg(REG_CTRL2, 0x0C))
    {
        Serial.println("MAG: write CTRL2 failed");
        return false;
    }

    delay(20);

    s_ready = true;
    Serial.println("MAG: init success");
    return true;
}

void HAL::MAG_SetCommitCallback(CommitFunc_t func, void* userData)
{
    CommitFunc = func;
    UserData = userData;
}

void HAL::MAG_Update()
{
    if (!s_ready)
    {
        return;
    }

    uint8_t status = 0;
    if (!i2cReadReg(REG_STATUS, status))
    {
        return;
    }

    // DRDY bit
    if ((status & 0x01) == 0)
    {
        return;
    }

    uint8_t buf[6];
    if (!i2cReadRegs(REG_X_L, buf, sizeof(buf)))
    {
        return;
    }

    s_x = (int16_t)((buf[1] << 8) | buf[0]);
    s_y = (int16_t)((buf[3] << 8) | buf[2]);
    s_z = (int16_t)((buf[5] << 8) | buf[4]);

    float headingNow = calcHeadingDeg(s_x, s_y);

    if (!s_hasHeading)
    {
        s_headingDeg = headingNow;
        s_hasHeading = true;
    }
    else
    {
        s_headingDeg = smoothHeading(s_headingDeg, headingNow, 0.20f);
    }

    s_headingDeg = norm360(s_headingDeg + MAG_HEADING_OFFSET_DEG);

    HAL::MAG_Info_t magInfo = {};
    magInfo.dir = s_headingDeg;
    magInfo.x = s_x;
    magInfo.y = s_y;
    magInfo.z = s_z;

    if (CommitFunc)
    {
        CommitFunc(&magInfo, UserData);
    }
}

bool HAL::MAG_IsReady()
{
    return s_ready && s_hasHeading;
}

bool HAL::MAG_GetHeading(float* headingDeg)
{
    if (headingDeg == nullptr)
    {
        return false;
    }

    if (!s_ready || !s_hasHeading)
    {
        return false;
    }

    *headingDeg = s_headingDeg;
    return true;
}