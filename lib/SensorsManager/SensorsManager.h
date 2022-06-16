#ifndef SENSORS_MANAGER_H
#define SENSORS_MANAGER_H
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
    
    void configureLumSensor();
    
    public:
    SensorsManager();
    void initialiseSensors();
    void sprink(bool);
    float getLux();
    int getMoisture();
    float getTemperature();
    DallasTemperature getDallasSensor();
    Adafruit_TSL2591 getTSL();
    static void greenBlink(void* pvParameters);
    static void initLed();
    void stopBlink();
};
#endif
