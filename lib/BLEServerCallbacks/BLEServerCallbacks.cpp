#include "BLEServerCallbacks.h"

void MyBLEServerCallbacks::onConnect(BLEServer* pServer) {
  Logger.Info("New client connected");
};

void MyBLEServerCallbacks::onDisconnect(BLEServer* pServer) {
  Logger.Warning("Client disconnected");
  if (WiFi.status() != WL_CONNECTED) {
    Logger.Warning("Restart bluetooth");
    pServer->getAdvertising()->start();
  }
}
