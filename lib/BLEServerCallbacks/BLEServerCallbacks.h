#ifndef BLE_SERVER_CALLBACKS_H
#define BLE_SERVER_CALLBACKS_H
#include "SerialLogger.h"

#include <BLEServer.h>
#include <WiFi.h>

class MyBLEServerCallbacks: public BLEServerCallbacks {
  private:

  public:
  void onConnect(BLEServer* pServer);
  void onDisconnect(BLEServer* pServer);
};

#endif
