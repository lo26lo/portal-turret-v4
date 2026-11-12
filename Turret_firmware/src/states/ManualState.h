#pragma once

#include "BaseState.h"

// The state machine is stopped: debug mode, reduced mode, hardware tests.
// Nothing happens here; "resume" goes through Booting again.
class ManualState : public BaseState {};
