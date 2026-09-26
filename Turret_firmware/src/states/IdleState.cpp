#include "board/Log.h"
#include "states/IdleState.h"
#include "StateMachine.h"
#include "sensors/Radar.h"

void IdleState::OnActivate() {
  Log.println("IdleState");
  BaseState::OnActivate();
  enteredAt = millis();
}

void IdleState::Update(ulong deltaTime) {
  // Never deploy the wings of a turret lying on its side (plan §3.2 step 6).
  if (!turret->motion.CanDeploy()) {
    return;
  }
  // A2: rest after every cycle (and after boot), so that someone standing
  // still in front of the turret does not make it fire in a loop.
  Settings &settings = turret->settings;
  if (millis() - enteredAt < (ulong)settings.GetInt(SettingId::CooldownMs)) {
    return;
  }
  // A1 + A2: only a live radar, only a target inside the detection zone.
  int8_t target = turret->radar.FirstTargetInZone(settings.GetInt(SettingId::DetectMaxMm),
                                                  settings.GetInt(SettingId::DetectAngle));
  if (target >= 0) {
    const RadarTarget &t = turret->radar.GetTarget(target);
    Log.printf("Target %d at x %d mm, y %d mm\n", target, t.x, t.y);
    stateMachine->GoToState(StateId::Activate);
  }
}
