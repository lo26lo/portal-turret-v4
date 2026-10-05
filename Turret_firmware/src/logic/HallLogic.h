#pragma once

// Pure logic of the wing Hall sensors, without Arduino: used by motion/Wing
// and by the native tests (test/test_logic).

#include <stdint.h>

namespace logic {

// The magnet can be mounted either way round: if the "open" threshold is
// below the "closed" one, the comparisons are reversed.
inline bool HallPastOpen(uint16_t value, int32_t openThreshold, int32_t closedThreshold) {
  return openThreshold >= closedThreshold ? value >= openThreshold : value <= openThreshold;
}

inline bool HallPastClosed(uint16_t value, int32_t openThreshold, int32_t closedThreshold) {
  return openThreshold >= closedThreshold ? value <= closedThreshold : value >= closedThreshold;
}

// How far the wing is open, 0..100, from the Hall value: 0 at the closed
// threshold, 100 at the open one, whatever the polarity. Only for display
// (the animation on the debug screen): the thresholds themselves decide.
inline uint8_t HallPercent(uint16_t value, int32_t openThreshold, int32_t closedThreshold) {
  int32_t span = openThreshold - closedThreshold;
  if (span == 0) {
    return 0;
  }
  int32_t percent = ((int32_t)value - closedThreshold) * 100 / span;
  return (uint8_t)(percent < 0 ? 0 : (percent > 100 ? 100 : percent));
}

// A reading at a rail: sensor unplugged or shorted.
inline bool HallAtRail(uint16_t value) { return value <= 15 || value >= 4080; }

} // namespace logic
