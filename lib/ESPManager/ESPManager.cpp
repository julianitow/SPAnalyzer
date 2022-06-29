#include "ESPManager.h"
int ESPManager::lastIndex = 0;
GlobalState ESPManager::globalState = ANALYZER_OK;

ESPManager::ESPManager() {
    Logger.Warning("********************SPA Firmware Starting...********************");
    Logger.Info("Hello from ESP Manager");
    this->lastIndex = 0;
    this->globalState = ANALYZER_PAIRING;
    Serial.begin(115200);
    EEPROM.begin(200);
}

bool ESPManager::isConfig() {
    int size;
    this->readEEPROM(&size, 0);
    if (size == 255 || size == 0) {
        return false;
    } else {
        return true;
    }
}

void ESPManager::setGlobalState(GlobalState state) {
    ESPManager::globalState = state;
}

GlobalState ESPManager::getGlobalState() {
    return ESPManager::globalState;
}

void ESPManager::restart() {
    Logger.Warning("********************SOULPOT_ANALYZER OS Rebooting...********************");
    delay(500);
    Logger.Warning("********************SOULPOT_ANALYZER OS Bye !***************************");
    ESP.restart();
}

void ESPManager::reset() {
    Logger.Warning("********************SELF DESTRUCT MODE ENGAGED********************");
    delay(500);
    for (int i = 0; i < EEPROM.length(); i++) {
        EEPROM.write(i, 0);
        int status = (i / EEPROM.length() * 100);
        std::string statusStr = "********************SOULPOT_ANALYZER 0S Erasing..." + std::to_string(status) + "****************";
        Logger.Warning(statusStr.c_str());
    }
    EEPROM.commit();
}

void ESPManager::saveEEPROM(std::string data) {
    int len = data.length();
    // writing size of element to memory
    EEPROM.write(this->lastIndex, len);
    int firstIndex = this->lastIndex + 1;

    for (int i = 0; i < len + 1; i++) {
        EEPROM.write(firstIndex + i, data[i]);
        this->lastIndex++;
    }

    if (EEPROM.commit()) {
        Logger.Debug("Save sucess !");
    } else {
        Logger.Error("Failed to save in EEPROM!");
    }
}

void ESPManager::readEEPROM(int* target, int offset) {
    Logger.Info("Reading EEPROM...");
    int data;
    data = EEPROM.read(offset);
    *target = data;
}

std::string ESPManager::readEEPROM(int offset, int len) {
    Logger.Info("Reading EEPROM...");
    char data[len + 1];
    for (int i = 0; i < len + 1; i++) {
        data[i] = EEPROM.read(offset + i);
    }
    data[len] = '\0';
    return std::string(data);
}

void ESPManager::controlButton() {
    int buttonState = digitalRead(BUTTON);
    if(buttonState == LOW && this->buttonLastState == HIGH) {
        this->buttonReleasedTime = millis();
        long unsigned int pressedDuration = this->buttonReleasedTime - this->buttonPressedTime;
        if (pressedDuration >= LONG_PRESS) {
        ESPManager::reset();
        ESPManager::restart();
        } else if (pressedDuration <= SHORT_PRESS) {
        ESPManager::restart();
        }
    } else if (buttonState == HIGH && this->buttonLastState == LOW) {
        this->buttonPressedTime = millis();
    }
    this->buttonLastState = buttonState;
    }