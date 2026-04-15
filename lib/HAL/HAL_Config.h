/*
* MIT License
 * Copyright (c) 2021 _VIFEXTech
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef __HAL_CONFIG_H
#define __HAL_CONFIG_H

/*=========================
   Hardware Configuration
 *=========================*/

/* Sensors */
#define CONFIG_SENSOR_ENABLE        1

#if CONFIG_SENSOR_ENABLE
#  define CONFIG_SENSOR_IMU_ENABLE  1
#  define CONFIG_SENSOR_MAG_ENABLE  1
#  define CONFIG_SENSOR_PHT_ENABLE  1
#  define CONFIG_SENSOR_GY91_ENABLE 0
#endif

//#define NULL_PIN                    PD0


#define CONFIG_SCREEN_BLK_PIN       21
#define CONFIG_SCREEN_HOR_RES       240
#define CONFIG_SCREEN_VER_RES       400




/* Buzzer */
#define CONFIG_BUZZ_PIN             38

/* GPS */
#define CONFIG_GPS_SERIAL           Serial2
#define CONFIG_GPS_USE_TRANSPARENT  0
#define CONFIG_GPS_BUF_OVERLOAD_CHK 0
#define CONFIG_GPS_TX_PIN           17
#define CONFIG_GPS_RX_PIN           18



#define CONFIG_I2C_SDA_PIN          8
#define CONFIG_I2C_SCL_PIN          16

/* Encoder */
/* Keys */
#define CONFIG_KEY_UP_PIN           9
#define CONFIG_KEY_DOWN_PIN         15
#define CONFIG_KEY_OK_PIN           7
#define CONFIG_KEY_BACKLIGHT_PIN    6
/* Power */
#define CONFIG_POWER_EN_PIN         -1
#define CONFIG_POWER_WAIT_TIME      0
#define CONFIG_POWER_SHUTDOWM_DELAY 5000
#define CONFIG_POWER_BATT_CHG_DET_PULLUP    true



/* SD CARD */
#define CONFIG_SD_SPI               HSPI
#define CONFIG_SD_MOSI_PIN   11
#define CONFIG_SD_MISO_PIN   13
#define CONFIG_SD_SCK_PIN    12
#define CONFIG_SD_CS_PIN     10

/* HAL Interrupt Update Timer */
//#define CONFIG_HAL_UPDATE_TIM       TIM4

/* Show Stack & Heap Info */
#define CONFIG_SHOW_STACK_INFO      0
#define CONFIG_SHOW_HEAP_INFO       0

/* Use Watch Dog */
#define CONFIG_WATCH_DOG_ENABLE     0
#if CONFIG_WATCH_DOG_ENABLE
#  define CONFIG_WATCH_DOG_TIMEOUT (10 * 1000) // [ms]
#endif

#endif