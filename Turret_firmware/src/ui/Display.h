#pragma once

#include <Arduino.h>
#include <U8g2lib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "logic/MenuLogic.h"

// OLED 128x64 on the Qwiic port J11 (improvement M1), shared I2C bus on
// IO35 / IO36. Controllers (OledType setting, it cannot be detected):
//   0 = SSD1306 (0.96"), 1 = SSD1309 (1.54"), 2 = SH1106 (1.3").
// The I2C address is detected: 0x3C, otherwise 0x3D.
// The screen is drawn from a logic::MenuScreen: six lines of text (21
// columns), or one of the drawings of logic::Graphic.
//
// Sending a frame takes about 25 ms on the bus. It is done by a task of its
// own, so loop() is never held up by the screen (the wings must stop on
// their Hall threshold without delay). The bus is shared safely: Wire takes
// a lock per transaction, and U8g2 sends the frame in 24-byte transactions
// (~0.7 ms each), so an IMU read from loop() waits about 1 ms at most.
class Display {
public:
  // After I2cBus::Begin(). Returns true if a screen answered.
  bool Begin(int32_t oledType);
  bool IsPresent() const { return present; }
  uint8_t GetAddress() const { return address; }
  // Hands the screen to the display task and returns at once. If frames
  // come faster than the screen takes them, only the latest is drawn.
  void Show(const logic::MenuScreen &screen);
  // Rotation by 180 degrees (OledFlip), contrast 0..255 (OledContrast) and
  // on / off (sleep). Applied by the display task before its next frame.
  void Configure(bool flip, uint8_t contrast);
  void SetPower(bool on);
  bool IsOn() const { return wantOn; }

private:
  static void TaskEntry(void *self);
  void Run();
  void Draw(const logic::MenuScreen &screen);
  void DrawFrameLines(const logic::MenuScreen &screen);
  void DrawWings(const logic::MenuScreen &screen);
  void DrawRadar(const logic::MenuScreen &screen);
  void DrawGraph(const logic::MenuScreen &screen);
  void DrawQr(const logic::MenuScreen &screen);
  void DrawEye(const logic::MenuScreen &screen);

  U8G2 *u8g2 = nullptr;
  bool present = false;
  uint8_t address = 0;

  TaskHandle_t task = nullptr;
  SemaphoreHandle_t lock = nullptr; // guards `pending`
  logic::MenuScreen pending = {};

  // Wanted by loop(), applied by the task.
  volatile bool wantFlip = false;
  volatile uint8_t wantContrast = 255;
  volatile bool wantOn = true;
  bool appliedFlip = false;
  uint8_t appliedContrast = 255;
  bool appliedOn = true;
  bool configured = false;
};
