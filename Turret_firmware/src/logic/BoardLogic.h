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

// MAX98357A gain: only 9, 12 or 15 dB exist; anything else goes to the nearest.
inline int32_t RoundAmpGain(int32_t db) { return db <= 10 ? 9 : (db <= 13 ? 12 : 15); }

} // namespace logic
