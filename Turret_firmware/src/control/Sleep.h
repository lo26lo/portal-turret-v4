#pragma once

#include <Arduino.h>

#include "logic/SleepLogic.h"

struct Turret;
class StateMachine;
class Actions;
class AccessPoint;
class Station;

// Sleep modes (lot F2.6, docs/carte-v2-plan.md §1), all set from the web page
// (Settings tab, group "Sleep"):
//  - radar sleep: after RadarSleepMin without a target, from Idle only. The
//    radar stays on; servos released, LEDs off, amplifier in shutdown, Hall
//    sensors off, WiFi as SleepWifi says; v2: servo and LED rails off. Wakes
//    up on a target in the detection zone, the knob, PROG, a cycle started
//    from the web page, or "sleep off";
//  - deep sleep (v2, SW1 open): after DeepSleepMin of radar sleep, or at night
//    (NightSleep, NightStart..NightEnd, needs the time). Every rail off, ESP32
//    in deep sleep. Wakes up on the IMU (WakeImu: the turret is moved), PROG,
//    the end of the night, or a patrol: every PatrolPeriodS the turret boots
//    "quiet" (rails off but the radar's, no homing), looks for PatrolOnS, and
//    goes back to deep sleep if nobody is there. A boot after a deep sleep is
//    a normal boot otherwise (the RAM is lost).
class Sleep {
public:
  // setup(), right after the board check: reads why the ESP32 woke up. True
  // when this boot is a patrol: setup() then leaves the rails off (radar
  // apart), and BootState neither attaches nor homes the servos.
  static bool BeginBoot();
  static bool IsQuietBoot();

  // setup(), end of step 10.
  void Initialize(Turret &turret, StateMachine &stateMachine, Actions &actions, AccessPoint &accessPoint,
                  Station &station);
  // loop(), before the state machine: wakes up before a state reacts.
  void Update(ulong now);
  // Knob, PROG, buttons: resets the idle time and wakes the radar sleep up.
  void NoteActivity();

  logic::SleepLevel GetLevel() const { return level; }
  bool IsAsleep() const { return level != logic::SleepLevel::Awake; }
  // Console / web: "sleep" (report), "sleep radar", "sleep deep", "sleep off".
  String Command(const String &args);
  // /api/status: {"level":..,"mode":..,"idleS":..,"nextS":..}.
  String StatusJson() const;

private:
  void EnterRadarSleep(const char *why);
  void ApplyRadarSleep();
  void WakeUp(const char *why);
  // Never returns once the ESP32 sleeps; false (and why) if it may not.
  bool EnterDeepSleep(const char *why, String &refused);
  bool DeepAllowed(String &refused) const;
  bool TargetInZone() const;
  bool IsNight() const;
  logic::SleepInputs Inputs(ulong now) const;

  Turret *turret = nullptr;
  StateMachine *stateMachine = nullptr;
  Actions *actions = nullptr;
  AccessPoint *accessPoint = nullptr;
  Station *station = nullptr;

  logic::SleepLevel level = logic::SleepLevel::Awake;
  ulong lastActivity = 0;
  ulong radarSleepSince = 0;
  bool patrol = false; // quiet boot: back to deep sleep at patrolUntil
  ulong patrolUntil = 0;
  bool apWasOn = false; // WiFi to restore when waking up
  bool staWasOn = false;
  bool wifiChanged = false;
};
