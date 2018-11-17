#include <FS.h>
#include "Arduino.h"
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include "HomeDashboardMqtt.h"
#include "HomeDashboardRelay.h"

int relays[RELAY_COUNT] = RELAYS;
boolean lastState[RELAY_COUNT];
void initRelays()
{
    for (int i = 0; i < RELAY_COUNT; i++)
    {
        pinMode(relays[i], OUTPUT);
        lastState[i] = false;
    }
}

void loadRelaySettings(JsonObject &jsonObject)
{
    
}

void saveRelaySettings(JsonObject &jsonObject)
{
}

boolean onRelayCommand(JsonObject &jsonObject)
{
    if (jsonObject.containsKey("command") && jsonObject.containsKey("relay"))
    {
        char command[80];
        strcpy(command, jsonObject["command"]);
        int relay = jsonObject.get<int>("relay");
        if (strcmp(command, "turnon") == 0)
        {
            if (relay < RELAY_COUNT)
            {
                digitalWrite(relays[relay], RELAY_ON);
                lastState[relay] = true;
                homeDashboardMqtt->info("Relay turnen on!");
                homeDashboardMqtt->publishState();
                return true;
            }
            else
            {
                homeDashboardMqtt->error("Cannot turn on, relay not exists!");
            }
        }
        else if (strcmp(command, "turnoff") == 0)
        {
            if (relay < RELAY_COUNT)
            {
                digitalWrite(relays[relay], RELAY_OFF);
                lastState[relay] = false;
                homeDashboardMqtt->info("Relay turned off!");
                homeDashboardMqtt->publishState();
                return true;
            }
            else
            {
                homeDashboardMqtt->error("Cannot turn off, relay not exists!");
            }
        }
    }
    return false;
}

void currentRelayState(JsonObject &jsonObject)
{
    JsonArray &relayList = jsonObject.createNestedArray("relays");
    for (int i = 0; i < RELAY_COUNT; i++)
    {
        relayList.add(lastState[i]);
    }
}
