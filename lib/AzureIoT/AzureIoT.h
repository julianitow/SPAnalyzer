#ifndef AZURE_IOT_H
#define AZURE_IOT_H

// Additional sample headers #include "inc/AzIoTSasToken.h"
#include "AzIoTSasToken.h"
#include "ESPManager.h"
#include "SerialLogger.h"
#include "SensorsManager.h"
#include "../../include/iot_configs.h"
// C99 libraries
#include <cstdlib>
#include <time.h>
// Libraries for MQTT client and WiFi connection
#include <mqtt_client.h>
// Azure IoT SDK for C includes
#include <az_core.h>
#include <az_iot.h>

class AzureIot {

    private:
    static const char* host;
    static const char* mqtt_broker_uri;
    static const char* device_id;
    static int mqtt_port;
    static esp_mqtt_client_handle_t mqtt_client;
    static az_iot_hub_client client;
    static char mqtt_client_id[128];
    static char mqtt_username[128];
    static char mqtt_password[200];
    static uint8_t sas_signature_buffer[256];
    static unsigned long next_data_send_time_ms;
    static char topic[TOPIC_SIZE];
    static uint32_t payload_send_count;
    static char incoming_data[INCOMING_DATA_BUFFER_SIZE];
    static uint8_t payload[PAYLOAD_SIZE];
    static struct tm* ptm;
    static SensorsManager* sensorsManager;

    public:
    static void init(SensorsManager*);
    static AzIoTSasToken* sasToken;
    static void initializeTime();
    void receivedCallback(char*, byte*, unsigned int);
    static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t);
    static void incomingDataFilter();
    static void initializeIoTHubClient();
    static int initializeMqttClient();
    static uint32_t getEpochTimeInSecs();
    static void destroyMqtt();
    static void establishConnection();
    static void getPayload(az_span, az_span*);
    static az_span constructDataPayload();
    static void sendData();
    static bool tokenExpired();
    static bool time2send();
};

#endif