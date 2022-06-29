#ifndef IOT_CONFIGS_H
#define IOT_CONFIGS_H

// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

// Azure IoT
#define IOT_CONFIG_IOTHUB_FQDN "fr-analyzers-hub.azure-devices.net"
#define IOT_CONFIG_DEVICE_ID "ANALYZER_01"
#define IOT_CONFIG_DEVICE_KEY "UnldpoNHIAFEl9WKhL7q6PJnpKyKi+Thky1cLDfsR8g="

// When developing for your own Arduino-based platform,
// please follow the format '(ard;<platform>)'. 
#define AZURE_SDK_CLIENT_USER_AGENT "c/" AZ_SDK_VERSION_STRING "(ard;esp32)"

#define INCOMING_DATA_BUFFER_SIZE 128

#define PAYLOAD_SIZE 200

#define TOPIC_SIZE 128

// Utility macros and defines
#define sizeofarray(a) (sizeof(a) / sizeof(a[0]))
#define NTP_SERVERS "pool.ntp.org", "time.nist.gov"
#define MQTT_QOS1 1
#define DO_NOT_RETAIN_MSG 0
#define SAS_TOKEN_DURATION_IN_MINUTES 60
#define UNIX_TIME_NOV_13_2017 1510592825

#define PST_TIME_ZONE -8
#define PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF   1

#define GMT_OFFSET_SECS (PST_TIME_ZONE * 3600)
#define GMT_OFFSET_SECS_DST ((PST_TIME_ZONE + PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF) * 3600)

// Bluetooth
#define SERVICE_UUID "80b7f088-0084-43e1-a687-8457bcb2dbc8"
#define CHAR_UUID "96c44fd5-c309-4553-a11e-b8457810b94c"
#define DESC_UUID "544bd464-8388-42aa-99cd-81cf6e6042d7"

// Moisture
#define MIN_MOISTURE_VAL 1720
#define MAX_MOISTURE_VAL 2115

// Sensors buses
#define GREENLED 23
#define ONEWIRE 33
#define RELAY 26
#define MOISTURE 34
#define BUTTON 35

// Button
const unsigned short int SHORT_PRESS = 1000;
const unsigned short int LONG_PRESS = 10000;

// EEPROM 1st case addr
#define EEPROM_ADDR 0

enum GlobalState { ANALYZER_OK, ANALYZER_PAIRING, ANALYZER_ERROR = -1 };
#endif