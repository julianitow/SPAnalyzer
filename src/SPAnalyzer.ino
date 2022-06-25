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
const int buttonBus = 35;
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;
int lastState = LOW;
int currentState;
int nbPressed = 0;
const int SHORT_PRESS_TIME = 500;
const int RESET_PRESS_TIME = 10000;

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
  currentState = digitalRead(buttonBus);
  if (lastState == HIGH && currentState == LOW) {
    pressedTime = millis();
    nbPressed++;
  } else if (lastState == LOW && currentState == HIGH) {
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime;
    nbPressed++;
    if (pressDuration > RESET_PRESS_TIME) {
      espManager->reset();
    } else if (pressDuration > 300 && pressDuration < RESET_PRESS_TIME) {
      Logger.Debug(std::to_string(currentState).c_str());
      Logger.Debug(std::to_string(pressDuration).c_str());
      // espManager->restart();
    }
  }
  lastState = currentState;
  nbPressed = 0;
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
  /*pinMode(buttonBus, INPUT_PULLUP);
  createBlinkGreenTask(500);

  // TMP
  //_ssid = "uifeedu75";
  //_password = "mandalorianBGdu75";
  // END TMP
  if (_ssid.length() > 0 && _password.length() > 0) {
    Logger.Info("Already config, with:");
    Logger.Info(_ssid.c_str());
    Logger.Info(_password.c_str());
    wifiSetup(_ssid, _password);
    if (WiFi.status() != WL_CONNECTED) {
      Logger.Error("WIFI ERROR, switching to bluetooth mode");
      bluetoothSetup();
    } else {
        //connexion à azure
        Logger.Info("Azure connection...");
        establishConnection();
        sensors.initialiseSensors();
        stopGreenBlink();
    }
  } else {
    Logger.Warning("Configuration mode detected");
    Logger.Info("Waiting for a client to notify...");
    bluetoothSetup();
  }*/
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
