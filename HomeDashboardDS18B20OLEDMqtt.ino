#include <FS.h>

#include "HomeDashboardDS18B20OLEDMqtt.h"

#include "HomeDashboardMqtt.h"

#include <U8g2lib.h>

#include <OneWire.h>
#include <DallasTemperature.h>

// the following override is not working due to this issue: https://github.com/knolleary/pubsubclient/pull/282
// #define MQTT_MAX_PACKET_SIZE    254
// you need to change PubSubClient.h MQTT_MAX_PACKET_SIZE to 254
#include <SPI.h>
#include <Wire.h>

#include <Adafruit_SSD1306.h>

#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

#include <PubSubClient.h>

#include <OneWire.h>
#include <DallasTemperature.h>

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

int sensorCount = 0;

float temperature;

void currentState(JsonObject &jsonObject);
void loadSettings(JsonObject &jsonObject);
void saveSettings(JsonObject &jsonObject);
void onCommand(JsonObject &jsonObject);

HomeDashboardMqtt homeDashboard(
    (char *)"DS18B20", // type
    1,                 // version
    STATUS_LED,
    WIFI_RESET_BUTTON,
    &currentState,
    &loadSettings,
    &saveSettings,
    &onCommand);

void setup()
{
  Serial.begin(115200);
  initSensors();
  homeDashboardMqtt = &homeDashboard;
  homeDashboardMqtt->init();
}

void loop()
{
  homeDashboardMqtt->loop();
}

void currentState(JsonObject &jsonObject)
{
  sensors.requestTemperatures();
  JsonArray &temperatures = jsonObject.createNestedArray("temperature");
  for (int i = 0; i < sensorCount; i++)
  {
    temperature = sensors.getTempCByIndex(i);
    if (isnan(temperature))
    {
      temperatures.add(NULL);
    }
    else
    {
      temperatures.add(temperature);
    }
  }

  jsonObject["rssi"] = WiFi.RSSI();
}

void onCommand(JsonObject &jsonObject)
{
  if (jsonObject.containsKey("command"))
  {
    char command[80];
    strcpy(command, jsonObject["command"]);

    if (strcmp(command, "read") == 0)
    {
      homeDashboardMqtt->publishState();
    }
    else if (strcmp(command, "readSettings") == 0)
    {
      homeDashboardMqtt->publishCurrentSettings();
    }
    else
    {
      Serial.println("unknown command!");
    }
  }
  else {
    Serial.print("Invalid command received, missing \"command\" element!");
  }
}

void loadSettings(JsonObject &jsonObject)
{
  Serial.println("load settings callback");
}

void saveSettings(JsonObject &jsonObject)
{
  Serial.println("saving settings callback");
}

void initSensors()
{
  Serial.println(F("try DS18B20"));
  sensors.begin();
  sensorCount = sensors.getDS18Count();
  Serial.print("Number of sensors: ");
  Serial.println(sensorCount);
}

//U8g2 Contructor
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, 16, 5, 4);

u8g2_uint_t width;

void initDisplay()
{
  u8g2.begin();
}

/*
//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "1883";
char mqtt_user[100];
char mqtt_password[100];
char device_name[34] = "DS18B20Sensor";
// char open_close_timeout[6] = "45000";

char inTopic[60];
char outTopic[60];

//flag for saving data
bool mqttFailedBefore = false;
bool prevWifiButtonPressed = false;

long lastConnectRetry = 0;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

int sensorCount = 0;

DynamicJsonBuffer jsonBuffer(512);

WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient client(espClient);

WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 100);
WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 100);
WiFiManagerParameter custom_device_name("deviceName", "Device name", device_name, 32);

float temperature;

char temperatureString[6];

//U8g2 Contructor
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, 16, 5, 4);

u8g2_uint_t width;

void initDisplay()
{
  u8g2.begin();
}

void drawWelcome()
{
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setFontMode(0); // enable transparent mode, which is faster

  u8g2.setCursor(1, 10);
  u8g2.print("Initializing device");

  u8g2.setCursor(1, 20);
  u8g2.print(device_name);

  u8g2.sendBuffer();
}

void drawText(const char *text, const char *text2, const char *text3)
{
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_t0_11_tf); // set the target font
  u8g2.setFontMode(0);              // enable transparent mode, which is faster

  u8g2.setCursor(1, 10);
  u8g2.print(text);
  
  u8g2.setCursor(1, 20);
  u8g2.print(text2);

  u8g2.setCursor(1, 30);
  u8g2.print(text3);

  u8g2.sendBuffer();
}

void drawStatus()
{
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x13_tf); // set the target font
  u8g2.setFontMode(0);
  char str_temp[6];
  char buffer[40];
  for (int i = 0; i < sensorCount; i++)
  {
    temperature = sensors.getTempCByIndex(i);
    dtostrf(temperature, 4, 1, str_temp);
    sprintf(buffer, "%s C (%d.)", str_temp, i + 1);
    u8g2.drawStr(0, 9 + i * 11, buffer);
  }
  u8g2.drawStr(0, 9 + sensorCount * 11, ("IP: " + IpAddress2String(WiFi.localIP())).c_str());

  u8g2.sendBuffer();
}

String IpAddress2String(const IPAddress &ipAddress)
{
  return String(ipAddress[0]) + String(".") +
         String(ipAddress[1]) + String(".") +
         String(ipAddress[2]) + String(".") +
         String(ipAddress[3]);
}

void checkButtonCommands()
{
  bool wifiReset = digitalRead(WIFI_RESET_BUTTON) == PRESSED;
  if (wifiReset && prevWifiButtonPressed == false)
  {
    publishState();
  }
  prevWifiButtonPressed = wifiReset;
}

//callback notifying us of the need to save config
void saveConfigCallback1()
{
  Serial.println("saving config");

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(device_name, custom_device_name.getValue());

  JsonObject &json = jsonBuffer.createObject();
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;
  json["mqtt_user"] = mqtt_user;
  json["mqtt_password"] = mqtt_password;

  json["device_name"] = device_name;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    Serial.println("failed to open config file for writing");
  }
  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  jsonBuffer.clear();
  //end save
}

void loadConfig()
{
  if (SPIFFS.begin())
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        JsonObject &json = jsonBuffer.parseObject(buf.get());

        json.printTo(Serial);

        if (json.success())
        {
          Serial.println("\nparsed json");

          strcpy(device_name, json["device_name"]);
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_password, json["mqtt_password"]);
        }
        else
        {
          Serial.println("failed to load json config");
        }
        configFile.close();
        jsonBuffer.clear();
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

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strcmp(topic, MQTT_TOPIC_REGISTRATION) == 0)
  {
    if (length == 2 && strncmp((char *)payload, "{}", length) == 0)
    {
      // resends a registration message
      deviceRegistration();
    }
    else
    {
      Serial.println("skip other device registration");
    }
  }
  else
  {
    JsonObject &inputObject = jsonBuffer.parseObject(payload);
    const char *command = inputObject["command"];
    if (strcmp(command, "read") == 0)
    {
      jsonBuffer.clear();
      publishState();
    }
    else if (strcmp(command, "heartbeat") == 0)
    {
      jsonBuffer.clear();
      publishState();
    }
    jsonBuffer.clear();
  }
}

void deviceRegistration()
{
  JsonObject &json = jsonBuffer.createObject();
  json["name"] = device_name;
  json["type"] = "DS18B20";

  char jsonChar[160];
  json.printTo((char *)jsonChar, json.measureLength() + 1);
  Serial.print("Register device (");
  Serial.print(MQTT_TOPIC_REGISTRATION);
  Serial.print("), result: ");
  Serial.println(client.publish(MQTT_TOPIC_REGISTRATION, jsonChar));
}

void reconnectToMqtt()
{
  Serial.print("trying to connect to MQTT server (");
  Serial.print(device_name);

  client.setCallback(mqttCallback);
  client.setServer(mqtt_server, atoi(mqtt_port));

  analogWrite(STATUS_LED, NETWORK_STATUS_CONNECTING_TO_MQTT);

  boolean connectResult = false;

  JsonObject &json = jsonBuffer.createObject();
  json["name"] = device_name;
  json["type"] = "DS18B20";
  json["offline"] = true;
  char jsonChar[200];
  json.printTo((char *)jsonChar, json.measureLength() + 1);

  if (strlen(mqtt_user) > 0)
  {
    Serial.print(", user: ");
    Serial.print(mqtt_user);
    connectResult = client.connect(device_name, mqtt_user, mqtt_password, MQTT_TOPIC_REGISTRATION, 1, true, jsonChar);
  }
  else
  {
    connectResult = client.connect(device_name, MQTT_TOPIC_REGISTRATION, 1, true, jsonChar);
  }
  Serial.println(")");
  jsonBuffer.clear();

  lastConnectRetry = millis();

  if (connectResult)
  {
    Serial.println("Connected to MQTT");
    mqttFailedBefore = false;

    deviceRegistration();
    client.subscribe(MQTT_TOPIC_REGISTRATION);
    
    strcpy(inTopic, "homedashboard/device");
    strcat(inTopic, device_name);
    strcpy(outTopic, inTopic);
    strcat(inTopic, "/in");
    strcat(outTopic, "/out");

    client.subscribe(inTopic);
    Serial.print("Subscribe to ");
    Serial.println(inTopic);
    publishState();
    analogWrite(STATUS_LED, NETWORK_STATUS_CONNECTED);
  }
  else
  {
    analogWrite(STATUS_LED, NETWORK_STATUS_ONLY_WIFI);
    if (!mqttFailedBefore)
    {
      Serial.print("Failed to connect to MQTT server: ");
      Serial.print(mqtt_server);
      Serial.print(", port: ");
      Serial.println(atoi(mqtt_port));
    }
    mqttFailedBefore = true;
  }
}

void connectToMqttIfNotConnected()
{
  if (!client.connected())
  {
    long now = millis();
    if (lastConnectRetry + 1000 < now)
    {
      reconnectToMqtt();
    }
  }
  else
  {
    client.loop();
  }
}

void publishState()
{
  if (client.connected())
  {
    sensors.requestTemperatures();
    JsonObject &json = jsonBuffer.createObject();
    JsonArray &temperatures = json.createNestedArray("temperature");
    for (int i = 0; i < sensorCount; i++)
    {
      temperature = sensors.getTempCByIndex(i);
      if (isnan(temperature))
      {
        temperatures.add(NULL);
      }
      else
      {
        temperatures.add(temperature);
      }
    }
    char jsonChar[200];
    json.printTo((char *)jsonChar, json.measureLength() + 1);
    int result = client.publish(outTopic, jsonChar, json.measureLength());

    Serial.print("publish state to topic:");
    Serial.print(outTopic);
    Serial.print(", length: ");
    Serial.println(json.measureLength());

    // Serial.print(", result: ");
    // Serial.println(result);

    // Serial.println(jsonChar);
    // Serial.println(openState);

    jsonBuffer.clear();

    drawStatus();
  }
}

void initPins()
{
  Serial.println("init pins");

  pinMode(STATUS_LED, OUTPUT);
  pinMode(WIFI_RESET_BUTTON, INPUT_PINMODE);

  Serial.println(F("try DS18B20"));
  sensors.begin();
  sensorCount = sensors.getDS18Count();
  Serial.print("Number of sensors: ");
  Serial.println(sensorCount);
}

void initWifi()
{
  analogWrite(STATUS_LED, NETWORK_STATUS_NOT_CONNECTED);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_device_name);

  WiFi.hostname(device_name);
  wifiManager.setConfigPortalTimeout(60 * 3);

  drawText("Initializing", "wait for 2 sec while you can reset", "");
  Serial.println("wait for 2 sec...");
  delay(2000);
  bool wifiReset = digitalRead(WIFI_RESET_BUTTON) == PRESSED;
  if (wifiReset)
  {
    Serial.println("reset pressed...");
    flashLedIn();
    flashLedOut();

    wifiReset = digitalRead(WIFI_RESET_BUTTON) == PRESSED;
    if (wifiReset)
    {
      drawText("Find HomeDashboardDS18B20AP wifi", "type 192.168.4.1 to your browser", "to configure");
      flashLedIn();
      flashLedOut();

      Serial.println("reset requested...");
      // SPIFFS.format();
      wifiManager.resetSettings();
    }
  }
  analogWrite(STATUS_LED, NETWORK_STATUS_NOT_CONNECTED);
  drawText("Connecting to wifi", WiFi.SSID().c_str(), "");
  if (!wifiManager.autoConnect("HomeDashboardDS18B20AP", "password"))
  {
    Serial.println("failed to connect and hit timeout, you should reset to reconfigure");
  }
  else
  {
    drawText("connected to wifi", WiFi.SSID().c_str(), IpAddress2String(WiFi.localIP()).c_str());
    Serial.println("connected...");
    Serial.println("local ip");
    Serial.println(WiFi.localIP());
  }
}

void printMqttErrorCode()
{
  int state = client.state();
  if (state == MQTT_CONNECTION_TIMEOUT)
  {
    Serial.println("MQTT_CONNECTION_TIMEOUT");
  }
  else if (state == MQTT_CONNECTION_LOST)
  {
    Serial.println("MQTT_CONNECTION_LOST");
  }
  else if (state == MQTT_CONNECT_FAILED)
  {
    Serial.println("MQTT_CONNECT_FAILED");
  }
  else if (state == MQTT_DISCONNECTED)
  {
    Serial.println("MQTT_DISCONNECTED");
  }
  else if (state == MQTT_CONNECTED)
  {
    Serial.println("MQTT_CONNECTED");
  }
  else if (state == MQTT_CONNECT_BAD_PROTOCOL)
  {
    Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
  }
  else if (state == MQTT_CONNECT_BAD_CLIENT_ID)
  {
    Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
  }
  else if (state == MQTT_CONNECT_UNAVAILABLE)
  {
    Serial.println("MQTT_CONNECT_UNAVAILABLE");
  }
  else if (state == MQTT_CONNECT_BAD_CREDENTIALS)
  {
    Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
  }
  else if (state == MQTT_CONNECT_UNAUTHORIZED)
  {
    Serial.println("MQTT_CONNECT_UNAUTHORIZED");
  }
}

void flashLedOut()
{
  for (int i = 1023; i > 0; i--)
  {
    analogWrite(STATUS_LED, i);
    delay(1);
  }
}
void flashLedIn()
{
  for (int i = 0; i < 1024; i++)
  {
    analogWrite(STATUS_LED, i);
    delay(1);
  }
}

void setup()
{
  Serial.begin(115200);

  Serial.println(MQTT_MAX_PACKET_SIZE);
  Serial.println("starting...");

  initDisplay();

  //clean FS, for testing
  //SPIFFS.format();
  //wifiManager.resetSettings();

  //read configuration from FS json
  

  loadConfig();

  drawWelcome();

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback1);

  // set static ip
  // wifiManager.setSTAStaticIPConfig(IPAddress(10, 0, 1, 99), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));

  //add all your parameters here
  initPins();
  initWifi();
}

void loop()
{
  if (!WiFi.isConnected())
  {
    analogWrite(STATUS_LED, NETWORK_STATUS_NOT_CONNECTED);
    Serial.println("Wifi connection lost...");
    drawText("Connection lost", WiFi.SSID().c_str(), ""); //TODO: print temperature
    if (WiFi.reconnect())
    {
      drawText("Connected to wifi", WiFi.SSID().c_str(), IpAddress2String(WiFi.localIP()).c_str()); //TODO: print temperature
      analogWrite(STATUS_LED, NETWORK_STATUS_ONLY_WIFI);
      Serial.print("successfully reconnected, local ip:");
      Serial.println(WiFi.localIP());
      connectToMqttIfNotConnected();
    }
  }
  else
  {
    connectToMqttIfNotConnected();
  }
  checkButtonCommands();
}
*/