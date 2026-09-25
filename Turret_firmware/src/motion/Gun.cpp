#include "Gun.h"
#include "pins.h"

const int EXTEND_ANGLE = 160;
const ulong HOLD_MS = 500;

Gun::Gun(int servoPinIn)
    : servoPin(servoPinIn),
      channel(servoPinIn, servoPinIn == PIN_GUN_LEFT ? "gun left" : "gun right") {}

void Gun::Extend()
{
  if (servoPin == PIN_GUN_LEFT)
  {
    channel.SetAngle(EXTEND_ANGLE);
  }
  else
  {
    channel.SetAngle(180 - EXTEND_ANGLE);
  }
}

void Gun::Retract()
{
  channel.SetAngle(90);
}

void Gun::Update(ulong now)
{
  if (channel.IsAttached() && now - channel.LastActivityAt() >= HOLD_MS)
  {
    channel.Release();
  }
}

void Gun::Detach() { channel.Release(); }
