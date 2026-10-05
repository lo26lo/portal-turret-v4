#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Usage counters (improvement C6): boots and firing cycles kept in NVS,
// plus what happened since this boot. Shown on the debug screen, the web
// page and with the "stats" command.
class Stats {
public:
  // Boot: reads the counters and counts this boot.
  void Begin();
  // A firing cycle started (state Firing entered).
  void CountCycle();
  // A new target appeared on the radar.
  void CountTarget();

  uint32_t GetBoots() const { return boots; }
  uint32_t GetTotalCycles() const { return totalCycles; }
  uint32_t GetSessionCycles() const { return sessionCycles; }
  uint32_t GetSessionTargets() const { return sessionTargets; }
  // millis() of the last cycle, 0 if none since boot.
  ulong GetLastCycleAt() const { return lastCycleAt; }

private:
  Preferences prefs;
  bool ready = false;
  uint32_t boots = 0;
  uint32_t totalCycles = 0;
  uint32_t sessionCycles = 0;
  uint32_t sessionTargets = 0;
  ulong lastCycleAt = 0;
};
