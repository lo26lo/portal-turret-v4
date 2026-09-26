#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "logic/BoardLogic.h"

// Turret2 board services (docs/firmware-plan.md §3.2 step 1, §4, D1, D3, D4, D5):
// safe state of the outputs, reset reason, reboot-loop detection, status LEDs,
// buttons, SW1 (bench mode) and the eFuse fault line.

// Red LED blink codes (D4). When several faults are active, the lowest number wins.
enum class Fault : uint8_t {
  BrownoutLoop = 1, // brownout or reboot loop (reduced mode)
  EFuse = 2,        // PWR_FLT went low
  Imu = 3,          // IMU missing
  Radar = 4,        // no radar frame
  Hall = 5,         // Hall sensor inconsistent
  LittleFs = 6,     // filesystem not mounted
};

// None, ShortPress (released before the long press delay), LongPress (held
// for the long press delay, sent once). Logic in logic/BoardLogic.h, tested natively.
using ButtonEvent = logic::PressEvent;

class Button {
public:
  explicit Button(uint8_t pin);
  void Initialize();
  void Update(ulong now);
  bool IsDown() const { return debouncer.IsDown(); }
  // Returns the pending event and clears it.
  ButtonEvent TakeEvent();

private:
  uint8_t pin;
  logic::Debouncer debouncer;
  ButtonEvent pending = ButtonEvent::None;
};

class Board {
public:
  Board();

  // First call of setup(), before anything else: takes control of every
  // output that floats after reset. No Serial output here (not started yet).
  void Begin();
  // Once Serial is up: reset reason, SW1, PWR_FLT, flash / PSRAM, counters.
  void PrintBanner();
  // End of the boot sequence: green LED to heartbeat, red LED to fault codes.
  void BootDone();
  void Update(ulong deltaTime);

  bool IsBenchMode() const { return benchMode; }             // D1, SW1 closed at boot
  bool IsFactoryResetRequested() const { return factoryReset; } // D3, A + B held at boot
  bool IsReducedMode() const { return reducedMode; }         // D5, reboot loop detected
  esp_reset_reason_t GetResetReason() const { return resetReason; }
  static const char *ResetReasonName(esp_reset_reason_t reason);

  // eFuse fault line (IO38): current level and falling edges seen since the last call.
  bool IsPowerFault() const;
  uint32_t TakePowerFaultEdges();
  // True once per power fault seen since the last call, even if PWR_FLT was
  // low for a moment only (falling edge caught by the interrupt).
  bool TakePowerFaultEvent();
  uint32_t GetPowerFaultCount() const { return powerFaultCount; }
  uint32_t GetBootLoopCount() const { return bootLoopCount; }
  uint32_t GetBrownoutCount() const { return brownoutCount; }
  uint8_t GetFaults() const { return faults; } // bit n-1 = Fault n

  void SetFault(Fault fault, bool active);
  bool HasFault(Fault fault) const;

  // L1 (lab aid): with LabMarkers on, the green LED no longer shows the
  // heartbeat; it toggles at each boot step and each servo attachment, to line
  // up an oscilloscope or PPK2 trace with the code. The label goes to the log.
  void SetLabMarkers(bool enabled);
  bool HasLabMarkers() const { return labMarkers; }
  static void Mark(const char *label);

  Button buttonA;
  Button buttonB;

private:
  void UpdateGreenLed(ulong now);
  void UpdateRedLed(ulong now);
  void UpdateBootLoopCounter(ulong now);

  Preferences prefs;
  bool prefsReady = false;

  esp_reset_reason_t resetReason = ESP_RST_UNKNOWN;
  bool benchMode = false;
  bool factoryReset = false;
  bool reducedMode = false;
  bool bootDone = false;
  bool bootLoopCleared = false;
  uint32_t bootLoopCount = 0;
  uint32_t brownoutCount = 0;
  uint32_t powerFaultCount = 0;
  bool powerFaultEvent = false;

  bool labMarkers = false;
  bool markerLevel = false;

  uint8_t faults = 0; // bit n-1 = Fault n
  uint8_t redCode = 0;
  uint8_t redStep = 0;
  ulong redStepAt = 0;
};
