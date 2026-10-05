#pragma once

#include <Arduino.h>

#include "Turret.h"

class Actions;
class StateMachine;

// Self-test (lot IM): one command that checks the board and gives a report,
// for the first power-up and whenever something looks wrong.
//  - quick: passive checks only, immediate (nothing moves, no sound);
//  - full: then LEDs, sound, each servo with PWR_FLT watched, and the wings
//    with their Hall sensors. Runs step by step from loop(), never blocks.
// Results: OK, FAIL, SKIP (could not be tested), CHECK (the firmware cannot
// see or hear: look at the LEDs, listen to the tone).
class SelfTest {
public:
  enum class Result : uint8_t { Pending, Ok, Fail, Skip, Check };

  static const uint8_t ENTRY_COUNT = 19;

  void Initialize(Turret &turret, StateMachine &stateMachine, Actions &actions);
  // Starts a test and returns a one-line answer. `displayPresent`: an OLED answered at boot.
  String Start(bool quick, bool displayPresent);
  void Stop();
  void Update(ulong now);

  bool IsRunning() const { return running; }
  bool HasRun() const { return hasRun; }
  // One line per entry: "OK    IMU", "FAIL  radar"...
  String Report(bool french) const;
  // "12 OK 1 FAIL 2 CHECK 4 SKIP"
  String Summary(bool french) const;
  uint8_t Count(Result result) const;
  // Name of the step in progress, or of the n-th entry with this result ("" if none).
  const char *CurrentName(bool french) const;
  const char *NameWith(Result result, uint8_t ordinal, bool french) const;
  static const char *ResultLabel(Result result, bool french);

private:
  void RunPassiveChecks(bool displayPresent);
  void Set(uint8_t entry, Result result) { results[entry] = result; }
  void Next(ulong now);
  void Finish(const char *reason);
  String Run(const char *command);

  Turret *turret = nullptr;
  StateMachine *stateMachine = nullptr;
  Actions *actions = nullptr;

  Result results[ENTRY_COUNT] = {};
  bool running = false;
  bool hasRun = false;
  uint8_t entry = 0; // active entry being tested
  uint8_t sub = 0;   // step inside the entry
  ulong waitUntil = 0;
  ulong stepStarted = 0;
  uint32_t faultsAtStart = 0;
  bool wingFailed = false;
};
