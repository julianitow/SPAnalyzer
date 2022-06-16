#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "../../include/iot_configs.h"
#include "SerialLogger.h"
#include <WiFi.h>

class WiFiManager {

    private:
    std::string ssid;
    std::string password;
    int timeout;
    static WiFiManager* instance;
    WiFiManager();

    public:
    static WiFiManager* getInstance();
    void config(std::string, std::string, int = 10000);
    bool connect();
};

#endif