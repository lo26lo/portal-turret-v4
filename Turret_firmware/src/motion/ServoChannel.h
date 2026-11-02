#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>

// One servo output with deferred attachment (D6, plan §4).
// A command on a detached servo is stored and the servo asks to be attached;
// Gantry attaches waiting servos one at a time (ServoStagger apart), directly
// on the stored command. Release() stops the pulses: continuous servos stop,
// positional ones go limp.
//
// Speed ramp (setting ServoRamp, positional servos on a weak supply only):
// the pulse sent follows the command at a limited speed instead of jumping,
// which keeps the start current of the motor low. Update() moves it.
class ServoChannel {
public:
  ServoChannel(uint8_t pin, const char *name) : pin(pin), name(name) {}

  void SetMicroseconds(int us);
  // Same mapping as Servo::write() with a 500-2400 us range.
  void SetAngle(int degrees);
  // reason: logged with the release (who stopped the pulses).
  void Release(const char *reason = "");

  bool IsAttached() const { return attached; }
  bool IsAttachPending() const { return pending; }
  // Called by the Gantry scheduler only.
  void AttachNow();
  // Ramp speed in us per second, 0 = none (every command is sent at once).
  void SetRamp(int32_t usPerSecond) { rampUsPerSecond = usPerSecond; }
  // Moves the pulse towards the command while a ramp is set.
  void Update(ulong now);
  // Last command sent to the servo while attached, or attachment time.
  ulong LastActivityAt() const { return lastActivityAt; }
  // The command (target), and the pulse really sent (-1 before the first one).
  int GetMicroseconds() const { return targetUs; }
  int GetSentMicroseconds() const { return sentUs; }
  const char *GetName() const { return name; }

private:
  void Send(int us, ulong now);

  uint8_t pin;
  const char *name;
  Servo servo;
  int targetUs = 1500;
  int sentUs = -1; // kept across Release(): where the servo was left
  int32_t rampUsPerSecond = 0;
  bool attached = false;
  bool pending = false;
  ulong lastActivityAt = 0;
  ulong lastStepAt = 0;
};
