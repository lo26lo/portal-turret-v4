#include "FiringState.h"
#include "StateMachine.h"
#include "logic/CycleLogic.h"

namespace {
// Rotation Z bound while aiming (the radar sees +-60 deg).
const float AIM_MAX_DEG = 60.0f;
const float AIM_ALPHA = 0.25f; // share of the way to the target per loop
} // namespace

void FiringState::OnActivate() {
  BaseState::OnActivate();
  startedAt = millis();
  turret->audio.ShootAudio.Begin();
}

void FiringState::OnDeactivate() {
  BaseState::OnDeactivate();
  turret->audio.ShootAudio.Stop(); // leaves the loop, the tail plays out
}

// B1: rotation Z follows the first target in the zone (AimZ on; the wings are
// open, so Gantry accepts the command). Shared with Searching.
void FiringState::Aim(Turret &turret, float &aimDeg) {
  Settings &settings = turret.settings;
  if (!settings.GetBool(SettingId::AimZ)) {
    return;
  }
  int8_t index = turret.radar.FirstTargetInZone(settings.GetInt(SettingId::DetectMaxMm),
                                                 settings.GetInt(SettingId::DetectAngle));
  if (index < 0) {
    return;
  }
  const RadarTarget &target = turret.radar.GetTarget(index);
  aimDeg = logic::SmoothAim(aimDeg, logic::AimAngleDeg(target.x, target.y, AIM_MAX_DEG), AIM_ALPHA);
  turret.gantry.SetRotationZ((int)lroundf(aimDeg), false);
}

void FiringState::Update(ulong deltaTime) {
  Aim(*turret, aimDeg);
  if (logic::FiringDone(millis(), startedAt, turret->settings.GetInt(SettingId::FireMs))) {
    stateMachine->GoToState(StateId::Searching);
  }
}
