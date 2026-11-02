#pragma once

#include <Arduino.h>

// TCAL6416 GPIO expander of the v2 board (U13, EXPANDER_ADDRESS = 0x20 with
// ADDR to GND), on the shared I2C bus. P00..P03 switch the rails
// (EXPANDER_* in pins.h); P04..P17 are free and stay inputs.
//
// Only the registers common to the whole 6416 family (TCA6416A, PCAL6416A,
// TCAL6416) are used: 0x00 / 0x01 input, 0x02 / 0x03 output, 0x06 / 0x07
// configuration (1 = input). At power-up and after a RESET (wired to EN of
// the ESP32) the ports are inputs and the output registers hold 0xFF
// (docs/carte-v2-plan.md §4.9): the 100k pull-downs / pull-up keep every rail
// off until the firmware drives the ports.
//
// Pitfall: a software restart (ESP.restart, panic, watchdog, brownout) does
// not touch EN, so the expander keeps its outputs. AllOff() is therefore the
// first thing done at boot (board/BoardCheck) and the last before a restart.
namespace Expander {
// True if a device acknowledges EXPANDER_ADDRESS and its configuration
// registers can be read. Nothing answers there on a v0.1.
bool Probe();
// Every rail off: output levels first (P00..P02 low, P03 high), then
// P00..P03 as outputs. In this order, so that no rail turns on for a moment
// with the 0xFF of the output register. Read back; true if it took.
bool AllOff();
// One port of P00..P07 to a level (it must already be an output: AllOff()).
// Raw level: HALL_EN_N is active low. Read back; true if it took.
bool SetPort(uint8_t port, bool high);
// Last output and configuration registers of port 0 read from the chip;
// false if the bus did not answer.
bool ReadPort0(uint8_t &output, uint8_t &config, uint8_t &input);
} // namespace Expander
