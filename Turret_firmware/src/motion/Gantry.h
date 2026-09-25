#pragma once

#include "Arduino.h"
#include "ServoChannel.h"
#include "Wing.h"
#include "settings/Settings.h"

class Gantry {
public:
  Gantry();
  // Reads settings and the Hall sensors. Attaches no servo.
  void Initialize(Settings &settings);
  // Reads the motion settings again (after a change from the web page or console).
  void ApplySettings();
  // Runs the attach schedule and the D6 hold policy.
  void Update(ulong deltaTime);
  void SetRotationX(int angle, bool force);
  void SetRotationZ(int angle, bool force);
  void OpenWings();
  void CloseWings();
  Wing& GetWingLeft();
  Wing& GetWingRight();

  // Boot step 11: rotations centred and guns retracted. The servos are attached
  // one at a time by Update(), in the order rotate Z, rotate X, gun left, gun right.
  void StartBoot();
  // Boot step 12: guns retracted, wings closed unless the Hall sensors say so.
  void Home();
  bool IsHoming();
  bool HasPendingAttach();
  void DetachAll();
  bool HasHallFault() const;

  // Servo outputs, in attach priority order (plan §3.2 step 11).
  static const uint8_t CHANNEL_COUNT = 6;
  ServoChannel &GetChannel(uint8_t index);

private:
  void RunAttachSchedule(ulong now);
  void ReleaseIdleRotation(ulong now);

  float X_AXIS_GEAR_RATIO = 75.0/17.0;
  float Z_AXIS_GEAR_RATIO = 30.0/15.0;
  float ANGLE_OFFSET_X = -3.4;
  float ANGLE_OFFSET_Z = 0;
  ulong servoStaggerMs = 250;
  ulong servoIdleMs = 5000;
  ulong lastAttachAt = 0;
  bool attachedOnce = false;

  Wing wingLeft;
  Wing wingRight;
  ServoChannel rotateX;
  ServoChannel rotateZ;
  Settings *settings;
};
