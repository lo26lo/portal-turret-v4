#pragma once

#include <Arduino.h>

#include "logic/BoardLogic.h"

// Optional I2C rotary encoder with push button on the Qwiic port J11, shared
// with the IMU and the OLED. Detected once at boot, in this order:
//  - Adafruit I2C QT Rotary Encoder (product 4991, seesaw), 0x36;
//  - Adafruit I2C Quad Rotary Encoder (product 5752, seesaw), 0x49: knob 0
//    only (the one next to the "0" mark), encoders soldered by the user;
//  - Soldered Rotary Encoder Breakout (333188, ATtiny404, easyC), 0x30.
// Address jumpers / switches left as delivered. Missing = nothing happens.
// No interrupt line on the Qwiic port: polled from loop() every 30 ms, about
// 1 ms of bus per poll.
class RotaryEncoder {
public:
  enum class Model : uint8_t { None, Adafruit4991, AdafruitQuad5752, Soldered };

  // After I2cBus::Begin(). True if one of the models above answers.
  bool Begin();
  // loop(): reads the turns and the push button.
  void Update(ulong now);

  bool IsPresent() const { return model != Model::None; }
  Model GetModel() const { return model; }
  const char *ModelName() const;
  // Detents turned since the last call, clockwise positive.
  int32_t TakeSteps();
  // Push button: ShortPress / LongPress, once each.
  logic::PressEvent TakeEvent();
  // Seesaw models only: the Soldered board decides its long press itself.
  void SetLongPressMs(uint32_t ms) { button.SetLongPressMs(ms); }

private:
  bool BeginSeesaw(uint8_t address, uint16_t product, uint8_t buttonPin);
  bool BeginSoldered();
  bool UpdateSeesaw(ulong now);
  bool UpdateSoldered();
  bool SeesawRead(uint8_t module, uint8_t reg, uint8_t *data, uint8_t length);
  bool SeesawWrite(uint8_t module, uint8_t reg, const uint8_t *data, uint8_t length);
  void Push(logic::PressEvent event);

  Model model = Model::None;
  uint8_t address = 0;
  uint32_t buttonMask = 0; // seesaw GPIO of the push switch
  int32_t lastCount = 0;   // Soldered: counter at the previous poll
  ulong lastPoll = 0;
  int32_t steps = 0;
  logic::PressEvent pending[2] = {logic::PressEvent::None, logic::PressEvent::None};
  logic::Debouncer button{30, 3000};
  uint8_t failures = 0;
};
