#include <FS.h>

#include "HomeDashboardDS18B20OLEDMqtt.h"

#ifdef OLED
#include "HomeDashboardOLED.h"
#endif

#include "HomeDashboardMqtt.h"
#include "HomeDashboardRelay.h"
#include "HomeDashboardInput.h"

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

void dummyLogEntryHandler(const char *message);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

int sensorCount = 0;

float temperature;
char str_temp[5];
char buffer[15];

boolean wifiResetPreviousState = false;

void currentState(JsonObject &jsonObject);
void loadSettings(JsonObject &jsonObject);
void saveSettings(JsonObject &jsonObject);
void onCommand(JsonObject &jsonObject);

long sensorLastRead = 0;
long sensorAutoReadInterval = DEFAULT_SENSOR_READ_INTERVAL;
boolean sensorAutoReadMqttPublish = true;

HomeDashboardMqtt homeDashboard(
    (char *)"DS18B20", // type
    1,                 // version
    STATUS_LED,
    WIFI_RESET_BUTTON,
    &currentState,
    &loadSettings,
    &saveSettings,
    &onCommand,
#ifdef OLED
    &oledDisplayLogEntry
#else
    &dummyLogEntryHandler
#endif
);

void setup()
{
  delay(100);

  Serial.begin(115200);

  // SPIFFS.format();
  //    this->wifiManager.resetSettings();

#ifdef OLED
  oledDisplayStatusDrawCallback(&oledDrawStatus);
  oledDisplayInit();
#endif

  delay(100);

  homeDashboardMqtt = &homeDashboard;
  initPins();
  homeDashboardMqtt->init();
  initRelays();
  initInputs();
}

void loop()
{
  homeDashboardMqtt->loop();
  wifiResetButtonLoop();
  loopInputs();
#ifdef OLED
  oledDisplayLoop();
#endif
  sensorAutoReadLoop();
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
  currentRelayState(jsonObject);
  currentInputState(jsonObject);
#ifdef OLED
  oledDisplayRedraw();
#endif
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
    else if (strcmp(command, "reset") == 0)
    {
      homeDashboardMqtt->resetSettings();
    }
    else if (strcmp(command, "showLogs") == 0)
    {
#ifdef OLED
      oledDisplayChangeMode(OLED_MODE_LOG);
#else
      homeDashboardMqtt->error("unsupported command: showLogs");
#endif
    }
    else if (strcmp(command, "showNetworkState") == 0)
    {
#ifdef OLED
      oledDisplayChangeMode(OLED_MODE_NETWORK_STATE);
#else
      homeDashboardMqtt->error("unsupported command: showNetworkState");
#endif
    }
    else if (strcmp(command, "showState") == 0)
    {
#ifdef OLED
      oledDisplayChangeMode(OLED_MODE_STATE);
#else
      homeDashboardMqtt->error("unsupported command: showState");
#endif
    }
    else if (!onRelayCommand(jsonObject) && !onInputCommand(jsonObject))
    {
      homeDashboardMqtt->error("unknown command!");
    }
  }
  else
  {
    Serial.print("Invalid command received, missing \"command\" element!");
  }
}

void loadSettings(JsonObject &jsonObject)
{
  if (jsonObject.containsKey("sensorAutoReadInterval"))
  {
    sensorAutoReadInterval = jsonObject.get<int>("sensorAutoReadInterval");
  }
  else
  {
    sensorAutoReadInterval = DEFAULT_SENSOR_READ_INTERVAL;
  }
  if (jsonObject.containsKey("sensorAutoReadMqttPublish"))
  {
    sensorAutoReadMqttPublish = jsonObject.get<boolean>("sensorAutoReadMqttPublish");
  }
  else
  {
    sensorAutoReadMqttPublish = true;
  }
  loadRelaySettings(jsonObject);
  loadInputSettings(jsonObject);
}

void saveSettings(JsonObject &jsonObject)
{
  jsonObject["sensorAutoReadInterval"] = sensorAutoReadInterval;
  jsonObject["sensorAutoReadMqttPublish"] = sensorAutoReadMqttPublish;
  saveRelaySettings(jsonObject);
  saveInputSettings(jsonObject);
}

void initPins()
{
  Serial.println(F("try DS18B20"));
  sensors.begin();
  sensorCount = sensors.getDS18Count();
  Serial.print("Number of sensors: ");
  Serial.println(sensorCount);
}

#ifdef OLED
void oledDrawStatus(U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
  u8g2.setFontMode(1); // enable transparent mode, which is faster
  if (WiFi.status() == WL_CONNECTED)
  {
    u8g2.drawGlyph(0, 31, 0x50); // WIFI
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(16, 31, WiFi.SSID().c_str());
  }
  else
  {
    u8g2.drawGlyph(0, 31, 0x47); // ERROR
    u8g2.setFont(u8g2_font_6x10_tf);
    String wifiStatus = wifiStatusAsText();
    u8g2.drawStr(16, 31, wifiStatus.c_str());
  }

  if (sensorCount == 1)
  {
    u8g2.setFont(u8g2_font_ncenB18_tr);
    u8g2.setFontMode(1);
    temperature = sensors.getTempCByIndex(0);
    dtostrf(temperature, 4, 1, str_temp);
    sprintf(buffer, "%s C", str_temp);
    u8g2.drawStr(0, 21, buffer);
  }
  else if (sensorCount > 1)
  {
    u8g2.setFont(u8g2_font_courR10_tr);
    u8g2.setFontMode(1);

    temperature = sensors.getTempCByIndex(0);
    dtostrf(temperature, 4, 1, str_temp);
    sprintf(buffer, "%s C (1.)", str_temp);
    u8g2.drawStr(0, 11, buffer);

    temperature = sensors.getTempCByIndex(1);
    dtostrf(temperature, 4, 1, str_temp);
    sprintf(buffer, "%s C (2.)", str_temp);
    u8g2.drawStr(0, 22, buffer);
  }
  u8g2.sendBuffer();
}
#endif

void wifiResetButtonLoop()
{
  bool wifiReset = digitalRead(WIFI_RESET_BUTTON) == PRESSED;
  if (wifiReset != wifiResetPreviousState)
  {
    if (!wifiReset)
    {
      homeDashboardMqtt->info("reset pressed");
#ifdef OLED
      oledDisplayChangeMode();
      delay(50);
#endif
    }
    wifiResetPreviousState = wifiReset;
  }
}

void sensorAutoReadLoop()
{
  long now = millis();
  if ((sensorAutoReadInterval > 0) && (now >= sensorLastRead + sensorAutoReadInterval))
  {
    if (sensorAutoReadMqttPublish)
    {
      homeDashboardMqtt->publishState();
    }
    else
    {
#ifdef OLED
      sensors.requestTemperatures();
      oledDisplayRedraw();
#endif
    }
    sensorLastRead = millis();
  }
}

void dummyLogEntryHandler(const char *message)
{
}
