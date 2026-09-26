#pragma once

// Pure logic of the HLK-LD2450 radar, without Arduino: used by sensors/Radar
// and by the native tests (test/test_logic).

#include <math.h>
#include <stdint.h>

namespace logic {

// LD2450 coordinate / speed field (little endian): bit 15 set = positive,
// clear = negative; the magnitude is in the other 15 bits.
inline int16_t DecodeLd2450(uint8_t low, uint8_t high) {
  int16_t magnitude = (int16_t)(((high & 0x7F) << 8) | low);
  return (high & 0x80) ? magnitude : (int16_t)-magnitude;
}

// Detection zone (improvement A2): in front of the radar (y > 0), closer than
// maxDistanceMm and within +-halfAngleDeg of its axis. x, y in mm.
inline bool IsInZone(int16_t x, int16_t y, int32_t maxDistanceMm, int32_t halfAngleDeg) {
  if (y <= 0) {
    return false;
  }
  float distance = sqrtf((float)x * x + (float)y * y);
  float angle = fabsf(atan2f((float)x, (float)y)) * 180.0f / 3.14159265f;
  return distance <= (float)maxDistanceMm && angle <= (float)halfAngleDeg;
}

} // namespace logic
