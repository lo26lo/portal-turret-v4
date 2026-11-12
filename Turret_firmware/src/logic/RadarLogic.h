#pragma once

// Pure logic of the HLK-LD2450 radar (the Ai-Thinker RD-03D fitted on the
// first board sends the same frames), without Arduino: used by sensors/Radar
// and by the native tests (test/test_logic).

#include <math.h>
#include <stdint.h>
#include <string.h>

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

// One target of a data frame; `present` is false for the 8 zero bytes of an
// empty slot.
struct RadarFrameTarget {
  bool present;
  int16_t x;        // mm, positive to one side of the radar axis
  int16_t y;        // mm, distance ahead
  int16_t speed;    // cm/s, signed
  uint16_t resolution; // mm
};

const uint8_t RADAR_TARGETS = 3;

struct RadarFrame {
  RadarFrameTarget target[RADAR_TARGETS];
};

// Data frame of the LD2450 / RD-03D, 30 bytes, about ten per second:
//   AA FF 03 00 | 3 x (x lo, x hi, y lo, y hi, speed lo, speed hi, res lo, res hi) | 55 CC
// Fixed length: the footer is only looked for after the 24 payload bytes, so
// a 55 CC inside the data cannot cut a frame short. A frame whose footer is
// wrong is dropped and counted (BadFrames), then the stream is re-synchronised
// on the next header. The configuration acknowledgements (FD FC FB FA ...) and
// any other bytes are skipped silently: they never match the header.
class RadarParser {
public:
  static const uint8_t PAYLOAD = 8 * RADAR_TARGETS; // 24

  // Feeds one byte; true when a whole frame has just been decoded into Frame().
  bool Feed(uint8_t byte) {
    // Function-local: a static constexpr array member would need a definition
    // in a .cpp before C++17 (the ESP32 build is gnu++11).
    static const uint8_t HEADER[4] = {0xAA, 0xFF, 0x03, 0x00};
    static const uint8_t FOOTER[2] = {0x55, 0xCC};
    if (index < 4) {
      // Header, byte by byte; a mismatch restarts, the byte itself may be a new start.
      if (byte == HEADER[index]) {
        index++;
      } else {
        index = byte == HEADER[0] ? 1 : 0;
      }
      return false;
    }
    if (index < 4 + PAYLOAD) {
      payload[index - 4] = byte;
      index++;
      return false;
    }
    // Footer.
    if (byte != FOOTER[index - 4 - PAYLOAD]) {
      badFrames++;
      index = byte == HEADER[0] ? 1 : 0;
      return false;
    }
    index++;
    if (index < 4 + PAYLOAD + 2) {
      return false;
    }
    index = 0;
    Decode();
    frames++;
    return true;
  }

  const RadarFrame &Frame() const { return frame; }
  uint32_t Frames() const { return frames; }
  uint32_t BadFrames() const { return badFrames; }
  void Reset() {
    index = 0;
    frames = badFrames = 0;
    memset(&frame, 0, sizeof(frame));
  }

private:
  void Decode() {
    for (uint8_t i = 0; i < RADAR_TARGETS; i++) {
      const uint8_t *b = payload + i * 8;
      RadarFrameTarget &t = frame.target[i];
      uint8_t any = 0;
      for (uint8_t k = 0; k < 8; k++) {
        any |= b[k];
      }
      t.present = any != 0;
      t.x = DecodeLd2450(b[0], b[1]);
      t.y = DecodeLd2450(b[2], b[3]);
      t.speed = DecodeLd2450(b[4], b[5]);
      t.resolution = (uint16_t)(b[6] | (b[7] << 8));
    }
  }

  uint8_t index = 0; // position in the frame: 0..3 header, 4..27 payload, 28..29 footer
  uint8_t payload[PAYLOAD] = {};
  RadarFrame frame = {};
  uint32_t frames = 0;
  uint32_t badFrames = 0;
};

} // namespace logic
