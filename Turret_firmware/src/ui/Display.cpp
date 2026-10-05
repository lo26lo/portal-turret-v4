#include "Display.h"

#include "board/I2cBus.h"
#include "board/Log.h"
#include "logic/RadarLogic.h"
#include "pins.h"

namespace {
const uint8_t LINE_HEIGHT = 10; // 6 lines of the 6x10 font on 64 pixels
const uint8_t BASELINE = 8;
const uint32_t TASK_STACK_BYTES = 4096;
// Drawing area between the title and the bottom line.
const int AREA_TOP = 12;
const int AREA_BOTTOM = 51;
const int32_t RADAR_RANGE_MM = 6000; // full scale of the radar view (range of the LD2450)
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

  // Frames are sent by a low-priority task on the other core (loop() runs on
  // core 1), so that the 25 ms of a frame never delay the main loop.
  lock = xSemaphoreCreateMutex();
  if (lock == nullptr ||
      xTaskCreatePinnedToCore(TaskEntry, "oled", TASK_STACK_BYTES, this, 1, &task, 0) != pdPASS) {
    Log.println("Display: could not start the display task, screen disabled");
    task = nullptr;
    present = false;
    return false;
  }
  return true;
}

// loop() side: copy the frame and wake the display task. No bus access here.
void Display::Show(const logic::MenuScreen &screen) {
  if (!present || task == nullptr) {
    return;
  }
  xSemaphoreTake(lock, portMAX_DELAY);
  pending = screen;
  xSemaphoreGive(lock);
  xTaskNotifyGive(task);
}

void Display::Configure(bool flip, uint8_t contrast) {
  wantFlip = flip;
  wantContrast = contrast;
  if (task != nullptr) {
    xTaskNotifyGive(task);
  }
}

void Display::SetPower(bool on) {
  wantOn = on;
  if (task != nullptr) {
    xTaskNotifyGive(task);
  }
}

void Display::TaskEntry(void *self) { static_cast<Display *>(self)->Run(); }

// Display task: the only code that touches U8g2 after Begin().
void Display::Run() {
  // Static: ~480 bytes that do not need to live on the task stack.
  static logic::MenuScreen frame;
  for (;;) {
    // Several notifications while a frame was being sent collapse into one:
    // the next pass draws the latest frame only.
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    bool flip = wantFlip, on = wantOn;
    uint8_t contrast = wantContrast;
    if (!configured || flip != appliedFlip) {
      u8g2->setFlipMode(flip ? 1 : 0);
      appliedFlip = flip;
    }
    if (!configured || contrast != appliedContrast) {
      u8g2->setContrast(contrast);
      appliedContrast = contrast;
    }
    if (!configured || on != appliedOn) {
      u8g2->setPowerSave(on ? 0 : 1);
      appliedOn = on;
    }
    configured = true;
    if (!on) {
      continue; // asleep: nothing to send
    }

    xSemaphoreTake(lock, portMAX_DELAY);
    frame = pending;
    xSemaphoreGive(lock);
    Draw(frame);
  }
}

void Display::Draw(const logic::MenuScreen &screen) {
  u8g2->clearBuffer();
  u8g2->setDrawColor(1);
  switch (screen.graphic) {
  case logic::Graphic::Wings:
    DrawFrameLines(screen);
    DrawWings(screen);
    break;
  case logic::Graphic::Radar:
    DrawFrameLines(screen);
    DrawRadar(screen);
    break;
  case logic::Graphic::Graph:
    DrawFrameLines(screen);
    DrawGraph(screen);
    break;
  case logic::Graphic::Qr:
    DrawQr(screen);
    break;
  case logic::Graphic::Eye:
    DrawEye(screen);
    break;
  default:
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
    break;
  }
  u8g2->setDrawColor(1);
  u8g2->sendBuffer();
}

// Title bar (line 0) and bottom line (line 5) around a drawing.
void Display::DrawFrameLines(const logic::MenuScreen &screen) {
  u8g2->setDrawColor(1);
  u8g2->drawBox(0, 0, 128, LINE_HEIGHT);
  u8g2->setDrawColor(0);
  u8g2->drawUTF8(1, BASELINE, screen.line[0]);
  u8g2->setDrawColor(1);
  u8g2->drawUTF8(1, 5 * LINE_HEIGHT + 2 + BASELINE, screen.line[5]);
}

// Front view of the turret: oval body with its eye, and the two side panels
// sliding outwards as the wings open (0..100 % = 0..24 pixels); the gun
// barrels appear in the gap.
void Display::DrawWings(const logic::MenuScreen &screen) {
  const int centerX = 64, centerY = 31, bodyRx = 11, bodyRy = 17;
  const int wingWidth = 9, wingTop = 15, wingHeight = 33, travel = 24;

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

// Top view: the radar at the bottom centre, full scale 6 m. The arc and the
// two lines are the detection zone; a target in the zone is a filled dot,
// outside a circle.
void Display::DrawRadar(const logic::MenuScreen &screen) {
  const int originX = 64, originY = AREA_BOTTOM;
  const int fullRadius = AREA_BOTTOM - AREA_TOP; // 39 px = 6 m
  int32_t rangeMm = screen.value[6];
  int32_t halfAngle = screen.value[7];
  int zoneRadius = constrain((int)(rangeMm * fullRadius / RADAR_RANGE_MM), 2, fullRadius);

  u8g2->drawCircle(originX, originY, zoneRadius, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
  float angle = halfAngle * PI / 180.0f;
  int dx = (int)(zoneRadius * sinf(angle));
  int dy = (int)(zoneRadius * cosf(angle));
  u8g2->drawLine(originX, originY, originX - dx, originY - dy);
  u8g2->drawLine(originX, originY, originX + dx, originY - dy);
  u8g2->drawBox(originX - 2, originY - 1, 5, 2); // the turret

  for (uint8_t i = 0; i < 3; i++) {
    int16_t x = screen.value[i * 2];
    int16_t y = screen.value[i * 2 + 1];
    if (y <= 0) {
      continue;
    }
    int px = originX + (int)((int32_t)x * fullRadius / RADAR_RANGE_MM);
    int py = originY - (int)((int32_t)y * fullRadius / RADAR_RANGE_MM);
    if (px < 3 || px > 124 || py < AREA_TOP + 2) {
      continue; // out of the drawing area
    }
    if (logic::IsInZone(x, y, rangeMm, halfAngle)) {
      u8g2->drawDisc(px, py, 2);
    } else {
      u8g2->drawCircle(px, py, 2);
    }
  }
}

// Curve of the last samples (newest on the right), with the two thresholds
// as dotted lines.
void Display::DrawGraph(const logic::MenuScreen &screen) {
  const int height = AREA_BOTTOM - AREA_TOP;
  auto toY = [&](int v) { return AREA_BOTTOM - constrain(v, 0, 255) * height / 255; };

  for (uint8_t t = 0; t < 2; t++) {
    int y = toY(screen.value[t]);
    for (int x = t * 2; x < 128; x += 4) {
      u8g2->drawPixel(x, y);
    }
  }
  int n = screen.dataLength > 128 ? 128 : screen.dataLength;
  for (int i = 1; i < n; i++) {
    int x1 = 128 - n + i;
    u8g2->drawLine(x1 - 1, toY(screen.data[i - 1]), x1, toY(screen.data[i]));
  }
}

// QR code on the left half (dark modules on a lit background, as scanners
// expect), a few short lines of text on the right.
void Display::DrawQr(const logic::MenuScreen &screen) {
  int size = screen.value[0];
  if (size > 0) {
    int scale = (size * 2 + 4 <= 64) ? 2 : 1;
    int total = size * scale;
    int offset = (64 - total) / 2;
    u8g2->drawBox(0, 0, 64, 64);
    u8g2->setDrawColor(0);
    for (int row = 0; row < size; row++) {
      for (int column = 0; column < size; column++) {
        int bit = row * size + column;
        if (bit / 8 < (int)logic::SCREEN_DATA_BYTES && (screen.data[bit / 8] & (0x80 >> (bit % 8)))) {
          u8g2->drawBox(offset + column * scale, offset + row * scale, scale, scale);
        }
      }
    }
    u8g2->setDrawColor(1);
  }
  for (uint8_t i = 0; i < logic::SCREEN_LINES; i++) {
    u8g2->drawUTF8(67, i * LINE_HEIGHT + BASELINE + 1, screen.line[i]);
  }
}

// The eye of the turret (normal mode): the pupil follows the target
// (value[0] = -100 .. 100), value[1] = 0 asleep, 1 awake, 2 angry.
// Lines 4 and 5 are the subtitles.
void Display::DrawEye(const logic::MenuScreen &screen) {
  const int centerX = 64, centerY = 22, rx = 30, ry = 18;
  int gaze = constrain((int)screen.value[0], -100, 100);
  int mood = screen.value[1];

  if (mood == 0) {
    // Closed: the lower lid and a line.
    u8g2->drawEllipse(centerX, centerY, rx, ry, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
    u8g2->drawHLine(centerX - rx, centerY, 2 * rx + 1);
  } else {
    u8g2->drawEllipse(centerX, centerY, rx, ry);
    int pupilX = centerX + gaze * (rx - 12) / 100;
    int pupilRadius = mood == 2 ? 10 : 8;
    u8g2->drawDisc(pupilX, centerY, pupilRadius);
    u8g2->setDrawColor(0);
    u8g2->drawDisc(pupilX - 3, centerY - 3, 2); // glint
    u8g2->setDrawColor(1);
    if (mood == 2) {
      // Brows slanted towards the centre.
      for (int t = 0; t < 2; t++) {
        u8g2->drawLine(centerX - rx, 1 + t, centerX - 6, 8 + t);
        u8g2->drawLine(centerX + rx, 1 + t, centerX + 6, 8 + t);
      }
    }
  }
  // Subtitles, centred.
  for (uint8_t i = 4; i < logic::SCREEN_LINES; i++) {
    int width = u8g2->getUTF8Width(screen.line[i]);
    u8g2->drawUTF8(width < 128 ? (128 - width) / 2 : 0, i * LINE_HEIGHT + 2 + BASELINE, screen.line[i]);
  }
}
