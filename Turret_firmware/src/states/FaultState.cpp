#include "board/Log.h"
#include "board/Power.h"
#include "FaultState.h"
#include "StateMachine.h"

namespace {
const ulong RECOVERY_MS = 2000; // PWR_FLT high this long before resuming
} // namespace

void FaultState::OnActivate() {
  BaseState::OnActivate();
  turret->gantry.DetachAll(); // wings stop dead, guns and rotations go limp
  turret->audio.ShootAudio.Stop();
  turret->audio.StopVoice();
  turret->audio.amp.Shutdown();
  turret->light.SetEnabled(false);
  // v2: the servo and LED rails go off too (servos released, LEDs disabled above).
  Power::Off(Rail::Servo);
  Power::Off(Rail::Led);
  turret->board.SetFault(Fault::EFuse, true);
  high = false;
  Log.println(Power::IsSwitchable() ? "Fault: load shed (servo and LED rails off, amp muted)"
                                    : "Fault: load shed (servos detached, amp muted, LEDs off)");
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
  // v2: rails back one at a time, LEDs first, servos last (as at boot).
  Power::On(Rail::Led);
  turret->light.SetEnabled(true);
  if (!turret->board.IsReducedMode()) {
    Power::On(Rail::Servo);
  }
  turret->audio.amp.Start(); // I2S clocks kept running during the fault
  stateMachine->GoToState(StateId::Booting);
}
