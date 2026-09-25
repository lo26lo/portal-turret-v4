#pragma once

#include <Arduino.h>

// Shared I2C bus: LSM6DSOX + Qwiic port (J11). Always on PIN_SDA / PIN_SCL
// (IO35 / IO36), never on the variant defaults (plan §2.1, boot step 5).
namespace I2cBus {
// Frees a stuck bus, starts Wire at 400 kHz on the Turret2 pins and logs a scan.
// Returns false if Wire could not start.
bool Begin();
// Logs the address of every device that acknowledges; returns how many.
uint8_t Scan();
// True if a device acknowledges this address.
bool Probe(uint8_t address);
} // namespace I2cBus
