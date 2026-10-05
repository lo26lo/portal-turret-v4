#include "FiringState.h"
#include "StateMachine.h"

void FiringRoutine::Initialize(Turret &_turret) { turret = &_turret; }

int FiringRoutine::runCoroutine() {
  COROUTINE_BEGIN();
  turret->audio.ShootAudio.Begin();
  shootingStartTime = millis();
  while (millis() < shootingStartTime + 3000) {
    COROUTINE_YIELD();
  }
  turret->audio.ShootAudio.Stop();
  COROUTINE_AWAIT(!turret->audio.ShootAudio.IsPlaying());
  COROUTINE_END();
}

void FiringState::Initialize(StateMachine *stateMachine, Turret &turret) {
  BaseState::Initialize(stateMachine, turret);
  firingRoutine.Initialize(turret);
}

void FiringState::OnActivate() {
  BaseState::OnActivate();
  turret->stats.CountCycle();
  firingRoutine.reset();
}

void FiringState::Update(ulong deltaTime) {
  firingRoutine.runCoroutine();
  if (firingRoutine.isDone()) {
    stateMachine->GoToState(StateId::Disengage);
  }
}