#include "ServoChannel.h"

#include "board/Log.h"
#include "logic/ServoLogic.h"

namespace {
const int MIN_US = 500;
const int MAX_US = 2400;
const ulong RAMP_PERIOD_MS = 20; // one step per servo period
} // namespace

void ServoChannel::Send(int us, ulong now) {
  servo.writeMicroseconds(us);
  sentUs = us;
  lastActivityAt = now;
}

void ServoChannel::SetMicroseconds(int us) {
  targetUs = constrain(us, MIN_US, MAX_US);
  if (attached) {
    ulong now = millis();
    if (rampUsPerSecond <= 0 || sentUs < 0) {
      Send(targetUs, now);
    } else {
      lastActivityAt = now; // Update() moves the pulse
    }
  } else {
    pending = true;
  }
}

void ServoChannel::SetAngle(int degrees) {
  SetMicroseconds(map(constrain(degrees, 0, 180), 0, 180, MIN_US, MAX_US));
}

void ServoChannel::Release(const char *reason) {
  if (attached) {
    servo.detach();
    Log.printf("Servo %s released (%s)\n", name, reason);
  }
  attached = false;
  pending = false;
}

void ServoChannel::AttachNow() {
  servo.setPeriodHertz(50); // standard 50 hz servo
  servo.attach(pin, MIN_US, MAX_US);
  // ESP32Servo starts a freshly attached channel at 1500 us: write a pulse
  // straight away. With a ramp, start where the servo was left (if known) and
  // let Update() go to the command; otherwise go to the command.
  ulong now = millis();
  bool ramp = rampUsPerSecond > 0 && sentUs >= 0;
  Send(ramp ? sentUs : targetUs, now);
  lastStepAt = now;
  attached = true;
  pending = false;
}

void ServoChannel::Update(ulong now) {
  // Signed: lastStepAt may be a little later than "now" (attached in this loop).
  long elapsed = (long)(now - lastStepAt);
  if (!attached || sentUs == targetUs || elapsed < (long)RAMP_PERIOD_MS) {
    return;
  }
  int next = logic::RampStep(sentUs, targetUs, rampUsPerSecond, (uint32_t)elapsed);
  lastStepAt = now;
  Send(next, now);
}
