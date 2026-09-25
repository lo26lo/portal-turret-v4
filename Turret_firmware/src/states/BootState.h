#pragma once

#include "BaseState.h"

// Last part of the boot sequence (docs/firmware-plan.md §3.2, steps 11 to 13):
// servos attached one at a time (Gantry attach schedule), homing, then Idle.
// setup() has already done steps 1 to 10. PWR_FLT low at any point stops the
// sequence (Fault state).
class BootState : public BaseState {
public:
  void OnActivate() override;
  void Update(ulong deltaTime) override;

private:
  enum class Phase { Start, Attach, Homing };

  void Finish();

  Phase phase = Phase::Start;
  ulong phaseAt = 0;
};
