#include "board/Log.h"
#include "BootState.h"
#include "StateMachine.h"

namespace {
// Guards the state only: the wings have their own 2 s timeout, counted once
// their servo is attached.
const ulong ATTACH_TIMEOUT_MS = 10000;
const ulong HOMING_TIMEOUT_MS = 5000;
} // namespace

void BootState::OnActivate() {
  BaseState::OnActivate();
  phase = Phase::Start;
  phaseAt = millis();
}

void BootState::Update(ulong deltaTime) {
  Board &board = turret->board;
  Gantry &gantry = turret->gantry;
  ulong now = millis();

  if (board.IsPowerFault()) {
    Log.println("Boot: PWR_FLT low, sequence stopped");
    stateMachine->GoToState(StateId::Fault);
    return;
  }

  switch (phase) {
  case Phase::Start:
    // D1 bench mode and D5 reduced mode: no servo is attached automatically.
    if (board.IsBenchMode() || board.IsReducedMode()) {
      Log.println(board.IsBenchMode() ? "Boot: bench mode, servos not attached"
                                         : "Boot: reduced mode, servos not attached");
      Finish();
      return;
    }
    // Step 11: rotate Z, rotate X (centred), gun left, gun right (retracted).
    gantry.StartBoot();
    phase = Phase::Attach;
    phaseAt = now;
    break;

  case Phase::Attach:
    if (gantry.HasPendingAttach() && now - phaseAt < ATTACH_TIMEOUT_MS) {
      return;
    }
    // Step 12: wings closed unless the Hall sensors say so (attached in turn).
    gantry.Home();
    phase = Phase::Homing;
    phaseAt = now;
    break;

  case Phase::Homing:
    if ((gantry.IsHoming() || gantry.HasPendingAttach()) && now - phaseAt < HOMING_TIMEOUT_MS) {
      return;
    }
    Finish();
    break;
  }
}

void BootState::Finish() {
  Board &board = turret->board;
  board.BootDone();
  Log.printf("Boot: done in %lu ms\n", millis());
  // Bench and reduced modes keep the state machine stopped (Manual does nothing).
  if (board.IsBenchMode() || board.IsReducedMode()) {
    stateMachine->GoToState(StateId::Manual);
  } else {
    stateMachine->GoToState(StateId::Idle);
  }
}
