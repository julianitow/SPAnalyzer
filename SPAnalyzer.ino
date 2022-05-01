#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <string>
#include <WiFi.h>
#include <EEPROM.h>

// C99 libraries
#include <cstdlib>
#include <time.h>

// Libraries for MQTT client and WiFi connection
#include <mqtt_client.h>

// Azure IoT SDK for C includes
#include <az_core.h>
#include <az_iot.h>
#include <azure_ca.h>

// Additional sample headers 
#include "include/AzIoTSasToken.h"
#include "include/SerialLogger.h"
#include "include/iot_configs.h"

// When developing for your own Arduino-based platform,
// please follow the format '(ard;<platform>)'. 
#define AZURE_SDK_CLIENT_USER_AGENT "c/" AZ_SDK_VERSION_STRING "(ard;esp32)"

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

#define deviceName "SOULPOT_ESP32_00"
#define SERVICE_UUID "80b7f088-0084-43e1-a687-8457bcb2dbc8"
#define CHAR_UUID "96c44fd5-c309-4553-a11e-b8457810b94c"
#define DESC_UUID "544bd464-8388-42aa-99cd-81cf6e6042d7"

#define EEPROM_ADDR 0

// Translate iot_configs.h defines into variables used by the sample
static const char* host = IOT_CONFIG_IOTHUB_FQDN;
static const char* mqtt_broker_uri = "mqtts://" IOT_CONFIG_IOTHUB_FQDN;
static const char* device_id = IOT_CONFIG_DEVICE_ID;
static const int mqtt_port = AZ_IOT_DEFAULT_MQTT_CONNECT_PORT;

// Memory allocated for the sample's variables and structures.
static esp_mqtt_client_handle_t mqtt_client;
static az_iot_hub_client client;

static char mqtt_client_id[128];
static char mqtt_username[128];
static char mqtt_password[200];
static uint8_t sas_signature_buffer[256];
static unsigned long next_data_send_time_ms = 0;
static const char* analyzer_topic = deviceName;
static uint32_t payload_send_count = 0;

#define INCOMING_DATA_BUFFER_SIZE 128
static char incoming_data[INCOMING_DATA_BUFFER_SIZE];

BLECharacteristic wifiParamChar(CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
BLEDescriptor wifiParamCharDesc(DESC_UUID);

static AzIoTSasToken sasToken(&client, AZ_SPAN_FROM_STR(IOT_CONFIG_DEVICE_KEY),AZ_SPAN_FROM_BUFFER(sas_signature_buffer), AZ_SPAN_FROM_BUFFER(mqtt_password));

bool deviceConnected = false;

static std::string _ssid, _password; 

void wifiSetup(std::string, std::string);

static void initializeTime()
{
  Logger.Info("Setting time using SNTP");

  configTime(GMT_OFFSET_SECS, GMT_OFFSET_SECS_DST, NTP_SERVERS);
  time_t now = time(NULL);
  while (now < UNIX_TIME_NOV_13_2017)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  Logger.Info("Time initialized!");
}

void receivedCallback(char* topic, byte* payload, unsigned int length)
{
  Logger.Info("Received [");
  Logger.Info(topic);
  Logger.Info("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println("");
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  switch (event->event_id)
  {
    int i, r;

    case MQTT_EVENT_ERROR:
      Logger.Info("MQTT event MQTT_EVENT_ERROR");
      break;
    case MQTT_EVENT_CONNECTED:
      Logger.Info("MQTT event MQTT_EVENT_CONNECTED");

      r = esp_mqtt_client_subscribe(mqtt_client, AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC, 1);
      if (r == -1)
      {
        Logger.Error("Could not subscribe for cloud-to-device messages.");
      }
      else
      {
        Logger.Info("Subscribed for cloud-to-device messages; message id:"  + String(r));
      }

      break;
    case MQTT_EVENT_DISCONNECTED:
      Logger.Info("MQTT event MQTT_EVENT_DISCONNECTED");
      break;
    case MQTT_EVENT_SUBSCRIBED:
      Logger.Info("MQTT event MQTT_EVENT_SUBSCRIBED");
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      Logger.Info("MQTT event MQTT_EVENT_UNSUBSCRIBED");
      break;
    case MQTT_EVENT_PUBLISHED:
      Logger.Info("MQTT event MQTT_EVENT_PUBLISHED");
      break;
    case MQTT_EVENT_DATA:
      Logger.Info("MQTT event MQTT_EVENT_DATA");

      for (i = 0; i < (INCOMING_DATA_BUFFER_SIZE - 1) && i < event->topic_len; i++)
      {
        incoming_data[i] = event->topic[i]; 
      }
      incoming_data[i] = '\0';
      Logger.Info("Topic: " + String(incoming_data));
      
      for (i = 0; i < (INCOMING_DATA_BUFFER_SIZE - 1) && i < event->data_len; i++)
      {
        incoming_data[i] = event->data[i]; 
      }
      incoming_data[i] = '\0';
      Logger.Info("Data: " + String(incoming_data));

      break;
    case MQTT_EVENT_BEFORE_CONNECT:
      Logger.Info("MQTT event MQTT_EVENT_BEFORE_CONNECT");
      break;
    default:
      Logger.Error("MQTT event UNKNOWN");
      break;
  }

  return ESP_OK;
}

static void initializeIoTHubClient()
{
  az_iot_hub_client_options options = az_iot_hub_client_options_default();
  options.user_agent = AZ_SPAN_FROM_STR(AZURE_SDK_CLIENT_USER_AGENT);

  if (az_result_failed(az_iot_hub_client_init(
          &client,
          az_span_create((uint8_t*)host, strlen(host)),
          az_span_create((uint8_t*)device_id, strlen(device_id)),
          &options)))
  {
    Logger.Error("Failed initializing Azure IoT Hub client");
    return;
  }

  size_t client_id_length;
  if (az_result_failed(az_iot_hub_client_get_client_id(
          &client, mqtt_client_id, sizeof(mqtt_client_id) - 1, &client_id_length)))
  {
    Logger.Error("Failed getting client id");
    return;
  }

  if (az_result_failed(az_iot_hub_client_get_user_name(
          &client, mqtt_username, sizeofarray(mqtt_username), NULL)))
  {
    Logger.Error("Failed to get MQTT clientId, return code");
    return;
  }

  Logger.Info("Client ID: " + String(mqtt_client_id));
  Logger.Info("Username: " + String(mqtt_username));
}

static int initializeMqttClient()
{
  if (sasToken.Generate(SAS_TOKEN_DURATION_IN_MINUTES) != 0)
  {
    Logger.Error("Failed generating SAS token");
    return 1;
  }

  esp_mqtt_client_config_t mqtt_config;
  memset(&mqtt_config, 0, sizeof(mqtt_config));
  mqtt_config.uri = mqtt_broker_uri;
  mqtt_config.port = mqtt_port;
  mqtt_config.client_id = mqtt_client_id;
  mqtt_config.username = mqtt_username;
  mqtt_config.password = (const char*)az_span_ptr(sasToken.Get());
  mqtt_config.keepalive = 30;
  mqtt_config.disable_clean_session = 0;
  mqtt_config.disable_auto_reconnect = false;
  mqtt_config.event_handle = mqtt_event_handler;
  mqtt_config.user_context = NULL;
  mqtt_config.cert_pem = (const char*)ca_pem;

  mqtt_client = esp_mqtt_client_init(&mqtt_config);

  if (mqtt_client == NULL)
  {
    Logger.Error("Failed creating mqtt client");
    return 1;
  }

  esp_err_t start_result = esp_mqtt_client_start(mqtt_client);

  if (start_result != ESP_OK)
  {
    Logger.Error("Could not start mqtt client; error code:" + start_result);
    return 1;
  }
  else
  {
    Logger.Info("MQTT client started");
    return 0;
  }
}

/*
 * @brief           Gets the number of seconds since UNIX epoch until now.
 * @return uint32_t Number of seconds.
 */
static uint32_t getEpochTimeInSecs() 
{ 
  return (uint32_t)time(NULL);
}

static void establishConnection()
{
  initializeTime();
  initializeIoTHubClient();
  (void)initializeMqttClient();
}

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

    wifiSetup(ssid, password);
  }
};

void wifiSetup(std::string ssid, std::string password) {
  _ssid = ssid;
  _password = password;
  WiFi.setHostname(deviceName);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("WifiConfig connecting to ");
  Serial.println(ssid.c_str());
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("Successfully connected to ");
  Serial.println(ssid.c_str());
  Serial.print("Local ip: ");
  Serial.println(WiFi.localIP());
  saveConfigEEPROM();
}

void saveConfigEEPROM() {
  Serial.print("Saving credentials to EEPROM...");
  int ssidLen = _ssid.length();
  EEPROM.write(EEPROM_ADDR, ssidLen);
  for (int i = 0; i < ssidLen; i++) {
    EEPROM.write(EEPROM_ADDR + 1 + i, _ssid[i]);
  }

  int passLen = _password.length();
  int offset = ssidLen + 2;
  EEPROM.write(offset, passLen);

  for (int i = 0; i < passLen; i++) {
    EEPROM.write(offset + 1 + i, _password[i]);
  }

  if (EEPROM.commit()) {
    Serial.println("Sucess !");
  } else {
    Serial.println("Failed !");
  }
}

void readConfig() {
  Serial.println("Fetching config saved in EEPROM...");
  // SSID
  int ssidLen = EEPROM.read(EEPROM_ADDR);
  Serial.print("Size of eeprom ssid:");
  Serial.println(ssidLen);
  char ssid[ssidLen + 1];
  for (int i = 0; i < ssidLen; i++) {
    ssid[i] = EEPROM.read(EEPROM_ADDR + 1 + i);
  }
  ssid[ssidLen] = '\0';
  Serial.print("SSID EEPROM: ");
  Serial.println(ssid);

  //PASSWORD
  int offset = ssidLen + 2;
  int passLen = EEPROM.read(offset);
  Serial.print("Size of eeprom password:");
  Serial.println(passLen);
  char password[passLen + 1];
  for (int i = 0; i < offset + passLen; i++) {
    password[i] = EEPROM.read(offset + 1 + i);
  }
  password[passLen] = '\0';
  if (ssidLen > 0 && passLen > 0) {
    _ssid = ssid;
    _password = password;
  }
  Serial.print("PASSWORD EEPROM: ");
  Serial.println(password);
}

void bluetoothSetup() {
  BLEDevice::init(deviceName);
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

void sendData() {
  std::string data = "Hello from analyzer";
  /*az_span payload = az_span_copy(payload, AZ_SPAN_FROM_STR(data));
  (void)az_span_u32toa(payload, payload_send_count++, &payload);*/
}

void setup() {
  Serial.begin(115200);
  Serial.println("Analyzer initializing...");
  EEPROM.begin(200);
  readConfig();
  if (_ssid.length() > 0 && _password.length() > 0) {
    Serial.println("Already config, booting...");
    wifiSetup(_ssid, _password);
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WIFI ERROR, switching to bluetooth mode");
      bluetoothSetup();
    } else {
        //connexion à azure
        Serial.print("Azure connection...");
        establishConnection();
    }
  } else {
    Serial.println("Configuration mode detected");
    bluetoothSetup();
  }
}

void loop() {
  
}
