#ifndef BLE_CHARACTERISTIC_CALLBACKS_H
#define BLE_CHARACTERISTIC_CALLBACKS_H
#include "SerialLogger.h"
#include <BLECharacteristic.h>

class MyBLECharacteristicCallbacks: public BLECharacteristicCallbacks {

    private:
    void (*callbackRead)(BLECharacteristic*);
    void (*callbackWrite)(BLECharacteristic*);

    public:
    MyBLECharacteristicCallbacks();
    MyBLECharacteristicCallbacks(void (*)(BLECharacteristic*), void (*)(BLECharacteristic*));
    void onRead(BLECharacteristic*);
    void onWrite(BLECharacteristic*);
};

#endif
