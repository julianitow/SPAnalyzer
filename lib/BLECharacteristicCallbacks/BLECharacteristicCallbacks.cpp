#include "BLECharacteristicCallbacks.h"

MyBLECharacteristicCallbacks::MyBLECharacteristicCallbacks(void (*callbackRead)(BLECharacteristic*), void (*callbackWrite)(BLECharacteristic*)) {
    this->callbackRead = callbackRead;
    this->callbackWrite = callbackWrite;
}

MyBLECharacteristicCallbacks::MyBLECharacteristicCallbacks() {

}

void MyBLECharacteristicCallbacks::onRead(BLECharacteristic* pCharacteristic) {
  this->callbackRead(pCharacteristic);
}

void MyBLECharacteristicCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
  this->callbackWrite(pCharacteristic);
}
