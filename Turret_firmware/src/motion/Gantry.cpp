#include "board/Power.h"
#include "board/Log.h"
#include "motion/Gantry.h"

#include "Arduino.h"
#include "board/Board.h"
#include "pins.h"

Gantry::Gantry()
    : wingLeft(PIN_WING_LEFT, PIN_GUN_LEFT, PIN_HALL_LEFT),
      wingRight(PIN_WING_RIGHT, PIN_GUN_RIGHT, PIN_HALL_RIGHT),
      rotateX(PIN_ROTATE_X, "rotate X"), rotateZ(PIN_ROTATE_Z, "rotate Z") {}

void Gantry::Initialize(Settings &settingsIn) {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  settings = &settingsIn;
  ApplySettings();

  wingLeft.Initialize(settingsIn, true);
  wingRight.Initialize(settingsIn, false);
}

void Gantry::ApplySettings() {
  ANGLE_OFFSET_X = settings->GetInt(SettingId::AngleOffsetX);
  ANGLE_OFFSET_Z = settings->GetInt(SettingId::AngleOffsetZ);
  servoStaggerMs = settings->GetInt(SettingId::ServoStagger);
  servoIdleMs = settings->GetInt(SettingId::ServoIdleMs);
  servoRampUs = settings->GetInt(SettingId::ServoRamp);
  ApplyRamp();
  wingLeft.ApplySettings();
  wingRight.ApplySettings();
}

void Gantry::SetRampActive(bool active) {
  if (active != rampActive) {
    rampActive = active;
    ApplyRamp();
  }
}

// Positional servos only: a ramp on a continuous wing would delay its stop on
// the Hall threshold.
void Gantry::ApplyRamp() {
  int32_t ramp = rampActive ? servoRampUs : 0;
  rotateZ.SetRamp(ramp);
  rotateX.SetRamp(ramp);
  wingLeft.GetGun().GetChannel().SetRamp(ramp);
  wingRight.GetGun().GetChannel().SetRamp(ramp);
}

ServoChannel &Gantry::GetChannel(uint8_t index) {
  switch (index) {
  case 0: return rotateZ;
  case 1: return rotateX;
  case 2: return wingLeft.GetGun().GetChannel();
  case 3: return wingRight.GetGun().GetChannel();
  case 4: return wingLeft.GetChannel();
  default: return wingRight.GetChannel();
  }
}

void Gantry::SetRotationX(int angle, bool force) {
  if (force || wingLeft.IsOpen() && wingRight.IsOpen()) {
    rotateX.SetAngle(round(90 + (angle + ANGLE_OFFSET_X) * X_AXIS_GEAR_RATIO));
  }
}

void Gantry::SetRotationZ(int angle, bool force) {
  if (force || wingLeft.IsOpen() && wingRight.IsOpen()) {
    rotateZ.SetAngle(round(90 + (angle + ANGLE_OFFSET_Z) * Z_AXIS_GEAR_RATIO));
  }
}

void Gantry::Update(ulong deltaTime) {
  ulong now = millis();
  RunAttachSchedule(now);
  rotateZ.Update(now);
  rotateX.Update(now);
  wingLeft.GetGun().GetChannel().Update(now);
  wingRight.GetGun().GetChannel().Update(now);
  wingLeft.Update(deltaTime);
  wingRight.Update(deltaTime);
  ReleaseIdleRotation(now);
  UpdateHallPower(now);
}

// At most one servo starts per ServoStagger window: several servos jumping to
// their target together draw 3 to 4 A, the limit of the eFuse (plan §4).
void Gantry::RunAttachSchedule(ulong now) {
  // v2: no pulse towards unpowered servos; the commands wait for the servo rail.
  if (!Power::IsOn(Rail::Servo)) {
    return;
  }
  ulong gap = staggerOverrideMs ? staggerOverrideMs : servoStaggerMs;
  if (attachedOnce && now - lastAttachAt < gap) {
    return;
  }
  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
    ServoChannel &channel = GetChannel(i);
    if (channel.IsAttachPending()) {
      channel.AttachNow();
      lastAttachAt = now;
      attachedOnce = true;
      Log.printf("Servo %s attached (%d us)\n", channel.GetName(), channel.GetMicroseconds());
      Board::Mark(channel.GetName()); // L1 lab marker
      return;
    }
  }
}

void Gantry::UpdateHallPower(ulong now) {
  const ulong HALL_OFF_DELAY_MS = 2000;
  if (!Power::IsSwitchable()) {
    return;
  }
  if (wingLeft.IsMoving() || wingRight.IsMoving()) {
    hallNeededAt = now;
    return;
  }
  if (hallPower == HallPower::Always) {
    hallNeededAt = now;
    Power::On(Rail::Hall); // nothing if already on
    return;
  }
  long idle = (long)(now - hallNeededAt);
  if (Power::IsOn(Rail::Hall) && (hallPower == HallPower::Off || idle >= (long)HALL_OFF_DELAY_MS)) {
    Power::Off(Rail::Hall);
  }
}

// D6: rotations are held while the wings are open (aiming). Wings closed and
// still for ServoIdleMs: released, re-attached on their last command when needed.
void Gantry::ReleaseIdleRotation(ulong now) {
  if (servoIdleMs == 0) {
    return;
  }
  if (wingLeft.IsOpen() || wingRight.IsOpen() || wingLeft.IsMoving() || wingRight.IsMoving()) {
    return;
  }
  // Signed difference: "now" is read at the start of Update(), before
  // RunAttachSchedule(), so a servo attached in this very loop has a later
  // activity time. Unsigned, the difference wrapped around to ~49 days and the
  // servo was released at once (seen on 23.10.2026: "pas de 5 degrés").
  if (rotateX.IsAttached() && (long)(now - rotateX.LastActivityAt()) >= (long)servoIdleMs) {
    rotateX.Release("idle, ServoIdleMs");
  }
  if (rotateZ.IsAttached() && (long)(now - rotateZ.LastActivityAt()) >= (long)servoIdleMs) {
    rotateZ.Release("idle, ServoIdleMs");
  }
}

void Gantry::StartBoot() {
  SetRotationZ(0, true);
  SetRotationX(0, true);
  wingLeft.GetGun().Retract();
  wingRight.GetGun().Retract();
}

void Gantry::Home() {
  wingLeft.GetGun().Retract();
  wingRight.GetGun().Retract();
  wingLeft.Home();
  wingRight.Home();
}

bool Gantry::IsHoming() {
  return wingLeft.IsClosing() || wingRight.IsClosing();
}

bool Gantry::HasPendingAttach() {
  for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
    if (GetChannel(i).IsAttachPending()) {
      return true;
    }
  }
  return false;
}

void Gantry::DetachAll() {
  rotateX.Release("detach all");
  rotateZ.Release("detach all");
  wingLeft.Detach();
  wingRight.Detach();
}

bool Gantry::HasHallFault() const {
  return wingLeft.HasHallFault() || wingRight.HasHallFault();
}

void Gantry::OpenWings() {
  wingLeft.Open();
  wingRight.Open();
}

Wing& Gantry::GetWingLeft() { return wingLeft; }

Wing& Gantry::GetWingRight() { return wingRight; }

void Gantry::CloseWings() {
  SetRotationX(0, true);
  SetRotationZ(0, true);
  wingLeft.Close();
  wingRight.Close();
}
