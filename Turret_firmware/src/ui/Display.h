#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

#include "logic/MenuLogic.h"

// OLED 128x64 on the Qwiic port J11 (improvement M1), shared I2C bus on
// IO35 / IO36. Controllers (OledType setting, it cannot be detected):
//   0 = SSD1306 (0.96"), 1 = SSD1309 (1.54"), 2 = SH1106 (1.3").
// The I2C address is detected: 0x3C, otherwise 0x3D.
// The screen is drawn from six lines of text (logic::MenuScreen), 21 columns.
class Display {
public:
  // After I2cBus::Begin(). Returns true if a screen answered.
  bool Begin(int32_t oledType);
  bool IsPresent() const { return present; }
  uint8_t GetAddress() const { return address; }
  // Sends the six lines; the highlighted one and the title are inverted.
  void Show(const logic::MenuScreen &screen);

private:
  void DrawWings(const logic::MenuScreen &screen);

  U8G2 *u8g2 = nullptr;
  bool present = false;
  uint8_t address = 0;
};
