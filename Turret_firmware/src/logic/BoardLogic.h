#pragma once

// Pure board logic (button debounce and presses, LED fault code, amplifier gain),
// without Arduino: used by board/Board, audio/Amp and the native tests (test/test_logic).

#include <math.h>
#include <stdint.h>

namespace logic {

enum class PressEvent : uint8_t {
  None,
  ShortPress, // released before the long press delay
  LongPress,  // held for the long press delay (sent once, nothing on release)
};

class Debouncer {
public:
  Debouncer(uint32_t debounceMs, uint32_t longPressMs) : debounceMs(debounceMs), longPressMs(longPressMs) {}

  // Initial level. A button already held is not an event.
  void Reset(bool down, uint32_t now) {
    rawDown = stableDown = down;
    rawChangedAt = downSince = now;
    longSent = down;
  }

  // Called with the raw level at any rate; returns at most one event.
  PressEvent Update(bool down, uint32_t now) {
    PressEvent event = PressEvent::None;
    if (down != rawDown) {
      rawDown = down;
      rawChangedAt = now;
    }
    if (rawDown != stableDown && now - rawChangedAt >= debounceMs) {
      stableDown = rawDown;
      if (stableDown) {
        downSince = now;
        longSent = false;
      } else if (!longSent) {
        event = PressEvent::ShortPress;
      }
    }
    if (stableDown && !longSent && now - downSince >= longPressMs) {
      longSent = true;
      event = PressEvent::LongPress;
    }
    return event;
  }

  bool IsDown() const { return stableDown; }
  void SetLongPressMs(uint32_t ms) { longPressMs = ms; }

private:
  uint32_t debounceMs;
  uint32_t longPressMs;
  bool rawDown = false;
  bool stableDown = false;
  bool longSent = false;
  uint32_t rawChangedAt = 0;
  uint32_t downSince = 0;
};

// Red LED code (D4): the lowest active fault number (bit n-1 = fault n), 0 if none.
inline uint8_t LowestFault(uint8_t faults) {
  for (uint8_t n = 1; n <= 8; n++) {
    if (faults & (1u << (n - 1))) {
      return n;
    }
  }
  return 0;
}

// Status LEDs after boot: values of the settings GreenLedMode and RedLedMode
// (same order as their option lists in settings/Settings.cpp).
enum class GreenLedMode : uint8_t { Breathing, Heartbeat, Steady, Off };
enum class RedLedMode : uint8_t { FaultCodes, Steady, Off };

// Shortest period accepted (ms): the two beats of the heartbeat take 350 ms.
const uint32_t MIN_LED_PERIOD_MS = 400;

// Breathing (like the PPK2's LED): from off at t = 0 to full at half the
// period. Brightness 0..255 at time now (ms): a raised cosine, squared because
// the eye sees small changes at low light much more than at full light.
inline uint8_t BreathLevel(uint32_t now, uint32_t periodMs) {
  if (periodMs < MIN_LED_PERIOD_MS) {
    periodMs = MIN_LED_PERIOD_MS;
  }
  uint32_t t = now % periodMs;
  float s = 0.5f - 0.5f * cosf(6.2831853f * (float)t / (float)periodMs);
  return (uint8_t)(s * s * 255.0f + 0.5f);
}

// Heartbeat: two short beats ("lub-dub", 100 ms at 0 and at 250 ms), then a
// rest until the end of the period. True while the LED is on.
inline bool HeartbeatOn(uint32_t now, uint32_t periodMs) {
  if (periodMs < MIN_LED_PERIOD_MS) {
    periodMs = MIN_LED_PERIOD_MS;
  }
  uint32_t t = now % periodMs;
  return t < 100 || (t >= 250 && t < 350);
}

// A brightness 0..255 scaled by percent (0..100), rounded.
inline uint8_t ScaleLevel(uint8_t level, uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  return (uint8_t)(((uint16_t)level * percent + 50) / 100);
}

// Green LED brightness 0..255 at time now, for a mode, a period and a maximum brightness (%).
inline uint8_t GreenLedLevel(GreenLedMode mode, uint32_t now, uint32_t periodMs, uint8_t percent) {
  switch (mode) {
  case GreenLedMode::Breathing:
    return ScaleLevel(BreathLevel(now, periodMs), percent);
  case GreenLedMode::Heartbeat:
    return HeartbeatOn(now, periodMs) ? ScaleLevel(255, percent) : 0;
  case GreenLedMode::Steady:
    return ScaleLevel(255, percent);
  case GreenLedMode::Off:
    break;
  }
  return 0;
}

// Factory reset with the board's own controls, no screen needed: SW1 closed
// (debug position) and PROG (SW2, IO0) held RESET_GESTURE_HOLD_MS, at any
// time after the boot (PROG held while RESET is released starts the ROM
// download mode instead: that is why RESET is not part of the gesture).
// Releasing PROG or opening SW1 before the end cancels. PROG must have been
// seen released once since the boot before a hold counts.
const uint32_t RESET_GESTURE_HOLD_MS = 5000;

class ResetGesture {
public:
  enum class State : uint8_t { Idle, Holding, Accepted };

  State Update(bool switchClosed, bool progDown, uint32_t now) {
    if (state == State::Accepted) {
      return state; // final: the board restarts
    }
    if (!progDown) {
      armed = true;
    }
    if (!(armed && progDown && switchClosed)) {
      state = State::Idle;
      return state;
    }
    if (state == State::Idle) {
      state = State::Holding;
      since = now;
    }
    if (now - since >= RESET_GESTURE_HOLD_MS) {
      state = State::Accepted;
    }
    return state;
  }
  State GetState() const { return state; }
  // Holding: milliseconds left before the reset (rounded up to whole seconds by the caller).
  uint32_t RemainingMs(uint32_t now) const {
    if (state != State::Holding) {
      return state == State::Accepted ? 0 : RESET_GESTURE_HOLD_MS;
    }
    uint32_t held = now - since;
    return held >= RESET_GESTURE_HOLD_MS ? 0 : RESET_GESTURE_HOLD_MS - held;
  }

private:
  State state = State::Idle;
  bool armed = false;
  uint32_t since = 0;
};

// MAX98357A gain: only 9, 12 or 15 dB exist; anything else goes to the nearest.
inline int32_t RoundAmpGain(int32_t db) { return db <= 10 ? 9 : (db <= 13 ? 12 : 15); }

} // namespace logic
