#pragma once

#include <AceRoutine.h>

#include "BaseState.h"
#include "StateId.h"

class ActivateRoutine : public ace_routine::Coroutine {
 public:
  void Initialize(Turret& turret);
  int runCoroutine() override;

 private:
  Turret* turret = nullptr;
  ulong startedAt = 0;
};

class ActivateState : public BaseState {
 public:
  void Initialize(StateMachine* stateMachine, Turret& turret) override;
  void OnActivate() override;
  void Update(ulong deltaTime) override;

 private:
  ActivateRoutine activateRoutine;
};