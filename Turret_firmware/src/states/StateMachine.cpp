#include "board/Log.h"
#include "states/StateMachine.h"

void StateMachine::Initialize(Turret &turretIn) {
  bootState.Initialize(this, turretIn);
  idleState.Initialize(this, turretIn);
  activateState.Initialize(this, turretIn);
  firingState.Initialize(this, turretIn);
  searchingState.Initialize(this, turretIn);
  disengageState.Initialize(this, turretIn);
  manualState.Initialize(this, turretIn);
  faultState.Initialize(this, turretIn);
}

void StateMachine::GoToState(StateId nextStateId) {
  if (currentState) {
    currentState->OnDeactivate();
  }
  currentState = GetState(nextStateId);
  currentStateId = nextStateId;
  Log.printf("State: %s\n", StateName(nextStateId));
  if (currentState) {
    currentState->OnActivate();
  }
}

BaseState *StateMachine::GetState(StateId stateId) {
  switch (stateId) {
  case StateId::Booting: return &bootState;
  case StateId::Idle: return &idleState;
  case StateId::Activate: return &activateState;
  case StateId::FiringState: return &firingState;
  case StateId::Searching: return &searchingState;
  case StateId::Disengage: return &disengageState;
  case StateId::Manual: return &manualState;
  case StateId::Fault: return &faultState;
  }
  return nullptr;
}

const char *StateMachine::StateName(StateId id) {
  switch (id) {
  case StateId::Booting: return "Booting";
  case StateId::Idle: return "Idle";
  case StateId::Activate: return "Activate";
  case StateId::FiringState: return "Firing";
  case StateId::Searching: return "Searching";
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
