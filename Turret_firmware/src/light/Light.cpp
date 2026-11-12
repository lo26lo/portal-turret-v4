#include "Light.h"
#include "board/Power.h"
#include "logic/BoardLogic.h" // BreathLevel
#include "pins.h"

namespace {
const uint32_t DEFAULT_MAX_MILLIAMPS = 500;
const uint8_t REDUCED_BRIGHTNESS = 26; // 10 %
} // namespace

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

void Light::SetBrightness(uint8_t brightness) {
  FastLED.setBrightness(brightness);
}

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
  int32_t value = settings.GetInt(SettingId::LedEffect);
  effect = value == 1 ? Effect::Rainbow : value == 2 ? Effect::Eye : Effect::RedEye;
}

void Light::SetTestColor(uint8_t stripMask, CRGB color) {
  testMode = true;
  rainbow = false;
  testMask = stripMask;
  testColor = color;
}

void Light::SetRainbow(uint8_t stripMask) {
  testMode = true;
  rainbow = true;
  testMask = stripMask;
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
  if (testMode && rainbow) {
    DrawRainbow(testMask);
    return;
  }
  if (testMode) {
    fill_solid(centerLeds, 9, (testMask & 1) ? testColor : CRGB::Black);
    fill_solid(leftLeds, 2, (testMask & 2) ? testColor : CRGB::Black);
    fill_solid(rightLeds, 2, (testMask & 4) ? testColor : CRGB::Black);
    Show();
    return;
  }
  if (effect == Effect::Rainbow) {
    DrawRainbow(7);
    return;
  }
  if (effect == Effect::Eye) {
    DrawEye();
    return;
  }

  DrawGunsGlow(millis() / 4);
  fill_solid(centerLeds, 9, CRGB::Red);
  Show();
}

// Guns: the flickering fire colour of the upstream firmware.
void Light::DrawGunsGlow(uint8_t t) {
  CRGB glow = HeatColor(triwave8(t));
  fill_solid(leftLeds, 2, glow);
  fill_solid(rightLeds, 2, glow);
}

// B6: the eye follows the state machine (Mood, set by main every loop).
void Light::DrawEye() {
  uint32_t now = millis();
  DrawGunsGlow(now / 4);
  CRGB ring;
  switch (mood) {
  case Mood::Idle: {
    // Breathing red, never fully off (the eye stays visible): the raised
    // cosine of the green status LED, 4 s per breath.
    uint8_t level = logic::BreathLevel(now, 4000);
    ring = CRGB(40 + (uint16_t)level * 215 / 255, 0, 0);
    break;
  }
  case Mood::Acquired: ring = CRGB::Red; break;
  case Mood::Firing: {
    // 12 Hz blink; the guns flash bright with it.
    bool on = (now / 40) % 2 == 0;
    ring = on ? CRGB::Red : CRGB(60, 0, 0);
    if (on) {
      fill_solid(leftLeds, 2, CRGB(255, 160, 60));
      fill_solid(rightLeds, 2, CRGB(255, 160, 60));
    }
    break;
  }
  case Mood::Searching:
    ring = CRGB(110, 150, 255); // cold white-blue: "are you still there?"
    break;
  }
  fill_solid(centerLeds, 9, ring);
  Show();
}

void Light::DrawRainbow(uint8_t stripMask) {
  // One turn of the colour wheel every ~2 s (hue + 1 every 8 ms).
  uint8_t hue = millis() / 8;
  fill_solid(centerLeds, 9, CRGB::Black);
  fill_solid(leftLeds, 2, CRGB::Black);
  fill_solid(rightLeds, 2, CRGB::Black);
  if (stripMask & 1) {
    fill_rainbow(centerLeds, 9, hue, 256 / 9);
  }
  if (stripMask & 2) {
    fill_solid(leftLeds, 2, CHSV(hue, 255, 255));
  }
  if (stripMask & 4) {
    fill_solid(rightLeds, 2, CHSV(hue, 255, 255));
  }
  Show();
}

void Light::Show() {
  // The last LED of the ring has its red and green channels swapped.
  std::swap(centerLeds[8].r, centerLeds[8].g);
  FastLED.show();
  std::swap(centerLeds[8].r, centerLeds[8].g);
}
