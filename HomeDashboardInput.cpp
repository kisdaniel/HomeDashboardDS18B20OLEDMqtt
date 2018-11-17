
#include <FS.h>
#include "Arduino.h"
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include "HomeDashboardMqtt.h"
#include "HomeDashboardRelay.h"
#include "HomeDashboardInput.h"

int inputs[INPUT_COUNT] = INPUTS;
boolean lastInputState[INPUT_COUNT];
int lastInputStateAt[INPUT_COUNT];
int inputMinTime = 200; // minimum time between two press, prevents too fast pressings

void initInputs()
{
    long now = millis();
    for (int i = 0; i < INPUT_COUNT; i++)
    {
        pinMode(inputs[i], INPUT_PULLUP);
        lastInputState[i] = (digitalRead(inputs[i]) == INPUT_ON);
        lastInputStateAt[i] = now;
    }
}

bool readInputStates()
{
    char buffer[50];
    bool state;
    bool changed = false;
    long now = millis();
    for (int i = 0; i < INPUT_COUNT; i++)
    {
        state = digitalRead(inputs[i]) == INPUT_ON;
        if ((lastInputState[i] != state) && (now - lastInputStateAt[i] > inputMinTime))
        {
            lastInputState[i] = state;
            lastInputStateAt[i] = now;
            changed = true;
            if (state)
            {
                sprintf(buffer, "%d input pressed");
            }
            else
            {
                sprintf(buffer, "%d input released");
            }
            homeDashboardMqtt->info(buffer);
        }
    }
    return changed;
}

void loopInputs()
{
    if (readInputStates())
    {
        homeDashboardMqtt->publishState();
    }
}

void loadInputSettings(JsonObject &jsonObject)
{
    if (jsonObject.containsKey("inputMinTime"))
    {
        inputMinTime = jsonObject.get<int>("inputMinTime");
    }
}

void saveInputSettings(JsonObject &jsonObject)
{
    jsonObject["inputMinTime"] = inputMinTime;
}

boolean onInputCommand(JsonObject &jsonObject)
{
    if (jsonObject.containsKey("command") &&
        jsonObject.containsKey("input") &&
        jsonObject.containsKey("relay"))
    {
        char command[80];
        strcpy(command, jsonObject["command"]);
        int input = jsonObject.get<int>("input");
        int output = jsonObject.get<int>("relay");
        if (strcmp(command, "bind") == 0)
        {
            if (input < INPUT_COUNT && output < RELAY_COUNT)
            {
                digitalRead(inputs[input]);
                lastInputState[input] = true;
                homeDashboardMqtt->info("Relay turnen on!");
                homeDashboardMqtt->publishState();
                return true;
            }
            else
            {
                homeDashboardMqtt->error("Cannot turn on, relay not exists!");
            }
        }
    }
    return false;
}

void currentInputState(JsonObject &jsonObject)
{
    JsonArray &inputList = jsonObject.createNestedArray("inputs");
    for (int i = 0; i < INPUT_COUNT; i++)
    {
        inputList.add(lastInputState[i]);
    }
}
