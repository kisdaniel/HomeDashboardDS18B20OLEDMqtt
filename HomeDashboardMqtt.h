// must #include <FS.h> to your main file

#ifndef HomeDashboardMqtt_h
#define HomeDashboardMqtt_h
#include <FS.h>
#include "Arduino.h"

#include <PubSubClient.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

#define PRESSED LOW                // LOW if you use built in pull up resistor
#define UNPRESSED HIGH             // HIGH if you use built in pull up resistor
#define INPUT_PINMODE INPUT_PULLUP // built in pull up resistor

#define MQTT_TOPIC_DISCOVERY_FROM_DEVICE "homedashboard/discovery/fromDevice"
#define MQTT_TOPIC_DISCOVERY_TO_DEVICE "homedashboard/discovery/toDevice"

#define NETWORK_STATUS_NOT_CONNECTED 0
#define NETWORK_STATUS_ONLY_WIFI 256
#define NETWORK_STATUS_CONNECTING_TO_MQTT 512
#define NETWORK_STATUS_CONNECTED 1023

class HomeDashboardMqtt
{
public:
  HomeDashboardMqtt(char *device_type,
                    int version,
                    int status_led,
                    int wifi_reset_button,
                    void (*currentState)(JsonObject &json),
                    void (*loadSettings)(JsonObject &json),
                    void (*saveSettings)(JsonObject &json),
                    void (*onCommand)(JsonObject &json));
  void init();
  void loop();
  void loadConfig();
  void saveConfigCallback();
  void connectToMqtt();
  void sendDeviceDiscoveryFromDevice();
  void mqttCallback(char *topic, byte *payload, unsigned int length);
  void connectToMqttIfNotConnected();
  void onDiscoveryMessage(byte *payload, unsigned int length);
  void onCommand(byte *payload, unsigned int length);
  void onHeartbeat(byte *payload, unsigned int length);
  void onSettingsChange(byte *payload, unsigned int length);
  void publishCurrentSettings();
  void publishState();
  void flashLedIn();
  void flashLedOut();
  String IpAddress2String(const IPAddress &ipAddress);
  void saveConfig();

  void initTopic();

  char mqtt_server[40];
  char mqtt_port[6];
  char mqtt_user[100];
  char mqtt_password[100];
  char device_name[34];

  DynamicJsonBuffer *jsonBuffer;

  WiFiManager wifiManager;
  WiFiClient espClient;
  PubSubClient client;

  WiFiManagerParameter custom_mqtt_server;
  WiFiManagerParameter custom_mqtt_port;
  WiFiManagerParameter custom_mqtt_user;
  WiFiManagerParameter custom_mqtt_password;
  WiFiManagerParameter custom_device_name;

private:
  char *device_type;
  int version;
  int status_led;
  int wifi_reset_button;
  boolean mqttFailedBefore;
  long lastConnectRetry;

  char topicDeviceCommand[69];
  char topicDeviceState[69];
  char topicDeviceHeartbeat[69];
  char topicDeviceChangeSettings[69];
  char topicDeviceCurrentSettings[69];

  void (*currentStateCallBack)(JsonObject &json);
  void (*loadSettingsCallBack)(JsonObject &json);
  void (*saveSettingsCallBack)(JsonObject &json);
  void (*onCommandCallBack)(JsonObject &json);
};

extern HomeDashboardMqtt *homeDashboardMqtt;

#endif