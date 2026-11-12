#pragma once

#include "Arduino.h"
#include "Turret.h"

class StateMachine; // forward declaration

class BaseState {
public:
  virtual void Initialize(StateMachine *stateMachine, Turret &turret);
  virtual void OnActivate();
  virtual void OnDeactivate();
  virtual void Update(ulong deltaTime);

protected:
  StateMachine *stateMachine = nullptr;
  Turret *turret = nullptr;
};