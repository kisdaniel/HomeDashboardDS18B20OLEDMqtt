#ifndef HomeDashboardOLED_h
#define HomeDashboardOLED_h
#include <FS.h>
#include "Arduino.h"
#include <U8g2lib.h>


#define OLED_MODE_LOG 0
#define OLED_MODE_STATE 1
#define OLED_MODE_NETWORK_STATE 2

extern void oledDisplayInit();
extern void oledDisplayRedraw();
extern void oledDisplayLogEntry(const char *message);
extern void oledDisplayChangeMode(int requiredMode);
extern void oledDisplayChangeMode();
extern void oledDisplayStatusDrawCallback(void (*callback)(U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2));
extern void oledDisplayLoop();

#endif
