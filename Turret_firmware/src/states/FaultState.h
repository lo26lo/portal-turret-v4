#pragma once

#include "BaseState.h"

// Power fault (PWR_FLT low, plan §4): everything that draws current on the
// 5 V rail is shed at once. After PWR_FLT has been high for 2 s the turret
// goes through Booting again (servos re-attached one at a time, homing).
class FaultState : public BaseState {
public:
  void OnActivate() override;
  void Update(ulong deltaTime) override;

private:
  ulong highSince = 0;
  bool high = false;
};
