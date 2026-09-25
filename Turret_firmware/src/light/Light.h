#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "settings/Settings.h"

class Light {
public:
  // Boot step 2: registers the strips and sends a black frame, which clears the
  // random colours latched while the AHCT125 inputs floated, and caps the power.
  void Initialize();
  void Update(ulong deltaTime);

  // FastLED power cap on the 5 V rail (13 LEDs at full white draw ~0.8 A).
  void SetMaxMilliamps(uint32_t milliamps);
  void SetBrightness(uint8_t brightness);
  // Disabled = black frame, Update() draws nothing (fault, shutdown).
  void SetEnabled(bool enabled);
  // LedBright and LedMaxmA; 10 % in reduced mode (D5).
  void ApplySettings(Settings &settings, bool reducedMode);

  // Test mode (console / web page): fixed colour on the selected strips
  // (bit 0 = ring, bit 1 = left gun, bit 2 = right gun) until ClearTest().
  void SetTestColor(uint8_t stripMask, CRGB color);
  void ClearTest();

private:
  void Show();

  CRGB centerLeds[9];
  CRGB leftLeds[2];
  CRGB rightLeds[2];
  bool enabled = true;
  bool testMode = false;
  uint8_t testMask = 0;
  CRGB testColor;
};
