#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>

// One servo output with deferred attachment (D6, plan §4).
// A command on a detached servo is stored and the servo asks to be attached;
// Gantry attaches waiting servos one at a time (ServoStagger apart), directly
// on the stored command. Release() stops the pulses: continuous servos stop,
// positional ones go limp.
class ServoChannel {
public:
  ServoChannel(uint8_t pin, const char *name) : pin(pin), name(name) {}

  void SetMicroseconds(int us);
  // Same mapping as Servo::write() with a 500-2400 us range.
  void SetAngle(int degrees);
  void Release();

  bool IsAttached() const { return attached; }
  bool IsAttachPending() const { return pending; }
  // Called by the Gantry scheduler only.
  void AttachNow();
  // Last command sent to the servo while attached, or attachment time.
  ulong LastActivityAt() const { return lastActivityAt; }
  int GetMicroseconds() const { return targetUs; }
  const char *GetName() const { return name; }

private:
  uint8_t pin;
  const char *name;
  Servo servo;
  int targetUs = 1500;
  bool attached = false;
  bool pending = false;
  ulong lastActivityAt = 0;
};
