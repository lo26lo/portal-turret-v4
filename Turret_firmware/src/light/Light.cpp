#include "Light.h"
#include "pins.h"

namespace {
const uint32_t DEFAULT_MAX_MILLIAMPS = 500;
const uint8_t REDUCED_BRIGHTNESS = 26; // 10 %
}

void Light::Initialize() {
  FastLED.addLeds<WS2812, PIN_NEOPIXEL_CENTER, GRB>(centerLeds, 9);
  FastLED.addLeds<WS2812, PIN_NEOPIXEL_LEFT, RGB>(leftLeds, 2);
  FastLED.addLeds<WS2812, PIN_NEOPIXEL_RIGHT, RGB>(rightLeds, 2);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, DEFAULT_MAX_MILLIAMPS);

  FastLED.clear();
  FastLED.show();
}

void Light::SetMaxMilliamps(uint32_t milliamps) {
  FastLED.setMaxPowerInVoltsAndMilliamps(5, milliamps);
}

void Light::SetBrightness(uint8_t brightness) { FastLED.setBrightness(brightness); }

void Light::SetEnabled(bool enabledIn) {
  enabled = enabledIn;
  if (!enabled) {
    FastLED.clear();
    FastLED.show();
  }
}

void Light::ApplySettings(Settings &settings, bool reducedMode) {
  SetMaxMilliamps(settings.GetInt(SettingId::LedMaxmA));
  SetBrightness(reducedMode ? REDUCED_BRIGHTNESS : settings.GetInt(SettingId::LedBright));
}

void Light::SetTestColor(uint8_t stripMask, CRGB color) {
  testMode = true;
  testMask = stripMask;
  testColor = color;
}

void Light::ClearTest() { testMode = false; }

void Light::Update(ulong deltaTime) {
  if (!enabled) {
    return;
  }
  if (testMode) {
    fill_solid(centerLeds, 9, (testMask & 1) ? testColor : CRGB::Black);
    fill_solid(leftLeds, 2, (testMask & 2) ? testColor : CRGB::Black);
    fill_solid(rightLeds, 2, (testMask & 4) ? testColor : CRGB::Black);
    Show();
    return;
  }

  uint8_t t = millis() / 4;
  uint8_t tri = triwave8(t);

  fill_solid(leftLeds, 2, HeatColor(tri));
  fill_solid(rightLeds, 2, HeatColor(tri));
  fill_solid(centerLeds, 9, CRGB::Red);
  Show();
}

void Light::Show() {
  // The last LED of the ring has its red and green channels swapped.
  std::swap(centerLeds[8].r, centerLeds[8].g);
  FastLED.show();
  std::swap(centerLeds[8].r, centerLeds[8].g);
}
