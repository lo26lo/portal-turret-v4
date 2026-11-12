#pragma once

#include "BaseState.h"
#include "StateId.h"

// Burst of fire: gunshot sound with its loop for FireMs, rotation Z following
// the target (AimZ). Then Searching decides whether to fire again or retire.
class FiringState : public BaseState {
public:
  void OnActivate() override;
  void OnDeactivate() override;
  void Update(ulong deltaTime) override;

  // Rotation Z towards the first target in the zone, smoothed; nothing if
  // AimZ is off or no target. aimDeg is the caller's smoothed angle.
  static void Aim(Turret &turret, float &aimDeg);
  // Where the rotation Z was last aimed (Searching starts its sweep from there).
  static float LastAimDeg() { return lastAimDeg; }

private:
  static float lastAimDeg;
  ulong startedAt = 0;
  float aimDeg = 0;
};
