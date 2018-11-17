/* HomeDashboardDS18B20OLEDMqtt.h */ 
#ifndef HOMEDASHBOARD_DS18B20_OLED_H
#define HOMEDASHBOARD_DS18B20_OLED_H

#define SERIAL_BAUD 115200

// Pin layout

#define STATUS_LED          D8  

#define WIFI_RESET_BUTTON   D6  // input button
#define ONE_WIRE_BUS        D7  // one wire bus for DS18B20

// comment below line if you not have OLED
// #define OLED 

// note: D2 is reserved during programming on NodeMCU
// note: D2, (D0 - not documented, but not works) SCL, SDA is reserved in wifi kit 8 for OLED display 


#define DEFAULT_SENSOR_READ_INTERVAL 60000

#include <Arduino.h>

#endif