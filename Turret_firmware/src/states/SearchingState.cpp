#include "SearchingState.h"
#include "FiringState.h"
#include "StateMachine.h"
#include "logic/CycleLogic.h"

namespace {
const uint32_t SWEEP_PERIOD_MS = 4000; // one left-right-left round
const float SWEEP_DEG = 30.0f;
} // namespace

void SearchingState::OnActivate() {
  BaseState::OnActivate();
  startedAt = millis();
  aimDeg = 0;
  turret->audio.PlayVoice(Voice::Search);
}

void SearchingState::Update(ulong deltaTime) {
  Settings &settings = turret->settings;
  ulong now = millis();
  bool target = turret->radar.FirstTargetInZone(settings.GetInt(SettingId::DetectMaxMm),
                                                settings.GetInt(SettingId::DetectAngle)) >= 0;
  switch (logic::SearchDecision(target, now, startedAt, settings.GetInt(SettingId::SearchMs))) {
  case logic::SearchStep::Fire:
    stateMachine->GoToState(StateId::FiringState);
    return;
  case logic::SearchStep::GiveUp:
    stateMachine->GoToState(StateId::Disengage);
    return;
  case logic::SearchStep::Keep:
    break;
  }
  if (settings.GetBool(SettingId::AimZ)) {
    aimDeg = logic::SearchSweepDeg(now, startedAt, SWEEP_PERIOD_MS, SWEEP_DEG);
    turret->gantry.SetRotationZ((int)lroundf(aimDeg), false);
  }
}
