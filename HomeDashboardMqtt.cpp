#include <FS.h>
#include "Arduino.h"
#include "HomeDashboardMqtt.h"

#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

#include <PubSubClient.h>

#if (MQTT_MAX_PACKET_SIZE < 255)
#error Too small MQTT_MAX_PACKET_SIZE
#endif
// the following override is not working due to this issue: https://github.com/knolleary/pubsubclient/pull/282
// #define MQTT_MAX_PACKET_SIZE    254
// you need to change PubSubClient.h MQTT_MAX_PACKET_SIZE to 254

HomeDashboardMqtt *homeDashboardMqtt;

inline void homeDashboardMqttSaveConfigCallback()
{
  homeDashboardMqtt->saveConfig();
}

inline void homeDashboardMqttCallback(char *topic, byte *payload, unsigned int length)
{
  homeDashboardMqtt->mqttCallback(topic, payload, length);
}

void (*resetFunc)(void) = 0; //declare reset function @ address 0

HomeDashboardMqtt::HomeDashboardMqtt(
    char *device_type,
    const int version,
    const int status_led,
    const int wifi_reset_button,
    void (*currentState)(JsonObject &json),
    void (*loadSettings)(JsonObject &json),
    void (*saveSettings)(JsonObject &json),
    void (*onCommand)(JsonObject &json),
    void (*onLogEntry)(const char *message)) : wifiManager(),
                                               espClient(),
                                               client(espClient),
                                               custom_mqtt_server("server", "mqtt server", mqtt_server, 40),
                                               custom_mqtt_port("port", "mqtt port", mqtt_port, 6),
                                               custom_mqtt_user("user", "mqtt user", mqtt_user, 100),
                                               custom_mqtt_password("password", "mqtt password", mqtt_password, 100),
                                               custom_device_name("deviceName", "device name", device_name, 32)
{
  this->logToSerial = true;
  this->logToMqtt = true;
  this->wifi_reset_button = wifi_reset_button;
  this->device_type = device_type;
  this->version = version;
  this->status_led = status_led;
  this->mqttFailedBefore = false;
  this->currentStateCallBack = currentState;
  this->loadSettingsCallBack = loadSettings;
  this->saveSettingsCallBack = saveSettings;
  this->onCommandCallBack = onCommand;
  this->onLogEntryCallback = onLogEntry;
  pinMode(this->status_led, OUTPUT);
  pinMode(this->wifi_reset_button, INPUT_PULLUP);

  strcpy(this->mqtt_port, "1883");

  this->jsonBuffer = new DynamicJsonBuffer(512);

  this->wifiManager.setSaveConfigCallback(&homeDashboardMqttSaveConfigCallback);
}

void HomeDashboardMqtt::init()
{
  this->loadConfig();

  analogWrite(this->status_led, NETWORK_STATUS_NOT_CONNECTED);

  this->wifiManager.addParameter(&this->custom_mqtt_server);
  this->wifiManager.addParameter(&this->custom_mqtt_port);
  this->wifiManager.addParameter(&this->custom_mqtt_user);
  this->wifiManager.addParameter(&this->custom_mqtt_password);
  this->wifiManager.addParameter(&this->custom_device_name);

  WiFi.hostname(this->device_name);
  this->wifiManager.setConfigPortalTimeout(60 * 3);

  // drawText("Initializing", "wait for 2 sec while you can reset", "");
  this->info("wait for 2 sec...");
  delay(2000);
  bool wifiReset = digitalRead(this->wifi_reset_button) == PRESSED;
  if (wifiReset)
  {
    this->info("reset pressed...");
    this->flashLedIn();
    this->flashLedOut();

    wifiReset = digitalRead(this->wifi_reset_button) == PRESSED;
    if (wifiReset)
    {
      this->flashLedIn();
      this->flashLedOut();

      this->info("reset requested...");

      sprintf(this->sprintfBuffer, "Find %s wifi", this->device_type);
      this->info(this->sprintfBuffer);
      this->info("type 192.168.4.1 to your browser to configure");

      SPIFFS.format();
      this->wifiManager.resetSettings();
    }
  }
  analogWrite(this->status_led, NETWORK_STATUS_NOT_CONNECTED);
  // drawText("Connecting to wifi", WiFi.SSID().c_str(), "");
  if (!this->wifiManager.autoConnect(this->device_type, "password"))
  {
    this->error("failed to connect and hit timeout, you should reset to reconfigure");
  }
  else
  {
    // drawText("connected to wifi", WiFi.SSID().c_str(), IpAddress2String(WiFi.localIP()).c_str());
    this->info("connected, ip:");
    this->info(ipAddress2String(WiFi.localIP()).c_str());
  }
}

void HomeDashboardMqtt::flashLedOut()
{
  for (int i = 1023; i > 0; i--)
  {
    analogWrite(this->status_led, i);
    delay(1);
  }
}

void HomeDashboardMqtt::flashLedIn()
{
  for (int i = 0; i < 1024; i++)
  {
    analogWrite(this->status_led, i);
    delay(1);
  }
}

void HomeDashboardMqtt::loop()
{
  if (!WiFi.isConnected())
  {
    analogWrite(this->status_led, NETWORK_STATUS_NOT_CONNECTED);
    this->error("Wifi connection lost...");
    // drawText("Connection lost", WiFi.SSID().c_str(), "");
    if (WiFi.reconnect())
    {
      // drawText("Connected to wifi", WiFi.SSID().c_str(), this->IpAddress2String(WiFi.localIP()).c_str()); //TODO: print temperature
      analogWrite(this->status_led, NETWORK_STATUS_ONLY_WIFI);
      this->info("successfully reconnected, local ip:");
      this->info(ipAddress2String(WiFi.localIP()).c_str());
      this->connectToMqttIfNotConnected();
    }
  }
  else
  {
    this->connectToMqttIfNotConnected();
  }
}

void HomeDashboardMqtt::initTopic()
{
  sprintf(this->topicDeviceCommand, "homedashboard/device/%s/command", this->device_name);
  sprintf(this->topicDeviceState, "homedashboard/device/%s/state", this->device_name);
  sprintf(this->topicDeviceHeartbeat, "homedashboard/device/%s/heartbeat", this->device_name);
  sprintf(this->topicDeviceChangeSettings, "homedashboard/device/%s/changeSettings", this->device_name);
  sprintf(this->topicDeviceCurrentSettings, "homedashboard/device/%s/currentSettings", this->device_name);
  sprintf(this->topicDeviceLog, "homedashboard/device/%s/log", this->device_name);
}

void HomeDashboardMqtt::connectToMqttIfNotConnected()
{
  if (!this->client.connected())
  {
    long now = millis();
    if (this->lastConnectRetry + 1000 < now)
    {
      this->connectToMqtt();
    }
  }
  else
  {
    this->client.loop();
  }
}

void HomeDashboardMqtt::sendDeviceDiscoveryFromDevice()
{
  JsonObject &json = this->jsonBuffer->createObject();
  json["name"] = this->device_name;
  json["type"] = this->device_type;
  json["version"] = this->version;
  json["ip"] = ipAddress2String(WiFi.localIP()).c_str();
  json["rssi"] = WiFi.RSSI();

  char jsonChar[200];
  json.printTo((char *)jsonChar, json.measureLength() + 1);

  client.publish(MQTT_TOPIC_DISCOVERY_FROM_DEVICE, jsonChar, false);
  this->jsonBuffer->clear();
}

void HomeDashboardMqtt::publishCurrentSettings()
{
  if (this->client.connected())
  {
    JsonObject &json = jsonBuffer->createObject();

    // load current configuration to json
    this->saveSettingsCallBack(json);

    char jsonChar[255];
    json.printTo((char *)jsonChar, json.measureLength() + 1);

    int result = client.publish(this->topicDeviceCurrentSettings, jsonChar, json.measureLength());
    if (result == 0)
    {
      sprintf(this->sprintfBuffer, "current settings published to %s", this->topicDeviceCurrentSettings);
      this->info(this->sprintfBuffer);
    }
    else
    {
      sprintf(this->sprintfBuffer, "failed to published current settings to %s", this->topicDeviceCurrentSettings);
      this->error(this->sprintfBuffer);
    }

    this->jsonBuffer->clear();
  }
}

void HomeDashboardMqtt::publishState()
{
  if (this->client.connected())
  {
    JsonObject &json = this->jsonBuffer->createObject();
    // todo callback to create state

    currentStateCallBack(json);

    char jsonChar[255];
    json.printTo((char *)jsonChar, json.measureLength() + 1);
    int result = client.publish(this->topicDeviceState, jsonChar, json.measureLength());

    sprintf(this->sprintfBuffer, "publish state to topic: %s", this->topicDeviceState);
    this->info(this->sprintfBuffer);
    this->jsonBuffer->clear();
  }
}

void HomeDashboardMqtt::connectToMqtt()
{
  sprintf(this->sprintfBuffer, "trying to connect to MQTT server [%s], user: [%s]", this->device_name, this->mqtt_user);
  this->info(sprintfBuffer);

  this->client.setCallback(&homeDashboardMqttCallback);
  this->client.setServer(this->mqtt_server, atoi(this->mqtt_port));

  analogWrite(this->status_led, NETWORK_STATUS_CONNECTING_TO_MQTT);

  boolean connectResult = false;

  this->initTopic();

  JsonObject &json = this->jsonBuffer->createObject();
  json["offline"] = true;
  char jsonChar[200];
  json.printTo((char *)jsonChar, json.measureLength() + 1);

  if (strlen(this->mqtt_user) > 0)
  {
    connectResult = this->client.connect(this->device_name, this->mqtt_user, this->mqtt_password, this->topicDeviceState, 1, true, jsonChar);
  }
  else
  {
    connectResult = this->client.connect(this->device_name, this->topicDeviceState, 1, true, jsonChar);
  }
  this->jsonBuffer->clear();

  this->lastConnectRetry = millis();

  if (connectResult)
  {
    this->info("Successfully connected to MQTT server");
    this->mqttFailedBefore = false;

    this->client.subscribe(MQTT_TOPIC_DISCOVERY_TO_DEVICE, 1);
    this->client.subscribe(this->topicDeviceCommand, 0); // qos 2 is not supported. TODO make it qos:1 and idempotent
    this->client.subscribe(this->topicDeviceHeartbeat, 1);
    this->client.subscribe(this->topicDeviceChangeSettings, 1);
    this->sendDeviceDiscoveryFromDevice();
    this->publishState();
    analogWrite(this->status_led, NETWORK_STATUS_CONNECTED);
  }
  else
  {
    analogWrite(this->status_led, NETWORK_STATUS_ONLY_WIFI);
    if (!this->mqttFailedBefore)
    {
      sprintf(this->sprintfBuffer, "Failed to connect to MQTT server: %s, port: %s", this->mqtt_server, this->mqtt_port);
      this->error(this->sprintfBuffer);
    }
    this->mqttFailedBefore = true;
  }
}

void HomeDashboardMqtt::onDiscoveryMessage(byte *payload, unsigned int length)
{
  JsonObject &json = this->jsonBuffer->parseObject(payload);
  if (json.success())
  {
    this->sendDeviceDiscoveryFromDevice();
  }
  else
  {
    Serial.println("cannot parse discovery message json!");
  }
  this->jsonBuffer->clear();
}

void HomeDashboardMqtt::onCommand(byte *payload, unsigned int length)
{
  JsonObject &json = this->jsonBuffer->parseObject(payload);
  if (json.success())
  {
    this->onCommandCallBack(json);
  }
  else
  {
    Serial.println("cannot parse command json!");
  }
  this->jsonBuffer->clear();
}

void HomeDashboardMqtt::onHeartbeat(byte *payload, unsigned int length)
{
  this->publishState();
}

void HomeDashboardMqtt::onSettingsChange(byte *payload, unsigned int length)
{
  JsonObject &json = this->jsonBuffer->parseObject(payload);
  if (json.success())
  {
    this->loadSettingsCallBack(json);
    this->saveConfig();
  }
  this->jsonBuffer->clear();
}

void HomeDashboardMqtt::mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strcmp(topic, MQTT_TOPIC_DISCOVERY_TO_DEVICE) == 0)
  {
    this->onDiscoveryMessage(payload, length);
  }
  else if (strcmp(topic, this->topicDeviceCommand) == 0)
  {
    this->onCommand(payload, length);
  }
  else if (strcmp(topic, this->topicDeviceHeartbeat) == 0)
  {
    this->onHeartbeat(payload, length);
  }
  else if (strcmp(topic, this->topicDeviceChangeSettings) == 0)
  {
    this->onSettingsChange(payload, length);
  }
  else
  {
    Serial.println("Unknown topic!");
  }
}

void HomeDashboardMqtt::saveConfigCallback()
{
  this->saveConfig();
  this->initTopic();
}

void HomeDashboardMqtt::loadConfig()
{
  this->info("mounting FS...");
  if (SPIFFS.begin())
  {
    //SPIFFS.format();
    //this->wifiManager.resetSettings();

    this->info("file system mounted...");
    if (SPIFFS.exists("/config.json"))
    {
      //file exists, reading and loading
      this->info("reading config file...");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        this->info("config file opened");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        JsonObject &json = this->jsonBuffer->parseObject(buf.get());

        char jsonChar[255];
        json.printTo((char *)jsonChar, json.measureLength() + 1);
        this->info(jsonChar);

        if (json.success())
        {
          this->info("config json parsed successfully");

          strcpy(device_name, json["device_name"]);
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          this->initTopic();

          this->loadSettingsCallBack(json["custom"]);
        }
        else
        {
          SPIFFS.format();
          this->wifiManager.resetSettings();
          this->error("failed to load json config");
        }
        configFile.close();
        this->jsonBuffer->clear();
      }
    }
    else
    {
      this->error("No config file present");
    }
  }
  else
  {
    this->error("failed to mount FS");
  }
}

void HomeDashboardMqtt::saveConfig()
{

  strcpy(this->mqtt_server, this->custom_mqtt_server.getValue());
  strcpy(this->mqtt_port, this->custom_mqtt_port.getValue());
  strcpy(this->mqtt_user, this->custom_mqtt_user.getValue());
  strcpy(this->mqtt_password, this->custom_mqtt_password.getValue());
  strcpy(this->device_name, this->custom_device_name.getValue());

  JsonObject &json = this->jsonBuffer->createObject();
  json["mqtt_server"] = this->mqtt_server;
  json["mqtt_port"] = this->mqtt_port;
  json["mqtt_user"] = this->mqtt_user;
  json["mqtt_password"] = this->mqtt_password;
  json["device_name"] = this->device_name;

  JsonObject &custom = json.createNestedObject("custom");
  this->info("Calling save settings callback");
  this->saveSettingsCallBack(custom);

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    this->error("failed to open config file for writing");
  }
  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  this->jsonBuffer->clear();
}

String ipAddress2String(const IPAddress &ipAddress)
{
  return String(ipAddress[0]) + String(".") +
         String(ipAddress[1]) + String(".") +
         String(ipAddress[2]) + String(".") +
         String(ipAddress[3]);
}

int getRSSIasQuality(int RSSI)
{
    int quality = 0;

    if (RSSI <= -100)
    {
        quality = 0;
    }
    else if (RSSI >= -50)
    {
        quality = 100;
    }
    else
    {
        quality = 2 * (RSSI + 100);
    }
    return quality;
}

void HomeDashboardMqtt::resetSettings()
{
  this->info("reset command retrieved, reset and rebooting");
  SPIFFS.format();
  this->wifiManager.resetSettings();
  resetFunc();
}

String wifiStatusAsText()
{
  int status = WiFi.status();
  if (status == WL_CONNECTED)
  {
    return "Connected";
  }
  else if (status == WL_NO_SHIELD)
  {
    return "No wifi shield";
  }
  else if (status == WL_IDLE_STATUS)
  {
    return "Idle";
  }
  else if (status == WL_NO_SSID_AVAIL)
  {
    return "No SSID available";
  }
  else if (status == WL_SCAN_COMPLETED)
  {
    return "Network scan completed";
  }
  else if (status == WL_CONNECT_FAILED)
  {
    return "Connect failed";
  }
  else if (status == WL_CONNECTION_LOST)
  {
    return "Connection lost";
  }
  else if (status == WL_DISCONNECTED)
  {
    return "Disconnected";
  }
  else {
    return "----";
  }
}

void HomeDashboardMqtt::info(const char *message)
{
  char msg[255] = "I:";
  strcat(msg, message);
  this->log(msg);
}

void HomeDashboardMqtt::info(String message)
{
  char msg[255] = "I:";
  strcat(msg, message.c_str());
  this->log(msg);
}

void HomeDashboardMqtt::error(const char *message)
{
  char msg[255] = "E:";
  strcat(msg, message);
  this->log(msg);
}

void HomeDashboardMqtt::error(String message)
{
  char msg[255] = "E:";
  strcat(msg, message.c_str());
  this->log(msg);
}

void HomeDashboardMqtt::log(const char *message)
{
  if (this->logToSerial)
  {
    Serial.println(message);
  }
  if (this->logToMqtt)
  {
    this->client.publish(this->topicDeviceLog, message, false);
  }
  this->onLogEntryCallback(message);
}