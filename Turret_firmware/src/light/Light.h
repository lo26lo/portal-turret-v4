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
  bool IsEnabled() const { return enabled; }
  // LedBright and LedMaxmA; 10 % in reduced mode (D5).
  void ApplySettings(Settings &settings, bool reducedMode);

  // Test mode (console / web page): fixed colour on the selected strips
  // (bit 0 = ring, bit 1 = left gun, bit 2 = right gun) until ClearTest().
  void SetTestColor(uint8_t stripMask, CRGB color);
  void ClearTest();
  // L3 (lab aid): fixed frame with a single bit set, readable on an
  // oscilloscope: first LED of the ring = red 0x80 (bit 9 of 24, GRB order),
  // everything else 0; brightness 255 and no dithering so that the bytes
  // are sent exactly. Leaving it restores the brightness.
  void SetBitPattern(bool enabled);

private:
  void Show();

  CRGB centerLeds[9];
  CRGB leftLeds[2];
  CRGB rightLeds[2];
  bool enabled = true;
  bool testMode = false;
  bool bitPattern = false;
  uint8_t savedBrightness = 255;
  uint8_t testMask = 0;
  CRGB testColor;
};
