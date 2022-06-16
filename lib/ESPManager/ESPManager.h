#ifndef ESP_MANAGER_H
#define ESP_MANAGER_H

#include "../../include/iot_configs.h"
#include "SerialLogger.h"

#include <EEPROM.h>
#include <string.h>

class ESPManager {

    private:
    int lastIndex = 0;
    
    public:
    ESPManager();
    bool isConfig();
    void restart();
    void reset();
    void saveEEPROM(std::string);
    void readEEPROM(int*, int offset);
    std::string readEEPROM(int, int);
};

#endif