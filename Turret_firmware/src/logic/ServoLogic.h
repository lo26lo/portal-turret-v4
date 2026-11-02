#pragma once

// Pure servo logic, without Arduino: used by motion/ServoChannel and the
// native tests (test/test_logic).

#include <stdint.h>

namespace logic {

// Speed ramp of a positional servo (setting ServoRamp): the pulse moves from
// current towards target by at most usPerSecond x elapsed, and at least 1 us
// per call while it is not there, so that a slow ramp always ends.
// usPerSecond <= 0: no ramp, straight to the target.
inline int RampStep(int current, int target, int32_t usPerSecond, uint32_t elapsedMs) {
  if (usPerSecond <= 0 || current == target) {
    return target;
  }
  int64_t step = (int64_t)usPerSecond * elapsedMs / 1000;
  if (step < 1) {
    step = 1;
  }
  int64_t distance = (int64_t)target - current;
  if (distance > step) {
    return current + (int)step;
  }
  if (distance < -step) {
    return current - (int)step;
  }
  return target;
}

} // namespace logic
