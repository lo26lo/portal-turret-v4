#pragma once

#include "BaseState.h"

// B3: after a burst, the target is checked again for SearchMs: back in the
// zone = Firing again (aimed, AimZ); nothing for SearchMs = Disengage. Says a
// *search* line once the gunshot tail has ended (a voice is refused while the
// shot plays) and, with AimZ, sweeps the rotation Z left and right meanwhile.
class SearchingState : public BaseState {
public:
  void OnActivate() override;
  void Update(ulong deltaTime) override;

private:
  ulong startedAt = 0;
  bool voiceStarted = false;
  float aimDeg = 0;
};
