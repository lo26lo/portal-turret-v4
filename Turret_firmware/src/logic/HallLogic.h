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

// A reading at a rail: sensor unplugged or shorted.
inline bool HallAtRail(uint16_t value) { return value <= 15 || value >= 4080; }

} // namespace logic
