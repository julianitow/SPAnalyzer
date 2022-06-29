#ifndef ESP_MANAGER_H
#define ESP_MANAGER_H

#include "../../include/iot_configs.h"
#include "SerialLogger.h"

#include <EEPROM.h>
#include <string.h>

class ESPManager {

    private:
    static int lastIndex;
    static GlobalState globalState;
    unsigned long buttonPressedTime = 0;
    unsigned long buttonReleasedTime = 0;
    int buttonLastState = LOW;

    public:
    ESPManager();
    bool isConfig();
    static void setGlobalState(GlobalState);
    static GlobalState getGlobalState();
    static void restart();
    static void reset();
    void saveEEPROM(std::string);
    void readEEPROM(int*, int offset);
    std::string readEEPROM(int, int);
    void controlButton();
    bool isReady = false;
};

#endif