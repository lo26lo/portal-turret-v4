#include "board/Log.h"
#include "states/IdleState.h"
#include "StateMachine.h"
#include "sensors/Radar.h"

void IdleState::OnActivate() {
  Log.println("IdleState");
  BaseState::OnActivate();
}

void IdleState::Update(ulong deltaTime) {
  // Never deploy the wings of a turret lying on its side (plan §3.2 step 6).
  if (!turret->motion.CanDeploy()) {
    return;
  }
  for (uint8_t i = 0; i < TRACK_COUNT; i++) {
    RadarTarget target = turret->radar.GetTarget(i);

    if (turret->radar.GetTargetCount() > 0 && target.available) {
      stateMachine->GoToState(StateId::Activate);
      break;
    }
  }
}