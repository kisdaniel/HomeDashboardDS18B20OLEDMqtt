# HomeDashboard DS18B20 sensor reader

Home Dashboard BME280 Arduino project for NodeMCU (ESP8266) / WIFI Kit 8 (128x32 OLED)

You can comment  #define OLED  in HomeDashboardDS18B20OLEDMqtt.h, if you don't use WIFI Kit 8.

This program connect to an MQTT server and receives commands from mqtt and publishes status of the sensors to mqtt.

# Configuration

## Wifi, mqtt

Wifi and mqtt server is configured in an embedded captive portal, find a DS18B20 access point and connect to the network and use "password" for password.

You can setup wifi and mqtt server host and port.

If you want to reconfigure your nodemcu you have to follow the following instcructions:
- press reset button (wifi reset button must not be pressed)
- press wifi reset button within 2 sec after reset
- find DS18B20 access point and connect to it, wifi password is "password" 
- type http://192.168.4.1 to your browser

## Default Pin layout

Pin layout definied in HomeDashboardDS18B20OLEDMqtt.h 

* D8 - STATUS_LED 
* D6 - WIFI_RESET_BUTTON
* D7 - ONE_WIRE_BUS
* D3, (D0 - not documented, but not works) SCL, SDA is reserved in wifi kit 8 for OLED display when you #define OLED

https://www.homedashboard.hu/

