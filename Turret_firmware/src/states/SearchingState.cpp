#include "SearchingState.h"
#include "FiringState.h"
#include "StateMachine.h"
#include "logic/CycleLogic.h"

namespace {
const uint32_t SWEEP_PERIOD_MS = 4000; // one left-right-left round
const float SWEEP_DEG = 30.0f;
const float SWEEP_ALPHA = 0.25f; // the sweep is reached smoothly from the last aim
} // namespace

void SearchingState::OnActivate() {
  BaseState::OnActivate();
  startedAt = millis();
  voiceStarted = false;
  aimDeg = FiringState::LastAimDeg();
}

void SearchingState::Update(ulong deltaTime) {
  Settings &settings = turret->settings;
  Audio &audio = turret->audio;
  ulong now = millis();
  // The gunshot tail plays out first: a voice is refused while it does, and
  // Disengage must not start its *retire* line over it either.
  bool shotOver = !audio.ShootAudio.IsPlaying();
  if (shotOver && !voiceStarted) {
    voiceStarted = true;
    audio.PlayVoice(Voice::Search);
  }
  bool target = turret->radar.FirstTargetInZone() >= 0;
  switch (logic::SearchDecision(target, now, startedAt, settings.GetInt(SettingId::SearchMs))) {
  case logic::SearchStep::Fire: stateMachine->GoToState(StateId::FiringState); return;
  case logic::SearchStep::GiveUp:
    if (shotOver) {
      stateMachine->GoToState(StateId::Disengage);
    }
    return;
  case logic::SearchStep::Keep: break;
  }
  if (settings.GetBool(SettingId::AimZ)) {
    aimDeg = logic::SmoothAim(aimDeg, logic::SearchSweepDeg(now, startedAt, SWEEP_PERIOD_MS, SWEEP_DEG), SWEEP_ALPHA);
    turret->gantry.SetRotationZ((int)lroundf(aimDeg), false);
  }
}
