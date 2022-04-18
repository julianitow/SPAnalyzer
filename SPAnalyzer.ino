#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <string>

#include <EEPROM.h>

#define bleServerName "SOULPOT_ESP32_00"
#define SERVICE_UUID "80b7f088-0084-43e1-a687-8457bcb2dbc8"

BLECharacteristic batteryLevelChar("8818604d-2616-4977-8d12-57c291e619f9", BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor batteryLevelCharDesc(BLEUUID((uint16_t)0x180F));

BLECharacteristic wifiParamChar("96c44fd5-c309-4553-a11e-b8457810b94c", BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
BLEDescriptor wifiParamCharDesc("544bd464-8388-42aa-99cd-81cf6e6042d7");

bool deviceConnected = false;

//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("New client connected");
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Client disconnected");
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic* pCharacteristic) {
    Serial.println("onRead triggered");
  }
  void onWrite(BLECharacteristic* pCharacteristic) {
    Serial.print("received: ");
    Serial.println(pCharacteristic->getValue().c_str());
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

    Serial.print("Parsed: ");
    Serial.print(ssid.c_str());
    Serial.print(", ");
    Serial.println(password.c_str());
  }
};

void bluetoothSetup() {
  Serial.println("Configuration mode detected");
  BLEDevice::init(bleServerName);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *bmeService = pServer->createService(SERVICE_UUID);
  bmeService->addCharacteristic(&wifiParamChar);
  wifiParamCharDesc.setValue("SP Wifi Config");
  wifiParamChar.addDescriptor(&wifiParamCharDesc);
  wifiParamChar.setCallbacks(new MyCharacteristicCallbacks());
  // wifiParamCharDesc.addDescriptor(new BLE2902());
  bmeService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void setup() {
  Serial.begin(115200);
  bluetoothSetup();
}

void loop() {
  
}
