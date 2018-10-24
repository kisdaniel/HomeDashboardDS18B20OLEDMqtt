/* GarageDoor-Opener.h */ 
#ifndef GARAGEDOOR_OPENER_H
#define GARAGEDOOR_OPENER_H

#define SERIAL_BAUD 115200

// Pin layout

#define STATUS_LED          D6
#define WIFI_RESET_BUTTON   D8
#define ONE_WIRE_BUS        D3  // one wire

// D3 is reserved during programming



#define PRESSED             LOW     // LOW if you use built in pull up resistor
#define UNPRESSED           HIGH    // HIGH if you use built in pull up resistor
#define INPUT_PINMODE       INPUT_PULLUP    // built in pull up resistor


#define NETWORK_STATUS_NOT_CONNECTED        0
#define NETWORK_STATUS_ONLY_WIFI            256
#define NETWORK_STATUS_CONNECTING_TO_MQTT   512
#define NETWORK_STATUS_CONNECTED            1023

#define MQTT_TOPIC_REGISTRATION    "/homedashboard/device/registration"

#include <Arduino.h>

#endif
