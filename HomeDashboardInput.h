
#ifndef HomeDashboardInput_h
#define HomeDashboardInput_h

#include <FS.h>
#include "Arduino.h"
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

// Set the number of relays 
#define INPUT_COUNT 1

// define relay ports
#define INPUTS { D8 }

#define INPUT_ON  LOW
#define INPUT_OFF HIGH

void initInputs();
void loopInputs();
void loadInputSettings(JsonObject &jsonObject);
void saveInputSettings(JsonObject &jsonObject);
boolean onInputCommand(JsonObject &jsonObject);
void currentInputState(JsonObject &jsonObject);

#endif
