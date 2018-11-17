#ifndef HomeDashboardRelay_h
#define HomeDashboardRelay_h

#include <FS.h>
#include "Arduino.h"
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

// Set the number of relays 
#define RELAY_COUNT 1

// define relay ports
#define RELAYS { D0 }

#define RELAY_ON  false
#define RELAY_OFF true

void initRelays();
void loadRelaySettings(JsonObject &jsonObject);
void saveRelaySettings(JsonObject &jsonObject);
boolean onRelayCommand(JsonObject &jsonObject);
void currentRelayState(JsonObject &jsonObject);

#endif