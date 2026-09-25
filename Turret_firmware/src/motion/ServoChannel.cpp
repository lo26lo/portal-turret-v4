#include "ServoChannel.h"

namespace {
const int MIN_US = 500;
const int MAX_US = 2400;
} // namespace

void ServoChannel::SetMicroseconds(int us) {
  targetUs = constrain(us, MIN_US, MAX_US);
  if (attached) {
    servo.writeMicroseconds(targetUs);
    lastActivityAt = millis();
  } else {
    pending = true;
  }
}

void ServoChannel::SetAngle(int degrees) {
  SetMicroseconds(map(constrain(degrees, 0, 180), 0, 180, MIN_US, MAX_US));
}

void ServoChannel::Release() {
  if (attached) {
    servo.detach();
  }
  attached = false;
  pending = false;
}

void ServoChannel::AttachNow() {
  servo.setPeriodHertz(50); // standard 50 hz servo
  servo.attach(pin, MIN_US, MAX_US);
  // ESP32Servo starts a freshly attached channel at 1500 us: write the stored
  // command straight away so that the servo goes to its last target.
  servo.writeMicroseconds(targetUs);
  attached = true;
  pending = false;
  lastActivityAt = millis();
}
