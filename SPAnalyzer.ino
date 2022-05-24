#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <string>
#include <WiFi.h>
#include <EEPROM.h>

// temperature sensor
#include <OneWire.h>
#include <DallasTemperature.h>

// luminosity sensor
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"

// C99 libraries
#include <cstdlib>
#include <time.h>

// Libraries for MQTT client and WiFi connection
#include <mqtt_client.h>

// Azure IoT SDK for C includes
#include <az_core.h>
#include <az_iot.h>
#include <azure_ca.h>

// Additional sample headers #include "inc/AzIoTSasToken.h"
#include "AzIoTSasToken.h"
#include "SerialLogger.h"
#include "iot_configs.h"

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
static char analyzer_topic[TOPIC_SIZE];
static uint32_t payload_send_count = 0;

static char incoming_data[INCOMING_DATA_BUFFER_SIZE];
static uint8_t payload[PAYLOAD_SIZE];

struct tm* ptm;

static char* networks; 

BLECharacteristic wifiParamChar(CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
BLEDescriptor wifiParamCharDesc(DESC_UUID);

bool deviceConnected = false;
static std::string _ssid, _password; 

// sensors
const int oneWireBus = 4;
const int relayBus = 26;
const int moistureBus = 36;

// leds
const int redLed = 39;
const int greenLed = 23;

// button
const int buttonBus = 15;
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;
int lastState = LOW;
int currentState;
int nbPressed = 0;
const int SHORT_PRESS_TIME = 500;
const int RESET_PRESS_TIME = 10000;

TaskHandle_t greenBlinkTask;

OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591); 

void wifiSetup(std::string, std::string);
static AzIoTSasToken sasToken(&client, AZ_SPAN_FROM_STR(IOT_CONFIG_DEVICE_KEY),AZ_SPAN_FROM_BUFFER(sas_signature_buffer), AZ_SPAN_FROM_BUFFER(mqtt_password));

//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Logger.Info("New client connected");
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Logger.Warning("Client disconnected");
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic* pCharacteristic) {
    Serial.println("onRead triggered");
    pCharacteristic->setValue(networks);
  }
  void onWrite(BLECharacteristic* pCharacteristic) {
    Logger.Info("received: ");
    Logger.Info(pCharacteristic->getValue().c_str());
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
    _ssid = ssid;
    _password = password;
    wifiSetup(ssid, password);
  }
};

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
      incomingDataFilter();
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

static void incomingDataFilter() {
  std::string data(incoming_data);
  std::string propName = data.substr(0, data.find(":"));
  propName = data.substr(data.find("{") + 2, propName.length() - 3);
  if (propName == "sprinkle") {
    std::string propVal = data.substr(data.find(":") + 2, data.length() - 1);
    propVal = propVal.substr(0, propVal.find("}") - 1);
    if (propVal == "true") {
      sprink(true);
    } else {
      sprink(false);
    }
  }
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

static void initializeSensors() {
  if (tsl.begin()) {
    Logger.Info("TSL2591 found !");
    configureLumSensor();
  } else {
    Logger.Error("TSL2591 not found !");
  }
}

void sprink(bool status) {
  if (status) {
    Logger.Debug("SPRINK");
    digitalWrite(relayBus, HIGH);
  } else {
    Logger.Debug("NOT SPRINK");
    digitalWrite(relayBus, LOW);
  }
}

float getLux() {
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir, full;
  ir = lum >> 16;
  full = lum & 0xFFFF;
  return tsl.calculateLux(full, ir);
}

int getMoisture() {
  const int value = analogRead(moistureBus);
  return value;
}

void configureLumSensor(void)
{
  // You can change the gain on the fly, to adapt to brighter/dimmer light situations
  //tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain (bright light)
  tsl.setGain(TSL2591_GAIN_MED);      // 25x gain
  //tsl.setGain(TSL2591_GAIN_HIGH);   // 428x gain
 
  // Changing the integration time gives you a longer time over which to sense light
  // longer timelines are slower, but are good in very low light situtations!
  //tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time (bright light)
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_200MS);
  tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_400MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_500MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time (dim light)
 
  /* Display the gain and integration time for reference sake */  
  Logger.Info("------------------------------------");
  Logger.Info("Gain:         ");
  tsl2591Gain_t gain = tsl.getGain();
  switch(gain)
  {
    case TSL2591_GAIN_LOW:
      Logger.Info("1x (Low)");
      break;
    case TSL2591_GAIN_MED:
      Logger.Info("25x (Medium)");
      break;
    case TSL2591_GAIN_HIGH:
      Logger.Info("428x (High)");
      break;
    case TSL2591_GAIN_MAX:
      Logger.Info("9876x (Max)");
      break;
  }
  Logger.Info("Timing:       ");
  Serial.print((tsl.getTiming() + 1) * 100, DEC); 
  Serial.println(F(" ms"));
  Serial.println(F("------------------------------------"));
  Serial.println(F(""));
}

void wifiSetup(std::string ssid, std::string password) {
  _ssid = ssid;
  _password = password;
  WiFi.setHostname(DEVICE_NAME);
  WiFi.mode(WIFI_STA);  
  WiFi.begin(ssid.c_str(), password.c_str());
  Logger.Info("WifiConfig connecting to ");
  Serial.print(ssid.c_str());
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Logger.Info("Successfully connected to ");
  Serial.print(ssid.c_str());
  Logger.Info(" Local ip: ");
  Serial.println(WiFi.localIP());
  saveConfigEEPROM();
}

void listNetworks() {
  Logger.Info("Networks available:");
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1) {
    Logger.Error("Couldn't get a wifi connection");
  }
  networks = (char *) malloc(1); //static char*
  // print the network number and name for each network found:
  for (int net = 0; net < numSsid; net++) {
    const char* ssid = WiFi.SSID(net).c_str();
    if (strlen(ssid) > 0) {
      int size = strlen(networks) + 1 + strlen(ssid) + 1;
      char* new_networks = (char *) malloc(size);
      strcpy(new_networks, networks);
      strcat(new_networks, ssid);
      Serial.println(size);
      Serial.println(new_networks);
    }
    /*strcat(cSsid, ssid);
    Serial.print(WiFi.SSID(net));
    Serial.print(" => Signal: ");
    Serial.print(WiFi.RSSI(net));
    Serial.print(" dBm\n");
    strcat(networks, cSsid);
    //strcat(",", networks);
    Logger.Debug(networks);*/
  }
}

void eraseMemory() {
  createBlinkGreenTask();
  Logger.Warning("********************SELF DESTRUCT MODE ENGAGED********************");
  delay(500);
  const int lastIndex = _ssid.length() + _password.length();
  for (int i = 0; i < lastIndex; i++) {
    EEPROM.write(i, 0);
    int status = (i / lastIndex * 100);
    std::string statusStr = "********************SOULPOT_ANALYZER 0S Erasing..." + std::to_string(status) + "****************";
    Logger.Warning(statusStr.c_str());
  }
  EEPROM.commit();
  delay(500);
  Logger.Warning("********************SOULPOT_ANALYZER 0S Rebooting...**************");
  delay(500);
  Logger.Warning("********************SOULPOT_ANALYZER 0S Bye.**********************");
  ESP.restart();
}

void saveConfigEEPROM() {
  Logger.Info("Saving credentials to EEPROM...");
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
    Logger.Debug("Success !");
  } else {
    Logger.Error("Failed to save in EEPROM!");
  }
}

void readConfig() {
  Logger.Info("Fetching config saved in EEPROM...");
  // SSID
  int ssidLen = EEPROM.read(EEPROM_ADDR);
  char ssid[ssidLen + 1];
  for (int i = 0; i < ssidLen; i++) {
    ssid[i] = EEPROM.read(EEPROM_ADDR + 1 + i);
  }
  ssid[ssidLen] = '\0';
  if (ssidLen > 0) {
    _ssid = ssid;
  }
  Logger.Debug("SSID EEPROM 1, _ssid: ");
  Logger.Debug(ssid);
  Logger.Debug(_ssid.c_str());
  //PASSWORD
  int offset = ssidLen + 2;
  int passLen = EEPROM.read(offset);
  char password[passLen + 1];
  for (int i = 0; i < offset + passLen; i++) {
    password[i] = EEPROM.read(offset + 1 + i);
  }
  password[passLen] = '\0';
  if (ssidLen > 0 && passLen > 0) {
    Logger.Debug("SSID EEPROM 2: ");
    Serial.print(ssid);
    Logger.Debug("PASSWORD EEPROM: ");
    Serial.print(password);
    _ssid = ssid;
    _password = password;
  }
}

void bluetoothSetup() {
  BLEDevice::init(DEVICE_NAME);
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
  Logger.Info("Waiting for a client to notify...");
}

static void getPayload(az_span payload, az_span* out_payload) {
  az_span original_payload = payload;
  payload = az_span_copy(
      payload, AZ_SPAN_FROM_STR("{ \"msgCount\": "));
  (void)az_span_u32toa(payload, payload_send_count++, &payload);
  payload = az_span_copy(
      payload, AZ_SPAN_FROM_STR(",\"device_id\": \""));
  payload = az_span_copy(
      payload, AZ_SPAN_FROM_STR(IOT_CONFIG_DEVICE_ID));
  payload = az_span_copy(
      payload, AZ_SPAN_FROM_STR("\""));
  payload = az_span_copy(payload, constructDataPayload()); 
  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" }"));
  payload = az_span_copy_u8(payload, '\0');

  *out_payload = az_span_slice(original_payload, 0, az_span_size(original_payload) - az_span_size(payload));
}

static az_span constructDataPayload() {
  time_t now = time(NULL);
  ptm = gmtime(&now);
  // Logger.Debug(isotime(ptm));
  double temp = getTemperature();
  double lum = getLux();
  int moisture = getMoisture();
  std::string payloadStr = ",\"data\": {\n \"temp\": \"" + std::to_string(temp) + "\",\"hygro\": \""+ std::to_string(moisture) + "\",\"lum\": \"" + std::to_string(lum) + "\"\n },";
  az_span span = az_span_create_from_str((char*) payloadStr.c_str());
  return span;
}

void sendData() {
  az_span data = AZ_SPAN_FROM_BUFFER(payload);
  
  if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(
          &client, NULL, analyzer_topic, sizeof(analyzer_topic), NULL)))
  {
    Logger.Error("Failed az_iot_hub_client_telemetry_get_publish_topic");
    return;
  }
  Logger.Info("Payload construction: in progress...");
  getPayload(data, &data);
  Logger.Info("Payload construction: success");
  if (esp_mqtt_client_publish(
          mqtt_client,
          analyzer_topic,
          (const char*)az_span_ptr(data),
          az_span_size(data),
          MQTT_QOS1,
          DO_NOT_RETAIN_MSG)
      == 0)
  {
    Logger.Error("Failed publishing");
  }
  else
  {
    Logger.Info("Message published successfully");
  }
}

float getTemperature() {
  sensors.requestTemperatures(); 
  return sensors.getTempCByIndex(0);
}

void greenBlink(void * pvParameters) {
  for(;;){
    digitalWrite(greenLed, HIGH);
    delay(500);
    digitalWrite(greenLed, LOW);
    delay(500);
  } 
}

void createBlinkGreenTask() {
  xTaskCreatePinnedToCore(greenBlink, "greenBlink", 10000, NULL, 0, &greenBlinkTask, 0);
}

void stopGreenBlink() {
  vTaskDelete(greenBlinkTask);
  digitalWrite(greenLed, HIGH);
}

void manageButtonPress() {
  currentState = digitalRead(buttonBus);
  if (lastState == HIGH && currentState == LOW) {
    pressedTime = millis();
    nbPressed++;
  } else if (lastState == LOW && currentState == HIGH) {
    releasedTime = millis();
    long pressDuration = releasedTime - pressedTime;
    nbPressed++;
    if (pressDuration > RESET_PRESS_TIME) {
      eraseMemory();
    } else if (pressDuration < SHORT_PRESS_TIME && nbPressed == 1) {
      Logger.Info("Rebooting analyzer...bye");
      ESP.restart();
    } else if (pressDuration < SHORT_PRESS_TIME && nbPressed == 2) {
      Logger.Info("Glitching !");
    }
  }
  lastState = currentState;
  nbPressed = 0;
}

void setup() {
  Serial.begin(115200);
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(buttonBus, INPUT_PULLUP);
  createBlinkGreenTask();
  Logger.Warning("********************SOULPOT_ANALYZER OS Booting...********************");
  // listNetworks();
  EEPROM.begin(200);
  readConfig();
  // TMP
  _ssid = "uifeedu75";
  _password = "mandalorianBGdu75";
  // END TMP
  if (_ssid.length() > 0 && _password.length() > 0) {
    Logger.Info("Already config, with:");
    Logger.Info(_ssid.c_str());
    Logger.Info(_password.c_str());
    wifiSetup(_ssid, _password);
    if (WiFi.status() != WL_CONNECTED) {
      Logger.Error("WIFI ERROR, switching to bluetooth mode");
      bluetoothSetup();
    } else {
        //connexion à azure
        Logger.Info("Azure connection...");
        establishConnection();
        sensors.begin();
        pinMode(relayBus, OUTPUT);
        stopGreenBlink();
    }
  } else {
    Logger.Warning("Configuration mode detected");
    Logger.Info("Waiting for a client to notify...");
    bluetoothSetup();
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    
  }
  else if (sasToken.IsExpired()) {
    Logger.Info("SAS token expired; reconnecting with a new one.");
    (void)esp_mqtt_client_destroy(mqtt_client);
    initializeMqttClient();
  } else if (millis() > next_data_send_time_ms) {
    Logger.Info("Pushing data to hub...");
    createBlinkGreenTask();
    sendData();
    next_data_send_time_ms = millis() + 15000;
    stopGreenBlink();
  }

  manageButtonPress();
}
