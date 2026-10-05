#include "DisengageState.h"
#include "StateMachine.h"

void DisengageRoutine::Initialize(Turret& _turret) { turret = &_turret; }

int DisengageRoutine::runCoroutine() {
  COROUTINE_BEGIN();
  turret->gantry.GetWingLeft().GetGun().Retract();
  turret->gantry.GetWingRight().GetGun().Retract();
  COROUTINE_DELAY(500);
  turret->gantry.CloseWings();
  COROUTINE_AWAIT(!turret->gantry.GetWingLeft().IsClosing() &&
                  !turret->gantry.GetWingRight().IsClosing());
  COROUTINE_DELAY(500);
  COROUTINE_END();
}

void DisengageState::Initialize(StateMachine* stateMachine, Turret& turret) {
  BaseState::Initialize(stateMachine, turret);
  disengageRoutine.Initialize(turret);
}

void DisengageState::OnActivate() {
  BaseState::OnActivate();
  disengageRoutine.reset();
}

void DisengageState::Update(ulong deltaTime) {
  disengageRoutine.runCoroutine();
  if (disengageRoutine.isDone()) {
    // Debug mode (SW1): the state machine never starts a cycle by itself, so a
    // demo cycle ends stopped (Manual) instead of waiting for the radar (Idle).
    stateMachine->GoToState(turret->board.IsBenchMode() ? StateId::Manual : StateId::Idle);
  }
}