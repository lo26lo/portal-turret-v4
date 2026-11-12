#pragma once

#include <AceRoutine.h>

#include "BaseState.h"
#include "StateId.h"

class DisengageRoutine : public ace_routine::Coroutine {
public:
  void Initialize(Turret &turret);
  int runCoroutine() override;

private:
  Turret *turret = nullptr;
};

class DisengageState : public BaseState {
public:
  void Initialize(StateMachine *stateMachine, Turret &turret) override;
  void OnActivate() override;
  void Update(ulong deltaTime) override;

private:
  DisengageRoutine disengageRoutine;
};