#pragma once

#include "Arduino.h"
#include "Gun.h"
#include "ServoChannel.h"
#include "settings/Settings.h"

enum class WingPosition { Closed, Open, Unknown };

// A wing: continuous rotation servo + Hall sensor + its gun.
// D6: the wing servo is only attached while the wing moves.
class Wing {
public:
  Wing(int servoPin, int gunServoPin, int hallSensorPin);
  // Boot step 8: settings, then the Hall sensor gives the initial state. No servo.
  void Initialize(Settings &settings, bool left);
  // Reads HallOpen*, HallClose*, WingTrim* again.
  void ApplySettings();
  void Detach(); // wing and gun servos
  void Open();
  void Close();
  // Boot step 12: closes the wing unless the Hall sensor says it is closed.
  void Home();
  void Update(ulong deltaTime);
  // Calibration: runs the servo at its stop point with `trimUs` (RAM only, not
  // saved) for `ms`, then releases it. The wing should not move.
  void TestStop(int32_t trimUs, ulong ms);
  bool IsOpen();
  bool IsClosing();
  bool IsMoving() const { return isOpening || isClosing; }
  WingPosition ReadPosition();
  uint16_t ReadHall();
  // Last value read by Update(), safe to use from another task (web server).
  uint16_t GetLastHall() const { return lastHall; }
  // Hall sensor stuck at a rail, or no change during a whole movement.
  bool HasHallFault() const { return hallFault; }
  Gun& GetGun();
  ServoChannel &GetChannel() { return channel; }

private:
  void StartClosing();
  void Stop();
  bool IsPastOpen(uint16_t value) const;
  bool IsPastClosed(uint16_t value) const;

  bool isOpening = false;
  bool isClosing = false;
  bool isOpen = false;
  bool left = true;
  int servoPin;
  int hallSensorPin;
  ulong timeMoving = 0;
  ServoChannel channel;
  Gun gun;
  Settings *settings = nullptr;

  int32_t hallOpen = 2500;
  int32_t hallClosed = 1500;
  int32_t trimUs = 0;

  bool hallFault = false;
  uint16_t moveMin = 4095;
  uint16_t moveMax = 0;
  ulong railTime = 0;
  uint16_t lastHall = 0;
  bool stopTest = false;
  ulong stopTestUntil = 0;
};
