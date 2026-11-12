#include "Power.h"

#include "board/Expander.h"
#include "board/Log.h"
#include "pins.h"

namespace {
#if BOARD_HAS_EXPANDER
// Gap between two switch-ons (plan v2 §2: ">= 10 ms apart").
const ulong SWITCH_ON_GAP_MS = 10;
// Time for the rail to come up. TPS22965: t_R 8.2 ms with CT 4.7 nF (servos),
// 2.0 ms with 1 nF (LEDs, radar) at VIN 5 V. OH49E: start-up time not in its
// datasheet (plan v2 §4.3, to measure): a few ms of margin.
const ulong RISE_MS[Power::RAIL_COUNT] = {15, 5, 5, 5};
const uint8_t PORT[Power::RAIL_COUNT] = {EXPANDER_SERVO_EN, EXPANDER_LED_EN, EXPANDER_RADAR_EN, EXPANDER_HALL_EN_N};
const bool ACTIVE_LOW[Power::RAIL_COUNT] = {false, false, false, true};
const uint8_t SERVO_PINS[] = {PIN_GUN_LEFT, PIN_GUN_RIGHT, PIN_WING_LEFT, PIN_WING_RIGHT, PIN_ROTATE_X, PIN_ROTATE_Z};

bool railOn[Power::RAIL_COUNT] = {false, false, false, false};
ulong lastOnAt = 0;
bool switchedOnce = false;

bool Drive(Rail rail, bool on) {
  uint8_t i = (uint8_t)rail;
  bool level = ACTIVE_LOW[i] ? !on : on;
  return Expander::SetPort(PORT[i], level);
}
#endif
} // namespace

namespace Power {

bool IsSwitchable() {
  return BOARD_HAS_EXPANDER != 0;
}

#if BOARD_HAS_EXPANDER

bool On(Rail rail) {
  uint8_t i = (uint8_t)rail;
  if (railOn[i]) {
    return true;
  }
  if (switchedOnce) {
    long wait = (long)SWITCH_ON_GAP_MS - (long)(millis() - lastOnAt);
    if (wait > 0) {
      delay(wait);
    }
  }
  if (!Drive(rail, true)) {
    Log.printf("Power: %s rail did not switch on (expander)\n", Name(rail));
    return false;
  }
  railOn[i] = true;
  delay(RISE_MS[i]);
  lastOnAt = millis();
  switchedOnce = true;
  Log.printf("Power: %s rail on\n", Name(rail));
  return true;
}

bool Off(Rail rail) {
  uint8_t i = (uint8_t)rail;
  if (rail == Rail::Servo) {
    // Servos released by the caller: their lines low, not floating at the
    // last PWM level (R28-R33 330R towards the connectors).
    for (uint8_t pin : SERVO_PINS) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }
  }
  // LEDs: the data lines rest low once Light is disabled (FastLED ends each
  // frame low; R22-R24 pull the AHCT inputs down). Radar: Radar::End() has
  // put its TX line back to an input. Hall: analog inputs only.
  bool ok = Drive(rail, false);
  if (ok) {
    if (railOn[i]) {
      Log.printf("Power: %s rail off\n", Name(rail));
    }
    railOn[i] = false;
  } else {
    Log.printf("Power: %s rail did not switch off (expander)\n", Name(rail));
  }
  return ok;
}

bool AllOff() {
  bool ok = Off(Rail::Servo);
  ok = Off(Rail::Led) && ok;
  ok = Off(Rail::Radar) && ok;
  ok = Off(Rail::Hall) && ok;
  // Whatever happened above, the whole port to its reset-safe value.
  return Expander::AllOff() && ok;
}

bool IsOn(Rail rail) {
  return railOn[(uint8_t)rail];
}

#else // v0.1: rails always on

bool On(Rail) {
  return true;
}
bool Off(Rail) {
  return true;
}
bool AllOff() {
  return true;
}
bool IsOn(Rail) {
  return true;
}

#endif

const char *Name(Rail rail) {
  switch (rail) {
  case Rail::Servo: return "servo";
  case Rail::Led: return "LED";
  case Rail::Radar: return "radar";
  case Rail::Hall: return "Hall";
  }
  return "?";
}

bool Parse(const String &name, Rail &rail) {
  String n = name;
  n.toLowerCase();
  if (n == "servo" || n == "servos") {
    rail = Rail::Servo;
  } else if (n == "led" || n == "leds") {
    rail = Rail::Led;
  } else if (n == "radar") {
    rail = Rail::Radar;
  } else if (n == "hall") {
    rail = Rail::Hall;
  } else {
    return false;
  }
  return true;
}

} // namespace Power
