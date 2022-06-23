#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "../../include/iot_configs.h"
#include "ESPManager.h"
#include "BLEServerCallbacks.h"
#include "BLECharacteristicCallbacks.h"

class BLEManager {
    
    private:
    BLEManager();
    static BLEManager* instance;
    BLEServer *pServer;
    BLEService *bmeService;
    BLEAdvertising *pAdvertising;
    MyBLEServerCallbacks* serverCallbacks;
    MyBLECharacteristicCallbacks* characteristicCallbacks;
    BLECharacteristic* wifiParamChar;
    BLEDescriptor* wifiParamCharDesc;

    public:
    static BLEManager* getInstance();
    void init();
    void setCharacteristicCallbacks(MyBLECharacteristicCallbacks*);
    void setServerCallbacks(MyBLEServerCallbacks*);
    void startAdvertising();
    void destroy();
};

#endif
