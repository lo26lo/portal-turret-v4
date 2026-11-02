#include "Light.h"
#include "board/Power.h"
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
  // v2: only towards a powered ring (a patrol boot leaves the LED rail off).
  if (Power::IsOn(Rail::Led)) {
    FastLED.show();
  }
}

void Light::SetMaxMilliamps(uint32_t milliamps) {
  FastLED.setMaxPowerInVoltsAndMilliamps(5, milliamps);
}

void Light::SetBrightness(uint8_t brightness) { FastLED.setBrightness(brightness); }

void Light::SetEnabled(bool enabledIn) {
  enabled = enabledIn;
  if (!enabled) {
    FastLED.clear();
    // v2: no frame towards an unpowered ring (the AHCT would feed it).
    if (Power::IsOn(Rail::Led)) {
      FastLED.show();
    }
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

void Light::ClearTest() {
  testMode = false;
  SetBitPattern(false);
}

void Light::SetBitPattern(bool enabled) {
  if (enabled == bitPattern) {
    return;
  }
  bitPattern = enabled;
  if (enabled) {
    savedBrightness = FastLED.getBrightness();
    FastLED.setBrightness(255); // scale8 is exact at 255 (FASTLED_SCALE8_FIXED)
    FastLED.setDither(DISABLE_DITHER);
  } else {
    FastLED.setBrightness(savedBrightness);
    FastLED.setDither(BINARY_DITHER);
  }
}

void Light::Update(ulong deltaTime) {
  // v2: nothing is sent while the LED rail is off (plan v2 §2.1).
  if (!enabled || !Power::IsOn(Rail::Led)) {
    return;
  }
  if (bitPattern) {
    fill_solid(centerLeds, 9, CRGB::Black);
    fill_solid(leftLeds, 2, CRGB::Black);
    fill_solid(rightLeds, 2, CRGB::Black);
    centerLeds[0] = CRGB(0x80, 0x00, 0x00);
    FastLED.show(); // no Show(): the channel swap of LED 8 does not matter here
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
