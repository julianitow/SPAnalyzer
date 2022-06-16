#include "SensorsManager.h"
#include "SerialLogger.h"
TaskHandle_t SensorsManager::greenBlinkTask = nullptr;

SensorsManager::SensorsManager() {
    Logger.Info("Hello from SensorsManager");
    pinMode(GREENLED, OUTPUT);
    pinMode(RELAY, OUTPUT);

    this->oneWire = OneWire(ONEWIRE);
    this->dallasTemp = DallasTemperature(&oneWire);
    this->tsl = Adafruit_TSL2591(2591);
}

void SensorsManager::initialiseSensors() {
    this->dallasTemp.begin();
    if (this->tsl.begin()) {
        Logger.Info("TSL2591 found !");
        this->configureLumSensor();
    } else {
        Logger.Error("TSL2591 not found !");
    }
}

DallasTemperature SensorsManager::getDallasSensor() {
  return this->dallasTemp;
}

Adafruit_TSL2591 SensorsManager::getTSL() {
  return this->tsl;
}

void SensorsManager::sprink(bool status) {
  if (status) {
    Logger.Debug("SPRINK");
    digitalWrite(RELAY, HIGH);
  } else {
    Logger.Debug("NOT SPRINK");
    digitalWrite(RELAY, LOW);
  }
}

float SensorsManager::getLux() {
    uint32_t lum = this->tsl.getFullLuminosity();
    uint16_t ir, full;
    ir = lum >> 16;
    full = lum & 0xFFFF;
    return this->tsl.calculateLux(full, ir);
}

int SensorsManager::getMoisture() {
    const int value = analogRead(MOISTURE);
    return value;
}

float SensorsManager::getTemperature() {
    this->dallasTemp.requestTemperatures(); 
    return this->dallasTemp.getTempCByIndex(0);
}

void SensorsManager::configureLumSensor() {
    // You can change the gain on the fly, to adapt to brighter/dimmer light situations
    this->tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain (bright light)
    //tsl.setGain(TSL2591_GAIN_MED);      // 25x gain
    // tsl.setGain(TSL2591_GAIN_HIGH);   // 428x gain

    // Changing the integration time gives you a longer time over which to sense light
    // longer timelines are slower, but are good in very low light situtations!
    //tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time (bright light)
    // tsl.setTiming(TSL2591_INTEGRATIONTIME_200MS);
    // tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
    // tsl.setTiming(TSL2591_INTEGRATIONTIME_400MS);
    this->tsl.setTiming(TSL2591_INTEGRATIONTIME_500MS);
    // tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time (dim light)

    /* Display the gain and integration time for reference sake */  
    Logger.Info("------------------------------------");
    Logger.Info("Gain:         ");
    tsl2591Gain_t gain = this->tsl.getGain();
    switch(gain)
    {
      case TSL2591_GAIN_LOW:
        Logger.Info("1x (Low)");
        break;
      case TSL2591_GAIN_MED:
        Logger.Info("25x (Medium)");
        break;
      case TSL2591_GAIN_HIGH:
        Logger.Info("428x (High)");
        break;
      case TSL2591_GAIN_MAX:
        Logger.Info("9876x (Max)");
        break;
    }
}
void SensorsManager::startBlink(int blinkDelay) {
  xTaskCreatePinnedToCore(startGreenBlink, "greenBlink", 10000, (void*)&blinkDelay, 0, &greenBlinkTask, 0);
}

void SensorsManager::stopBlink() {
  vTaskDelete(this->greenBlinkTask);
  this->stopBlink();
}

void SensorsManager::startGreenBlink(void* pvParameters) {
  for(;;){
    digitalWrite(GREENLED, HIGH);
    // delay(*((int*)pvParameters));
    delay(500);
    digitalWrite(GREENLED, LOW);
    // delay(*((int*)pvParameters));
    delay(500);
  } 
}

void SensorsManager::stopGreenBlink() {
  digitalWrite(GREENLED, LOW);
}
