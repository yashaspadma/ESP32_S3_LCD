#include <Arduino.h>
#include <Wire.h>

#include "display_setup.h"
#include "app_config.h"
#include "SensorQMI8658.hpp"

SensorQMI8658 imu;

void setup()
{
    Serial.begin(115200);

    // ST7701
    if (!initDisplay())
    {
        while (true)
            delay(1000);
    }

    // QMI8658
    if (!imu.begin(
            Wire,
            SHAKE_NEXT_VIDEO_QMI8658_ADDRESS,
            SHAKE_NEXT_VIDEO_QMI8658_SDA,
            SHAKE_NEXT_VIDEO_QMI8658_SCL))
    {
        // IMU failed
        while (true)
            delay(1000);
    }

    imu.configAccelerometer(
        SensorQMI8658::ACC_RANGE_8G,
        SensorQMI8658::ACC_ODR_125Hz,
        SensorQMI8658::LPF_MODE_3);

    imu.enableAccelerometer();

    Serial.println("DISPLAY + IMU READY");
}

void loop()
{
    if (imu.getDataReady())
    {
        float x, y, z;

        imu.getAccelerometer(x, y, z);

        Serial.print("X: ");
        Serial.print(x);
        Serial.print(" Y: ");
        Serial.print(y);
        Serial.print(" Z: ");
        Serial.println(z);
    }
}