#pragma once

#include "BaseState.h"

// B3: after a burst, the target is checked again for SearchMs: back in the
// zone = Firing again (aimed, AimZ); nothing for SearchMs = Disengage. Says a
// *search* line and, with AimZ, sweeps the rotation Z left and right meanwhile.
class SearchingState : public BaseState {
public:
  void OnActivate() override;
  void Update(ulong deltaTime) override;

private:
  ulong startedAt = 0;
  float aimDeg = 0;
};
