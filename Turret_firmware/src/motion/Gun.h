#pragma once

#include "Arduino.h"
#include "ServoChannel.h"

class Gun {
public:
  Gun(int servoPin);
  void Extend();
  void Retract();
  // D6: detached ~500 ms after the last command (nothing loads the gun).
  void Update(ulong now);
  void Detach();
  ServoChannel &GetChannel() { return channel; }

private:
  int servoPin;
  ServoChannel channel;
};
