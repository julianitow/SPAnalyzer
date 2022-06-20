#ifndef SENSORS_MANAGER_H
#define SENSORS_MANAGER_H

#include "../../include/iot_configs.h"
// temperature sensor
#include <OneWire.h>
#include <DallasTemperature.h>

// luminosity sensor
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"

#define GREENLED 23
#define ONEWIRE 33
#define RELAY 26
#define MOISTURE 34

class SensorsManager {
    private:
    OneWire oneWire;
    DallasTemperature dallasTemp;
    Adafruit_TSL2591 tsl;
    TaskHandle_t greenBlinkTask;
    void configureLumSensor();
    static void startGreenBlink(void* pvParameters);

    public:
    SensorsManager();
    void initialiseSensors();
    void sprink(bool);
    float getLux();
    int getMoisture();
    float getTemperature();
    DallasTemperature getDallasSensor();
    Adafruit_TSL2591 getTSL();
    void startBlink(int);
    void stopBlink();
    static void initLed();
};
#endif
