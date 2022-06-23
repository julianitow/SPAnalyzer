#include "BLEManager.h"
BLEManager* BLEManager::instance = nullptr;

BLEManager::BLEManager() {
    Logger.Info("Hello from BLE Manager");
}

void BLEManager::init() {
    if (this->characteristicCallbacks == nullptr) {
      Logger.Error("No BLE char callbacks set");
      ESPManager::setGlobalState(ANALYZER_ERROR);
      return;
    }
    if (this->serverCallbacks == nullptr) {
      Logger.Error("No BLE server callbacks set");
      ESPManager::setGlobalState(ANALYZER_ERROR);
      return;
    }
    BLEDevice::init(IOT_CONFIG_DEVICE_ID);
    wifiParamChar = new BLECharacteristic(CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    wifiParamCharDesc = new BLEDescriptor(DESC_UUID);
    this->pServer = BLEDevice::createServer();
    this->pServer->setCallbacks(serverCallbacks);
    this->bmeService = pServer->createService(SERVICE_UUID);
    this->bmeService->addCharacteristic(wifiParamChar);
    wifiParamCharDesc->setValue("SP Wifi Config");
    wifiParamChar->addDescriptor(wifiParamCharDesc);
    wifiParamChar->setCallbacks(this->characteristicCallbacks);
    this->bmeService->start();
    this->pAdvertising = BLEDevice::getAdvertising();
    this->pAdvertising->addServiceUUID(SERVICE_UUID);
}

BLEManager* BLEManager::getInstance() {
    if (BLEManager::instance == nullptr) {
      BLEManager::instance = new BLEManager();
    }
    return BLEManager::instance;
}

void BLEManager::setServerCallbacks(MyBLEServerCallbacks* serverCallbacks) {
  this->serverCallbacks = serverCallbacks;
}

void BLEManager::setCharacteristicCallbacks(MyBLECharacteristicCallbacks* characteristicCallbacks) {
  this->characteristicCallbacks = characteristicCallbacks;
}

void BLEManager::startAdvertising() {
  this->pServer->getAdvertising()->start();
  Logger.Info("Waiting for a client to connect");
}

void BLEManager::destroy() {
  // TODO
  /*if (pServer != nullptr) {
    Logger.Warning("Destroying previous BLE Server");
    pServer->getAdvertising()->stop();
    BLEDevice::deinit(true);
    delete pServer;
    delete bmeService;
    delete pAdvertising;
  }*/
}
