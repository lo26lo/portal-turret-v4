#include "board/Log.h"
#include "states/StateMachine.h"

void StateMachine::Initialize(Turret &turretIn) {
  bootState.Initialize(this, turretIn);
  idleState.Initialize(this, turretIn);
  activateState.Initialize(this, turretIn);
  disengageState.Initialize(this, turretIn);
  manualState.Initialize(this, turretIn);
  firingState.Initialize(this, turretIn);
  faultState.Initialize(this, turretIn);
}

void StateMachine::GoToState(StateId nextStateId) {
  if (currentState) {
    currentState->OnDeactivate();
  }

  currentState = GetState(nextStateId);
  currentStateId = nextStateId;

  if (currentState) {
    currentState->OnActivate();
  }
}

BaseState *StateMachine::GetState(StateId stateId) {
  switch (stateId) {
  case StateId::Booting:
    Log.println("Bootstate");
    return &bootState;
    break;
  case StateId::Activate:
    Log.println("ActivateState");
    return &activateState;
    break;
  case StateId::FiringState:
    Log.println("FiringState");
    return &firingState;
    break;
  case StateId::Disengage:
    Log.println("DisengageState");
    return &disengageState;
    break;
  case StateId::Idle:
    Log.println("IdleState");
    return &idleState;
  case StateId::Manual:
    Log.println("ManualState");
    return &manualState;
  case StateId::Fault:
    Log.println("FaultState");
    return &faultState;
  }
  return nullptr;
}

const char *StateMachine::StateName(StateId id) {
  switch (id) {
  case StateId::Booting: return "Booting";
  case StateId::Idle: return "Idle";
  case StateId::Activate: return "Activate";
  case StateId::FiringState: return "Firing";
  case StateId::Disengage: return "Disengage";
  case StateId::Manual: return "Manual";
  case StateId::Fault: return "Fault";
  }
  return "?";
}

void StateMachine::Update(ulong deltaTime) {
  if (currentState) {
    currentState->Update(deltaTime);
  }
}