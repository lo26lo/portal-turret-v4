#pragma once

#include "BaseState.h"

class IdleState : public BaseState {
public:
  void OnActivate() override;
  void Update(ulong deltaTime) override;

private:
  ulong enteredAt = 0; // start of the CooldownMs rest
};