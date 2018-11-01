#include <FS.h>
#include "Arduino.h"
#include <U8g2lib.h>
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include "HomeDashboardMqtt.h"
#include "HomeDashboardOLED.h"

#define OLED_LINE_LENGTH 25

//U8g2 Contructor
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/16, /* clock=*/5, /* data=*/4);
// Alternative board version. Uncomment if above doesn't work.
// U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ 4, /* clock=*/ 14, /* data=*/ 2);

// u8g2_uint_t oledWidth;

void (*statusDrawCallback)(U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2);

char logBuffer[4][OLED_LINE_LENGTH] = {"", "", "", ""};
int logBufferIndex = 0;
int oledCurrentMode = OLED_MODE_LOG;
long prevRedraw = 0;

void oledDisplayInit()
{
    u8g2.begin();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_mn);
    u8g2.setFontMode(0);
    u8g2.setCursor(0, 8);
    u8g2.print("Initializing...");
}

void oledDisplayRedrawTypeLog()
{
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setFontMode(1); // enable transparent mode, which is faster

    u8g2.setCursor(0, 8);
    u8g2.print(logBuffer[0]);
    u8g2.setCursor(0, 16);
    u8g2.print(logBuffer[1]);
    u8g2.setCursor(0, 24);
    u8g2.print(logBuffer[2]);
    u8g2.setCursor(0, 32);
    u8g2.print(logBuffer[3]);

    u8g2.sendBuffer();
}

void oledDisplayRedrawNetworkState()
{
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
    u8g2.setFontMode(1); // enable transparent mode, which is faster
    if (WiFi.status() == WL_CONNECTED)
    {
        u8g2.drawGlyph(0, 8, 0x50); // WIFI
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(16, 8, WiFi.SSID().c_str());
    }
    else
    {
        u8g2.drawGlyph(0, 8, 0x47); // ERROR
        u8g2.setFont(u8g2_font_6x10_tf);
        String wifiStatus = wifiStatusAsText();
        u8g2.drawStr(16, 8, wifiStatus.c_str());
    }

    u8g2.setCursor(0, 18);
    u8g2.print("IP:");
    String ip = ipAddress2String(WiFi.localIP());
    u8g2.print(ip.c_str());

    u8g2.setCursor(0, 28);
    char buffer[10];
    itoa(getRSSIasQuality(WiFi.RSSI()), buffer, 9);
    u8g2.print("Signal: ");
    u8g2.print(buffer);
    u8g2.print("%");
    
    u8g2.sendBuffer();
}

void oledDisplayRedraw()
{
    if (oledCurrentMode == OLED_MODE_LOG)
    {
        oledDisplayRedrawTypeLog();
    }
    else if (oledCurrentMode == OLED_MODE_NETWORK_STATE)
    {
        oledDisplayRedrawNetworkState();
    }
    else
    {
        statusDrawCallback(u8g2);
    }
}

void oledDisplayChangeMode()
{
    if (oledCurrentMode == OLED_MODE_LOG)
    {
        oledDisplayChangeMode(OLED_MODE_STATE);
    }
    else if (oledCurrentMode == OLED_MODE_STATE)
    {
        oledDisplayChangeMode(OLED_MODE_NETWORK_STATE);
    }
    else
    {
        oledDisplayChangeMode(OLED_MODE_LOG);
    }
}

void oledDisplayChangeMode(int requiredMode)
{
    oledCurrentMode = requiredMode;
    oledDisplayRedraw();
}

inline void oledAddLogEntryToBuffer(const char message[OLED_LINE_LENGTH])
{
    if (logBufferIndex > 3)
    {
        strcpy(logBuffer[0], logBuffer[1]);
        strcpy(logBuffer[1], logBuffer[2]);
        strcpy(logBuffer[2], logBuffer[3]);
        strcpy(logBuffer[3], message);
    }
    else
    {
        strcpy(logBuffer[logBufferIndex], message);
        logBufferIndex++;
    }
}

inline void oladAddLogEntryToBufferWithTruncate(const char *message)
{
    char msg[OLED_LINE_LENGTH];
    int length = strlen(message) - 2;
    //strncpy(msg, message+2, length>OLED_LINE_LENGTH ? OLED_LINE_LENGTH : length);
    strlcpy(msg, message + 2, OLED_LINE_LENGTH);
    oledAddLogEntryToBuffer(msg);
}

void oledDisplayLogEntry(const char *message)
{
    oladAddLogEntryToBufferWithTruncate(message);
    oledDisplayRedrawTypeLog();
}

void oledDisplayStatusDrawCallback(void (*callback)(U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2))
{
    statusDrawCallback = callback;
}

void oledDisplayLoop()
{
    long now = millis();
    if (now > prevRedraw + 1000)
    {
        oledDisplayRedraw();
        prevRedraw = now;
    }
}
