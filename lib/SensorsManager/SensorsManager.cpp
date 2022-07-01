#include "SensorsManager.h"
#include "SerialLogger.h"

SensorsManager::SensorsManager() {
    Logger.Info("Hello from SensorsManager");
    pinMode(GREENLED, OUTPUT);
    pinMode(RELAY, OUTPUT);
    pinMode(BUTTON, INPUT_PULLUP);
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

void SensorsManager::sprink(bool status, int val) {
  if (status) {
    Logger.Debug("SPRINK");
    digitalWrite(RELAY, HIGH);
    while (this->getMoisture() < val) {
      Logger.Info("Sprinkling...");
      delay(SPRINK_DELAY);
      digitalWrite(RELAY, LOW);
      delay(SPRINK_DELAY);
      digitalWrite(RELAY, HIGH);
    }
    digitalWrite(RELAY, LOW);
  } else {
    Logger.Debug("STOP SPRINK");
    digitalWrite(RELAY, LOW);
  }
}

float SensorsManager::getLux() {
    uint32_t lum = this->tsl.getFullLuminosity();
    uint16_t ir, full;
    ir = lum >> 16;
    full = lum & 0xFFFF;
    const float lux = this->tsl.calculateLux(full, ir);
    return isnan(lux) ? -255 : lux;
}

int SensorsManager::getMoisture() {
    const unsigned short int value = analogRead(MOISTURE);
    if (value == 0) {
      return -255;
    }
    unsigned short int percentage =
      100 - (((value - MIN_MOISTURE_VAL) * 100) / (MAX_MOISTURE_VAL - MIN_MOISTURE_VAL));
    /**
     * @brief due to no fixed values returned by the sensor
     * the percentage could be above 100 by 1 or 2
     *
    if (percentage > 100) {
      percentage = 100;
    }*/
    return percentage;
}

float SensorsManager::getTemperature() {
    this->dallasTemp.requestTemperatures(); 
    return this->dallasTemp.getTempCByIndex(0);
}

void SensorsManager::configureLumSensor() {
    // You can change the gain on the fly, to adapt to brighter/dimmer light situations
    // this->tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain (bright light)
    tsl.setGain(TSL2591_GAIN_MED);      // 25x gain
    // tsl.setGain(TSL2591_GAIN_HIGH);   // 428x gain

    // Changing the integration time gives you a longer time over which to sense light
    // longer timelines are slower, but are good in very low light situtations!
    //tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time (bright light)
    // tsl.setTiming(TSL2591_INTEGRATIONTIME_200MS);
    // tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
    // tsl.setTiming(TSL2591_INTEGRATIONTIME_400MS);
    // this->tsl.setTiming(TSL2591_INTEGRATIONTIME_500MS);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time (dim light)

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
  xTaskCreatePinnedToCore(startGreenBlink, "greenBlink", 1024, (void*)&blinkDelay, 0, &greenBlinkTask, 0);
}

void SensorsManager::stopBlink() {
  if (this->greenBlinkTask != nullptr) {
    vTaskDelete(this->greenBlinkTask);
  }
  digitalWrite(GREENLED, HIGH);
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
