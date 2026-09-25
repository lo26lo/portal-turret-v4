#include "board/Log.h"
#include "FaultState.h"
#include "StateMachine.h"

namespace {
const ulong RECOVERY_MS = 2000; // PWR_FLT high this long before resuming
} // namespace

void FaultState::OnActivate() {
  BaseState::OnActivate();
  turret->gantry.DetachAll(); // wings stop dead, guns and rotations go limp
  turret->audio.ShootAudio.Stop();
  turret->audio.amp.Shutdown();
  turret->light.SetEnabled(false);
  turret->board.SetFault(Fault::EFuse, true);
  high = false;
  Log.println("Fault: load shed (servos detached, amp muted, LEDs off)");
}

void FaultState::Update(ulong deltaTime) {
  ulong now = millis();
  if (turret->board.IsPowerFault()) {
    high = false;
    return;
  }
  if (!high) {
    high = true;
    highSince = now;
    return;
  }
  if (now - highSince < RECOVERY_MS) {
    return;
  }

  Log.println("Fault: PWR_FLT high for 2 s, resuming");
  turret->board.SetFault(Fault::EFuse, false);
  turret->light.SetEnabled(true);
  turret->audio.amp.Start(); // I2S clocks kept running during the fault
  stateMachine->GoToState(StateId::Booting);
}
