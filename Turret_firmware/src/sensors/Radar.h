#pragma once

#include "Arduino.h"

#define TRACK_COUNT 3

struct RadarTarget {
  uint8_t id;
  int16_t x;
  int16_t y;
  int16_t previousX;
  int16_t previousY;
  int16_t speed;
  uint16_t resolution;
  bool available;
  bool isMoving;
};

class Radar {
public:
  void Initialize();
  void Update(ulong deltaTime);
  const RadarTarget& GetTarget(uint8_t index) const;
  uint8_t GetTargetCount();
  // True if a complete frame arrived less than maxAgeMs ago (boot step 7).
  bool IsAlive(ulong maxAgeMs) const;
  bool HasSeenFrame() const { return frameSeen; }
  // Detection zone (A2): in front of the radar, closer than maxDistanceMm and
  // within +-halfAngleDeg of its axis. x / y in mm, y = distance ahead.
  bool IsInZone(const RadarTarget &target, int32_t maxDistanceMm, int32_t halfAngleDeg) const;
  // Index of the first live target in the zone, -1 if none or if the radar is silent.
  int8_t FirstTargetInZone(int32_t maxDistanceMm, int32_t halfAngleDeg) const;

private:
  void UpdateSerialData();

  RadarTarget radarTargets[TRACK_COUNT];
  bool readingData = false;
  bool readingAck = false;

  uint8_t radarTargetCount;
  uint8_t previousRadarTargetCount;

  uint8_t writeIndex = 0;

  uint8_t headerIndex = 0;
  uint8_t footerIndex = 0;

  uint8_t messageHeader[4] = {0XAA, 0xFF, 0x03, 0x00};
  uint8_t messageFooter[2] = {0x55, 0xCC};

  uint8_t ackHeaderIndex = 0;
  uint8_t ackFooterIndex = 0;
  uint8_t ackHeader[4] = {0XFD, 0xFC, 0xFB, 0xFA};
  uint8_t ackFooter[4] = {0x04, 0x03, 0x02, 0x01};

  uint8_t messageBuffer[256];

  ulong lastSensorUpdateTime = 0;
  bool frameSeen = false;
};