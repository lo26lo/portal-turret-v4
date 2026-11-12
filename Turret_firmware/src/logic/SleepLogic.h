#pragma once

// Pure sleep logic (lot F2.6), without Arduino: used by control/Sleep and the
// native tests (test/test_logic). Modes: docs/carte-v2-plan.md §1.

#include <stdint.h>

namespace logic {

// SleepMode setting, in the order of its options.
enum class SleepMode : uint8_t { Never = 0, RadarOnly = 1, RadarThenDeep = 2 };
// SleepWifi setting: Wi-Fi during the radar sleep and the patrols.
enum class SleepWifi : uint8_t { Unchanged = 0, StationOnly = 1, Off = 2 };

enum class SleepLevel : uint8_t { Awake, Radar, Deep };

// True if hour (0..23) is in the night window [start, end), which may cross
// midnight (23 -> 7). start == end: no night. hour < 0 (time unknown): false.
inline bool InNightWindow(int hour, int start, int end) {
  if (hour < 0 || start == end) {
    return false;
  }
  if (start < end) {
    return hour >= start && hour < end;
  }
  return hour >= start || hour < end;
}

// Seconds from secondOfDay (0..86399) to the next hour:00:00 (1..86400).
inline uint32_t SecondsUntilHour(uint32_t secondOfDay, int hour) {
  uint32_t target = (uint32_t)hour * 3600u;
  return target > secondOfDay ? target - secondOfDay : 86400u - secondOfDay + target;
}

struct SleepInputs {
  SleepMode mode = SleepMode::Never;
  uint32_t idleMs = 0;            // since the last activity (target, cycle, key)
  uint32_t inRadarSleepMs = 0;    // time spent in the radar sleep (current == Radar)
  uint32_t radarSleepAfterMs = 0; // RadarSleepMin; 0 = never on its own
  uint32_t deepSleepAfterMs = 0;  // DeepSleepMin; 0 = never on its own
  bool night = false;             // NightSleep on and the clock in the window
  bool deepAllowed = false;       // v2 board, normal mode (SW1 open)
};

// Level the turret should be in, one step at a time (Awake -> Radar -> Deep).
// The radar sleep always comes first, after the idle delay; at night the deep
// sleep follows at once, otherwise after DeepSleepMin of radar sleep.
inline SleepLevel WantedSleepLevel(const SleepInputs &in, SleepLevel current) {
  if (in.mode == SleepMode::Never) {
    return SleepLevel::Awake;
  }
  if (current == SleepLevel::Awake) {
    bool idle = in.radarSleepAfterMs > 0 && in.idleMs >= in.radarSleepAfterMs;
    return idle ? SleepLevel::Radar : SleepLevel::Awake;
  }
  if (current == SleepLevel::Radar && in.mode == SleepMode::RadarThenDeep && in.deepAllowed) {
    bool longEnough = in.deepSleepAfterMs > 0 && in.inRadarSleepMs >= in.deepSleepAfterMs;
    if (longEnough || in.night) {
      return SleepLevel::Deep;
    }
  }
  return current;
}

// Wake-up threshold of the LSM6DSOX (WK_THS, 6 bits, 1 LSB = full scale / 64,
// 31.25 mg at +-2 g) for the WakeImuLevel setting: 0 low, 1 medium, 2 high
// sensitivity.
inline uint8_t ImuWakeThreshold(int32_t level) {
  switch (level) {
  case 0: return 12; // 375 mg: a firm knock
  case 2: return 2;  // 62 mg: a light touch
  default: return 5; // 156 mg
  }
}

} // namespace logic
