#include "ActivateState.h"
#include "StateMachine.h"

namespace {
// The deploy voice line (01_activate, up to 3.2 s) ends before the first shot,
// but never holds the turret back longer than this from the activation.
const ulong VOICE_WAIT_MAX_MS = 3500;
} // namespace

void ActivateRoutine::Initialize(Turret &_turret) { turret = &_turret; }

int ActivateRoutine::runCoroutine() {
  COROUTINE_BEGIN();
  startedAt = millis();
  turret->audio.PlayVoice(Voice::Activate);
  turret->gantry.OpenWings();
  COROUTINE_AWAIT(turret->gantry.GetWingLeft().IsOpen() && turret->gantry.GetWingRight().IsOpen());
  turret->gantry.GetWingLeft().GetGun().Extend();
  turret->gantry.GetWingRight().GetGun().Extend();
  COROUTINE_DELAY(500);
  COROUTINE_AWAIT(!turret->audio.IsVoicePlaying() || millis() - startedAt >= VOICE_WAIT_MAX_MS);
  COROUTINE_END();
}

void ActivateState::Initialize(StateMachine *stateMachine, Turret &turret) {
  BaseState::Initialize(stateMachine, turret);
  activateRoutine.Initialize(turret);
}

void ActivateState::OnActivate() {
  BaseState::OnActivate();
  activateRoutine.reset();
}

void ActivateState::Update(ulong deltaTime) {
  activateRoutine.runCoroutine();
  if (activateRoutine.isDone()) {
    stateMachine->GoToState(StateId::FiringState);
  }
}