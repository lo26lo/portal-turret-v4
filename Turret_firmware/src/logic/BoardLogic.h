#pragma once

// Pure board logic (button debounce and presses, LED fault code, amplifier gain),
// without Arduino: used by board/Board, audio/Amp and the native tests (test/test_logic).

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

// Green LED after boot: a heartbeat, two short beats ("lub-dub") then a rest,
// 50 beats per minute. True while the LED is on at time now (ms).
const uint32_t HEARTBEAT_PERIOD_MS = 1200;
inline bool HeartbeatOn(uint32_t now) {
  uint32_t t = now % HEARTBEAT_PERIOD_MS;
  return t < 100 || (t >= 250 && t < 350);
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
