#include "Display.h"

#include "board/I2cBus.h"
#include "board/Log.h"
#include "pins.h"

namespace {
const uint8_t LINE_HEIGHT = 10; // 6 lines of the 6x10 font on 64 pixels
const uint8_t BASELINE = 8;
} // namespace

bool Display::Begin(int32_t oledType) {
  address = I2cBus::Probe(0x3C) ? 0x3C : (I2cBus::Probe(0x3D) ? 0x3D : 0);
  present = address != 0;
  if (!present) {
    Log.println("Display: no OLED on J11 (0x3C / 0x3D)");
    return false;
  }
  // Clock and data pins given explicitly: U8g2 then calls Wire.begin(35, 36),
  // which leaves the bus started by I2cBus as it is.
  switch (oledType) {
  case 1:
    u8g2 = new U8G2_SSD1309_128X64_NONAME0_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
    break;
  case 2:
    u8g2 = new U8G2_SH1106_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
    break;
  default:
    u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE, PIN_SCL, PIN_SDA);
    break;
  }
  u8g2->setI2CAddress(address << 1);
  u8g2->setBusClock(400000);
  u8g2->begin();
  u8g2->setFont(u8g2_font_6x10_tf); // Latin-1: accented letters for French
  u8g2->setFontMode(1);             // transparent: needed to write on an inverted bar
  Log.printf("Display: OLED at 0x%02X, type %ld (%s)\n", address, (long)oledType,
             oledType == 1 ? "SSD1309" : oledType == 2 ? "SH1106" : "SSD1306");
  return true;
}

// Front view of the turret between the title and the bottom line: oval body
// with its eye, and the two side panels sliding outwards as the wings open
// (0..100 % = 0..24 pixels); the gun barrels appear in the gap.
void Display::DrawWings(const logic::MenuScreen &screen) {
  const int centerX = 64, centerY = 31, bodyRx = 11, bodyRy = 17;
  const int wingWidth = 9, wingTop = 15, wingHeight = 33, travel = 24;

  u8g2->setDrawColor(1);
  u8g2->drawBox(0, 0, 128, LINE_HEIGHT);
  u8g2->setDrawColor(0);
  u8g2->drawUTF8(1, BASELINE, screen.line[0]);
  u8g2->setDrawColor(1);
  u8g2->drawUTF8(1, 5 * LINE_HEIGHT + 2 + BASELINE, screen.line[5]);

  u8g2->drawEllipse(centerX, centerY, bodyRx, bodyRy);
  u8g2->drawDisc(centerX, centerY, 3);

  int left = screen.leftPercent * travel / 100;
  int right = screen.rightPercent * travel / 100;
  int leftX = centerX - bodyRx - 1 - wingWidth - left;
  int rightX = centerX + bodyRx + 2 + right;
  u8g2->drawRBox(leftX, wingTop, wingWidth, wingHeight, 2);
  u8g2->drawRBox(rightX, wingTop, wingWidth, wingHeight, 2);
  // Two barrels per side, once the gap is wide enough to see them.
  if (left >= 4) {
    u8g2->drawHLine(leftX + wingWidth, centerY - 5, left);
    u8g2->drawHLine(leftX + wingWidth, centerY + 5, left);
  }
  if (right >= 4) {
    u8g2->drawHLine(centerX + bodyRx + 2, centerY - 5, right);
    u8g2->drawHLine(centerX + bodyRx + 2, centerY + 5, right);
  }
}

void Display::Show(const logic::MenuScreen &screen) {
  if (!present) {
    return;
  }
  u8g2->clearBuffer();
  if (screen.wingAnimation) {
    DrawWings(screen);
    u8g2->sendBuffer();
    return;
  }
  for (uint8_t i = 0; i < logic::SCREEN_LINES; i++) {
    uint8_t top = i * LINE_HEIGHT + (i > 0 ? 2 : 0); // small gap under the title
    bool inverted = i == 0 || i == screen.highlight;
    u8g2->setDrawColor(1);
    if (inverted) {
      u8g2->drawBox(0, top, 128, LINE_HEIGHT);
      u8g2->setDrawColor(0);
    }
    u8g2->drawUTF8(1, top + BASELINE, screen.line[i]);
  }
  u8g2->setDrawColor(1);
  u8g2->sendBuffer();
}
