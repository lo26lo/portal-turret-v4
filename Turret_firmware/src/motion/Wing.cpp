#include "board/Log.h"
#include "Wing.h"
#include "Arduino.h"
#include "pins.h"

// Continuous rotation servos stop at 1500 us + trim (WingTrimL / WingTrimR).
#define STOP_US 1500
// Speed offset from the stop point. The V4 used write(90 +- 45) with a
// 500-2400 us range, i.e. +-475 us around 1450 us.
#define SPEED_US 475
#define MOVE_TIMEOUT_MS 2000
// Hall sensor diagnostics (plan §7 lot 8).
#define HALL_RAIL_LOW 15
#define HALL_RAIL_HIGH 4080
#define HALL_RAIL_MS 1000   // at a rail for this long = unplugged or shorted
#define HALL_MIN_SWING 150  // less change than this during a movement = stuck

Wing::Wing(int servoPinIn, int gunServoPinIn, int hallSensorPinIn)
    : servoPin(servoPinIn), hallSensorPin(hallSensorPinIn),
      channel(servoPinIn, servoPinIn == PIN_WING_LEFT ? "wing left" : "wing right"),
      gun(gunServoPinIn) {}

void Wing::Initialize(Settings &settingsIn, bool leftIn) {
  settings = &settingsIn;
  left = leftIn;
  ApplySettings();
  // A reset in the middle of a cycle can leave the wing open: do not assume closed.
  isOpen = ReadPosition() == WingPosition::Open;
}

void Wing::ApplySettings() {
  if (settings == nullptr) {
    return;
  }
  hallOpen = settings->GetInt(left ? SettingId::HallOpenL : SettingId::HallOpenR);
  hallClosed = settings->GetInt(left ? SettingId::HallCloseL : SettingId::HallCloseR);
  trimUs = settings->GetInt(left ? SettingId::WingTrimL : SettingId::WingTrimR);
}

// The magnet can be mounted either way round: if the "open" threshold is
// below the "closed" one, the comparisons are reversed.
bool Wing::IsPastOpen(uint16_t value) const {
  return hallOpen >= hallClosed ? value >= hallOpen : value <= hallOpen;
}

bool Wing::IsPastClosed(uint16_t value) const {
  return hallOpen >= hallClosed ? value <= hallClosed : value >= hallClosed;
}

uint16_t Wing::ReadHall() {
  lastHall = analogRead(hallSensorPin);
  return lastHall;
}

WingPosition Wing::ReadPosition() {
  uint16_t hallValue = ReadHall();
  if (IsPastOpen(hallValue)) {
    return WingPosition::Open;
  }
  if (IsPastClosed(hallValue)) {
    return WingPosition::Closed;
  }
  return WingPosition::Unknown;
}

void Wing::Detach() {
  channel.Release();
  gun.Detach();
  isOpening = false;
  isClosing = false;
}

void Wing::Stop() {
  // D6: no pulse = the continuous servo stops dead, no creeping from a bad neutral.
  channel.Release();
}

void Wing::Open() {
  if (isOpen) {
    return;
  }
  Log.println("Opening Wing");
  isOpening = true;
  isClosing = false;
  timeMoving = 0;
  moveMin = 4095;
  moveMax = 0;
  channel.SetMicroseconds(STOP_US + trimUs + (left ? SPEED_US : -SPEED_US));
}

void Wing::Close() {
  if (!isOpen) {
    return;
  }
  StartClosing();
}

void Wing::Home() {
  if (ReadPosition() == WingPosition::Closed) {
    isOpen = false;
    return;
  }
  Log.println("Homing Wing");
  StartClosing();
}

void Wing::StartClosing() {
  Log.println("Closing Wing");
  isOpening = false;
  isClosing = true;
  isOpen = false;
  timeMoving = 0;
  moveMin = 4095;
  moveMax = 0;
  channel.SetMicroseconds(STOP_US + trimUs + (left ? -SPEED_US : SPEED_US));
}

void Wing::TestStop(int32_t trim, ulong ms) {
  isOpening = false;
  isClosing = false;
  trimUs = constrain(trim, -200, 200);
  channel.SetMicroseconds(STOP_US + trimUs);
  stopTest = true;
  stopTestUntil = millis() + ms;
}

Gun &Wing::GetGun() { return gun; }

void Wing::Update(ulong deltaTime) {
  uint16_t hallValue = ReadHall();
  gun.Update(millis());

  // Rail check at all times.
  if (hallValue <= HALL_RAIL_LOW || hallValue >= HALL_RAIL_HIGH) {
    railTime += deltaTime;
    if (railTime >= HALL_RAIL_MS && !hallFault) {
      hallFault = true;
      Log.printf("Wing %s: Hall sensor at a rail (%u)\n", left ? "left" : "right", hallValue);
    }
  } else {
    railTime = 0;
  }

  if (stopTest && (long)(millis() - stopTestUntil) >= 0) {
    stopTest = false;
    Stop();
  }

  if (!isOpening && !isClosing) {
    return;
  }
  // The timeout only runs once the servo is actually attached (it may wait
  // for its turn in the attach schedule).
  if (!channel.IsAttached()) {
    return;
  }

  moveMin = min(moveMin, hallValue);
  moveMax = max(moveMax, hallValue);
  timeMoving += deltaTime;

  if (isOpening && IsPastOpen(hallValue)) {
    Log.println("Wing Is Open");
    isOpening = false;
    isOpen = true;
    hallFault = false;
    Stop();
    return;
  }

  if (isClosing && IsPastClosed(hallValue)) {
    Log.println("Wing Is Closed");
    isClosing = false;
    hallFault = false;
    Stop();
    return;
  }

  if (timeMoving >= MOVE_TIMEOUT_MS) {
    Log.println("Wing Movement Timeout");
    if (moveMax - moveMin < HALL_MIN_SWING) {
      hallFault = true;
      Log.printf("Wing %s: Hall sensor did not change during the movement (%u..%u)\n",
                    left ? "left" : "right", moveMin, moveMax);
    }
    if (isOpening) {
      isOpen = true;
    }
    isOpening = false;
    isClosing = false;
    Stop();
  }
}

bool Wing::IsOpen() { return isOpen; }
bool Wing::IsClosing() { return isClosing; }
