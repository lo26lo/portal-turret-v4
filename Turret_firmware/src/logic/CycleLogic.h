#pragma once

// Pure logic of the firing cycle (Activate -> Firing -> Searching -> Disengage),
// without Arduino: used by states/* and the native tests (test/test_logic).
// Everything that decides when a state ends is here, so that it can be tested
// on the computer; the states only move the hardware.

#include <math.h>
#include <stdint.h>

namespace logic {

// Angle of a radar target from the radar axis, in degrees, positive on the
// side of positive x, bounded to +-maxDeg. 0 for a target behind or on the
// radar (y <= 0): the turret does not turn towards what it cannot see.
inline float AimAngleDeg(int16_t x, int16_t y, float maxDeg) {
  if (y <= 0) {
    return 0;
  }
  float angle = atan2f((float)x, (float)y) * 180.0f / 3.14159265f;
  return angle > maxDeg ? maxDeg : (angle < -maxDeg ? -maxDeg : angle);
}

// Smoothed aim: moves `current` a share `alpha` (0..1) of the way to `target`
// each call, so that the rotation servo follows the radar without jitter.
inline float SmoothAim(float current, float target, float alpha) {
  if (alpha <= 0) {
    return current;
  }
  if (alpha >= 1) {
    return target;
  }
  return current + (target - current) * alpha;
}

// Firing: a burst of `fireMs` from the start of the state. Signed elapsed, so
// that a `now` taken before the start (same loop) counts as 0.
inline bool FiringDone(uint32_t now, uint32_t startedAt, uint32_t fireMs) {
  return (int32_t)(now - startedAt) >= (int32_t)fireMs;
}

// Searching (target lost after a burst): what the state does next.
enum class SearchStep : uint8_t {
  Keep,   // keep looking
  Fire,   // a target is back in the zone: fire again
  GiveUp, // searchMs over without a target: retire
};

inline SearchStep SearchDecision(bool targetInZone, uint32_t now, uint32_t startedAt, uint32_t searchMs) {
  if (targetInZone) {
    return SearchStep::Fire;
  }
  if ((int32_t)(now - startedAt) >= (int32_t)searchMs) {
    return SearchStep::GiveUp;
  }
  return SearchStep::Keep;
}

// Search sweep: the rotation Z goes from -sweepDeg to +sweepDeg and back,
// one full round every periodMs, starting at 0 and going to the positive side
// first (a triangle wave, like the lab servo sweep).
inline float SearchSweepDeg(uint32_t now, uint32_t startedAt, uint32_t periodMs, float sweepDeg) {
  if (periodMs == 0 || sweepDeg <= 0) {
    return 0;
  }
  uint32_t t = (now - startedAt) % periodMs;
  float p = (float)t / (float)periodMs; // 0..1
  // 0 -> +1 at 1/4 -> 0 at 1/2 -> -1 at 3/4 -> 0 at 1.
  float tri = p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
  return tri * sweepDeg;
}

} // namespace logic
