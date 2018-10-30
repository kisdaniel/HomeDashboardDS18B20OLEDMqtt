/* GarageDoor-Opener.h */ 
#ifndef GARAGEDOOR_OPENER_H
#define GARAGEDOOR_OPENER_H

#define SERIAL_BAUD 115200

// Pin layout

// #define STATUS_LED          D6
#define STATUS_LED          D0
// #define WIFI_RESET_BUTTON   D8
#define WIFI_RESET_BUTTON   D6
#define ONE_WIRE_BUS        D3  // one wire

// D3 is reserved during programming






#define MQTT_TOPIC_REGISTRATION    "/homedashboard/device/registration"

#include <Arduino.h>


#endif