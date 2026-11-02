#pragma once

#include <Arduino.h>

// Shared I2C bus: LSM6DSOX + Qwiic port (J11). Always on PIN_SDA / PIN_SCL
// (IO35 / IO36), never on the variant defaults (plan §2.1, boot step 5).
namespace I2cBus {
// Very first step of setup() (board check, F2.2), before the console: frees a
// stuck bus and starts Wire at 400 kHz on the Turret2 pins. Silent: what
// happened is logged later by Begin(). Returns false if Wire could not start.
// Safe on both boards: IO35 / IO36 are the I2C bus on the v0.1 and the v2.
bool Start();
// Boot step 5: Start() if not done yet, then logs the recovery, if any, and a scan.
// Returns false if Wire could not start.
bool Begin();
// Logs the address of every device that acknowledges; returns how many.
uint8_t Scan();
// True if a device acknowledges this address.
bool Probe(uint8_t address);
} // namespace I2cBus
