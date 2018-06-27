#pragma once

#include "TimeObj.h"

struct SensorValue{
    static const int TEMPERATURE = 1;
    static const int HUMIDITY = 2;
    static const int PRESSURE = 4;
    static const int LUMINOSItY = 8;

    int enableFlag;
    Time captureTime;

    int32_t temperature;
    int32_t humidity;
    int32_t pressure;
    uint32_t luminosity;

    float temperatureFloat(){return (float)temperature / 100.;};
    float humidityFloat(){return (float)humidity / 1024.;};
    float pressureFloat(){return (float)pressure / 25600.;};
};
    
bool startSensorService();
void enableSensorCapturing();
void getSensorValue(SensorValue* value);
void getSensorValueAsJson(std::ostream& out);
