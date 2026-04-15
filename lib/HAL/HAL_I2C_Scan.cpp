#include "HAL.h"
#include "Wire.h"
#include "Config/Config.h"

int HAL::I2C_Scan()
{
    Wire.begin(CONFIG_I2C_SDA_PIN, CONFIG_I2C_SCL_PIN);

    uint8_t error, address;
    int nDevices = 0;

    Serial.println("I2C: device scanning...");

    for (address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0)
        {
            Serial.print("I2C: device found at address 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            Serial.println(" !");
            nDevices++;
        }
        else if (error == 4)
        {
            Serial.print("I2C: unknown error at address 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
        }
    }

    Serial.printf("I2C: %d devices was found\r\n", nDevices);
    return nDevices;
}