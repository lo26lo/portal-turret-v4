#include "board/Log.h"
#include "Radar.h"
#include "pins.h"

namespace {
// No complete frame for this long: the targets in memory are stale (radar
// unplugged or hung). The LD2450 (and the RD-03D) send about 10 frames per second.
const ulong STALE_MS = 1000;
} // namespace

void Radar::Initialize() {
  Serial1.begin(256000, SERIAL_8N1, PIN_RADAR_RX, PIN_RADAR_TX);
  running = true;
  startedAt = millis();
}

void Radar::End() {
  if (!running) {
    return;
  }
  Serial1.end();
  pinMode(PIN_RADAR_TX, INPUT);
  running = false;
  parser.Reset();
  ClearTargets();
}

void Radar::ClearTargets() {
  for (uint8_t i = 0; i < TRACK_COUNT; i++) {
    targets[i].available = false;
  }
  targetCount = 0;
}

void Radar::Update(ulong deltaTime) {
  if (!running) {
    return;
  }
  while (Serial1.available()) {
    bytes++;
    if (parser.Feed((uint8_t)Serial1.read())) {
      TakeFrame(parser.Frame());
    }
  }
  // A1: a silent radar must not keep its last target alive forever.
  if (targetCount > 0 && millis() - lastFrameAt > STALE_MS) {
    ClearTargets();
    Log.println("Radar: no frame for 1 s, targets cleared");
  }
}

void Radar::TakeFrame(const logic::RadarFrame &frame) {
  uint8_t previousCount = targetCount;
  targetCount = 0;
  for (uint8_t i = 0; i < TRACK_COUNT; i++) {
    const logic::RadarFrameTarget &in = frame.target[i];
    RadarTarget &out = targets[i];
    out.available = in.present;
    if (!in.present) {
      continue;
    }
    out.previousX = out.x;
    out.previousY = out.y;
    out.x = in.x;
    out.y = in.y;
    out.speed = in.speed;
    out.resolution = in.resolution;
    targetCount++;
  }
  lastFrameAt = millis();
  if (targetCount != previousCount) {
    Log.printf("Radar: %u target(s)\n", targetCount);
  }
}

bool Radar::IsInZone(const RadarTarget &target, int32_t maxDistanceMm, int32_t halfAngleDeg) const {
  return target.available && logic::IsInZone(target.x, target.y, maxDistanceMm, halfAngleDeg);
}

int8_t Radar::FirstTargetInZone(int32_t maxDistanceMm, int32_t halfAngleDeg) const {
  if (!IsAlive(STALE_MS)) {
    return -1;
  }
  for (uint8_t i = 0; i < TRACK_COUNT; i++) {
    if (IsInZone(targets[i], maxDistanceMm, halfAngleDeg)) {
      return i;
    }
  }
  return -1;
}

const RadarTarget &Radar::GetTarget(uint8_t index) const { return targets[index % TRACK_COUNT]; }

bool Radar::IsAlive(ulong maxAgeMs) const { return HasSeenFrame() && millis() - lastFrameAt < maxAgeMs; }
