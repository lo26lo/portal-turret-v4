#pragma once

#include "Arduino.h"
#include "logic/RadarLogic.h"

#define TRACK_COUNT logic::RADAR_TARGETS

struct RadarTarget {
  int16_t x;
  int16_t y;
  int16_t previousX;
  int16_t previousY;
  int16_t speed;
  uint16_t resolution;
  bool available;
};

// HLK-LD2450 / Ai-Thinker RD-03D on UART1: frames decoded by
// logic::RadarParser (fixed length, tested natively), targets kept here.
class Radar {
public:
  // UART1 started (boot step 7, after the radar rail on the v2).
  void Initialize();
  // UART1 stopped and its TX line back to an input, before the radar rail is
  // switched off (v2: a TX line left high feeds the unpowered radar).
  void End();
  bool IsRunning() const { return running; }
  // millis() of the last Initialize(): the radar needs time to send frames.
  ulong GetStartedAt() const { return startedAt; }
  void Update(ulong deltaTime);
  const RadarTarget &GetTarget(uint8_t index) const;
  uint8_t GetTargetCount() const { return targetCount; }
  // True if a complete frame arrived less than maxAgeMs ago (boot step 7).
  bool IsAlive(ulong maxAgeMs) const;
  bool HasSeenFrame() const { return parser.Frames() > 0; }
  // Diagnostics (fault 4): bytes received, whole frames, frames dropped on a
  // bad footer. Bytes without frames = wrong baud rate or wrong protocol.
  uint32_t GetByteCount() const { return bytes; }
  uint32_t GetFrameCount() const { return parser.Frames(); }
  uint32_t GetBadFrameCount() const { return parser.BadFrames(); }
  // Detection zone (A2): in front of the radar, closer than maxDistanceMm and
  // within +-halfAngleDeg of its axis. x / y in mm, y = distance ahead.
  bool IsInZone(const RadarTarget &target, int32_t maxDistanceMm, int32_t halfAngleDeg) const;
  // Index of the first live target in the zone, -1 if none or if the radar is silent.
  int8_t FirstTargetInZone(int32_t maxDistanceMm, int32_t halfAngleDeg) const;

private:
  void TakeFrame(const logic::RadarFrame &frame);
  void ClearTargets();

  logic::RadarParser parser;
  RadarTarget targets[TRACK_COUNT] = {};
  uint8_t targetCount = 0;
  uint32_t bytes = 0;
  ulong lastFrameAt = 0;
  bool running = false;
  ulong startedAt = 0;
};
