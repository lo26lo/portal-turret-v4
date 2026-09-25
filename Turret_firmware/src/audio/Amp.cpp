#include "Amp.h"

#include "pins.h"

namespace {
const ulong GAIN_SETTLE_MS = 10;

// Open drain: HIGH = released (high impedance), LOW = pulled to ground.
// pinMode is set again each time so that the pins can never end up push-pull.
void GainPin(uint8_t pin, bool pullLow) {
  pinMode(pin, OUTPUT_OPEN_DRAIN);
  digitalWrite(pin, pullLow ? LOW : HIGH);
}
} // namespace

void Amp::Initialize() {
  digitalWrite(PIN_AMP_SD, LOW);
  pinMode(PIN_AMP_SD, OUTPUT);
  digitalWrite(PIN_AMP_SD, LOW);
  running = false;
  started = false;
  ApplyGainPins();
}

void Amp::SetGain(int32_t db) {
  int32_t rounded = db <= 10 ? 9 : (db <= 13 ? 12 : 15);
  if (rounded == gainDb) {
    return;
  }
  gainDb = rounded;
  bool wasRunning = running;
  digitalWrite(PIN_AMP_SD, LOW);
  running = false;
  ApplyGainPins();
  if (wasRunning) {
    delay(GAIN_SETTLE_MS);
    digitalWrite(PIN_AMP_SD, HIGH);
    running = true;
  }
}

void Amp::ApplyGainPins() {
  GainPin(PIN_AMP_GAIN, gainDb == 12);
  GainPin(PIN_AMP_GAIN_100K, gainDb == 15);
}

void Amp::Start() {
  started = true;
  if (muted) {
    return;
  }
  digitalWrite(PIN_AMP_SD, LOW);
  ApplyGainPins();
  delay(GAIN_SETTLE_MS);
  digitalWrite(PIN_AMP_SD, HIGH);
  running = true;
}

void Amp::Shutdown() {
  digitalWrite(PIN_AMP_SD, LOW);
  running = false;
  started = false;
}

void Amp::SetMuted(bool mutedIn) {
  muted = mutedIn;
  if (muted) {
    digitalWrite(PIN_AMP_SD, LOW);
    running = false;
  } else if (started && !running) {
    ApplyGainPins();
    delay(GAIN_SETTLE_MS);
    digitalWrite(PIN_AMP_SD, HIGH);
    running = true;
  }
}
