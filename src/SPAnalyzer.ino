#include <string>
#include <SPI.h>
#include "AzureIoT.h"
#include "SensorsManager.h"
#include "BLEManager.h"
#include "ESPManager.h"
#include "WiFiManager.h"

// Additional sample headers #include "inc/AzIoTSasToken.h"
#include "AzIoTSasToken.h"
#include "SerialLogger.h"
#include "iot_configs.h"

// button
int lastState = LOW;
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;
const unsigned short int SHORT_PRESS = 1000;
const unsigned short int LONG_PRESS = 10000;

/** new code **/
void createBlinkGreenTask(int blinkDelay);
void restart();
void wifiSetup(std::string, std::string);

BLEManager* bleManager;
ESPManager* espManager;
SensorsManager* sensorsManager;
WiFiManager* wifiManager;

void BLEOnReadCB(BLECharacteristic* pCharacteristic);
void BLEOnWriteCB(BLECharacteristic* pCharacteristic);
MyBLECharacteristicCallbacks* myBLECharacteristicCallbacks = new MyBLECharacteristicCallbacks(BLEOnReadCB, BLEOnWriteCB);
MyBLEServerCallbacks* serverCallbacks = new MyBLEServerCallbacks();

void manageButtonPress() {
  int buttonState = digitalRead(BUTTON);
  if(buttonState == LOW && lastState == HIGH) {
    Serial.println("**************BUTTON PRESSED*****************");
    releasedTime = millis();
    long unsigned int pressedDuration = releasedTime - pressedTime;
    if (pressedDuration >= LONG_PRESS) {
      ESPManager::reset();
      ESPManager::restart();
    } else if (pressedDuration <= SHORT_PRESS) {
      ESPManager::restart();
    }
    Serial.println(releasedTime - pressedTime);
  } else if (buttonState == HIGH && lastState == LOW) {
    pressedTime = millis();
  }
  lastState = buttonState;
  /*if (lastState == HIGH && currentState == LOW) {
    pressedTime = millis();
  } else if (lastState == LOW && currentState == HIGH) {
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime;
    if (pressDuration > RESET_PRESS_TIME) {
      Logger.Info("RESET MODE ENGAGED");
      //espManager->reset();
    } else if (pressDuration > 300 && pressDuration < RESET_PRESS_TIME) {
      Logger.Debug(std::to_string(currentState).c_str());
      Logger.Debug(std::to_string(pressDuration).c_str());
      // espManager->restart();
    }
  }
  lastState = currentState;
  nbPressed = 0;*/
}

/**
 * NEW CODE REFACTO
 */

void BLEOnReadCB(BLECharacteristic* pCharacteristic) {
    Serial.println("onRead triggered");
    int state = ESPManager::getGlobalState();
    pCharacteristic->setValue(state);
 }

void BLEOnWriteCB(BLECharacteristic* pCharacteristic) {
    Logger.Info("received: ");
    Logger.Info(pCharacteristic->getValue().c_str());
    std::string value = pCharacteristic->getValue();
    
    if (value == "reset") {
      ESPManager::reset();
      ESPManager::restart();
    }

    int scIndex = 0;
    std::string ssid = "";
    std::string password = "";

    for(int i = 0; i < value.length(); i++) {
      if(value[i] == ',') {
        scIndex = i;
      }
    }

    for(int i = 0; i < scIndex; i++) {
      ssid += value[i];
    }
    
    for(int i = scIndex + 1; i < value.length(); i++) {
      password += value[i];
    }
    
    wifiManager->config(ssid, password);
    if (wifiManager->connect()) {
      espManager->saveEEPROM(ssid);
      espManager->saveEEPROM(password);
      espManager->restart();
    } else {
      ESPManager::setGlobalState(ANALYZER_ERROR);
    }
 }

void initBLE() {
  bleManager = BLEManager::getInstance();
  bleManager->setServerCallbacks(serverCallbacks);
  bleManager->setCharacteristicCallbacks(myBLECharacteristicCallbacks);
  bleManager->init();
}

void startAzure() {
  AzureIot::init(sensorsManager);
  AzureIot::establishConnection();
}

void setup() {
  std::string SSID, PASSWORD;
  int ssidSize, passwordSize;
  std::string ssid;
  std::string password;

  espManager = new ESPManager();
  ESPManager::setGlobalState(ANALYZER_PAIRING);
  sensorsManager = new SensorsManager();
  wifiManager = WiFiManager::getInstance();
  sensorsManager->startBlink(500);
  sensorsManager->initialiseSensors();
  initBLE();

  if (espManager->isConfig()) {
    espManager->readEEPROM(&ssidSize, 0);
    espManager->readEEPROM(&passwordSize, ssidSize + 1);
    ssid = espManager->readEEPROM(1, ssidSize);
    password = espManager->readEEPROM(ssidSize + 2, passwordSize);
    wifiManager->config(ssid, password);
    if (wifiManager->connect()) {
      espManager->saveEEPROM(ssid);
      espManager->saveEEPROM(password);
      startAzure();
      sensorsManager->stopBlink();
      espManager->isReady = true;
      ESPManager::setGlobalState(ANALYZER_OK);
    } else {
      ESPManager::setGlobalState(ANALYZER_ERROR);
    }
  }
  bleManager->startAdvertising();
}

void loop() {
  if (!espManager->isReady) {
    
  } else if (espManager->isReady && ESPManager::getGlobalState() == ANALYZER_OK) {
    if (AzureIot::tokenExpired()) {
      Logger.Info("SAS token expired; reconnecting with a new one.");
      sensorsManager->startBlink(100);
      AzureIot::destroyMqtt();
      AzureIot::establishConnection();
      sensorsManager->stopBlink();
    } else if (AzureIot::time2send()) {
      Logger.Info("Pushing data to hub...");
      sensorsManager->startBlink(100);
      AzureIot::sendData();
      sensorsManager->stopBlink();
    }
  }
  manageButtonPress();
}
