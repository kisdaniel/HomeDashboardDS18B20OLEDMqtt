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

HomeDashboardMqtt::HomeDashboardMqtt(
    char *device_type,
    const int version,
    const int status_led,
    const int wifi_reset_button,
    void (*currentState)(JsonObject &json),
    void (*loadSettings)(JsonObject &json),
    void (*saveSettings)(JsonObject &json),
    void (*onCommand)(JsonObject &json)) : wifiManager(),
                                           espClient(),
                                           client(espClient),
                                           custom_mqtt_server("server", "mqtt server", mqtt_server, 40),
                                           custom_mqtt_port("port", "mqtt port", mqtt_port, 6),
                                           custom_mqtt_user("user", "mqtt user", mqtt_user, 100),
                                           custom_mqtt_password("password", "mqtt password", mqtt_password, 100),
                                           custom_device_name("deviceName", "device name", device_name, 32)
{
  this->wifi_reset_button = wifi_reset_button;
  this->device_type = device_type;
  this->version = version;
  this->status_led = status_led;
  this->mqttFailedBefore = false;
  this->currentStateCallBack = currentState;
  this->loadSettingsCallBack = loadSettings;
  this->saveSettingsCallBack = saveSettings;
  this->onCommandCallBack = onCommand;
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
  Serial.println("wait for 2 sec...");
  delay(2000);
  bool wifiReset = digitalRead(this->wifi_reset_button) == PRESSED;
  if (wifiReset)
  {
    Serial.println("reset pressed...");
    this->flashLedIn();
    this->flashLedOut();

    wifiReset = digitalRead(this->wifi_reset_button) == PRESSED;
    if (wifiReset)
    {
      // drawText("Find HomeDashboardDS18B20AP wifi", "type 192.168.4.1 to your browser", "to configure");
      this->flashLedIn();
      this->flashLedOut();

      Serial.println("reset requested...");
      SPIFFS.format();
      this->wifiManager.resetSettings();
    }
  }
  analogWrite(this->status_led, NETWORK_STATUS_NOT_CONNECTED);
  // drawText("Connecting to wifi", WiFi.SSID().c_str(), "");

  if (!this->wifiManager.autoConnect(this->device_type, "password"))
  {
    Serial.println("failed to connect and hit timeout, you should reset to reconfigure");
  }
  else
  {
    // drawText("connected to wifi", WiFi.SSID().c_str(), IpAddress2String(WiFi.localIP()).c_str());
    Serial.println("connected...");
    Serial.println("local ip");
    Serial.println(WiFi.localIP());
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
    Serial.println("Wifi connection lost...");
    // drawText("Connection lost", WiFi.SSID().c_str(), "");
    if (WiFi.reconnect())
    {
      // drawText("Connected to wifi", WiFi.SSID().c_str(), this->IpAddress2String(WiFi.localIP()).c_str()); //TODO: print temperature
      analogWrite(this->status_led, NETWORK_STATUS_ONLY_WIFI);
      Serial.print("successfully reconnected, local ip:");
      Serial.println(WiFi.localIP());
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
  json["ip"] = this->IpAddress2String(WiFi.localIP()).c_str();
  json["rssi"] = WiFi.RSSI();
  
  char jsonChar[200];
  json.printTo((char *)jsonChar, json.measureLength() + 1);

  Serial.print("Send reply to discover device, result: ");
  Serial.println(client.publish(MQTT_TOPIC_DISCOVERY_FROM_DEVICE, jsonChar, false));

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

    Serial.print("publish state to topic:");
    Serial.print(this->topicDeviceState);
    Serial.print(", length: ");
    Serial.println(json.measureLength());

    this->jsonBuffer->clear();
  }
}

void HomeDashboardMqtt::connectToMqtt()
{
  Serial.print("trying to connect to MQTT server (");
  Serial.print(this->device_name);

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
    Serial.print(", user: ");
    Serial.print(this->mqtt_user);
    connectResult = this->client.connect(this->device_name, this->mqtt_user, this->mqtt_password, this->topicDeviceState, 1, true, jsonChar);
  }
  else
  {
    connectResult = this->client.connect(this->device_name, this->topicDeviceState, 1, true, jsonChar);
  }
  Serial.println(")");
  this->jsonBuffer->clear();

  this->lastConnectRetry = millis();

  if (connectResult)
  {
    Serial.println("Connected to MQTT");
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
      Serial.print("Failed to connect to MQTT server: ");
      Serial.print(mqtt_server);
      Serial.print(", port: ");
      Serial.println(atoi(mqtt_port));
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
  Serial.println("mounting FS...");
  if (SPIFFS.begin())
  {
    //SPIFFS.format();
    //this->wifiManager.resetSettings();

    Serial.println("file system mounted...");
    if (SPIFFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file...");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        JsonObject &json = this->jsonBuffer->parseObject(buf.get());

        json.printTo(Serial);

        if (json.success())
        {
          Serial.println("\nparsed json");

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
          Serial.println("failed to load json config");
        }
        configFile.close();
        this->jsonBuffer->clear();
      }
    }
    else
    {
      Serial.println("No config file present");
    }
  }
  else
  {
    Serial.println("failed to mount FS");
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
  Serial.println("Calling save settings callback");
  this->saveSettingsCallBack(custom);

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    Serial.println("failed to open config file for writing");
  }
  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  this->jsonBuffer->clear();
}

String HomeDashboardMqtt::IpAddress2String(const IPAddress &ipAddress)
{
  return String(ipAddress[0]) + String(".") +
         String(ipAddress[1]) + String(".") +
         String(ipAddress[2]) + String(".") +
         String(ipAddress[3]);
}