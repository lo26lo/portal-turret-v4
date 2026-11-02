#pragma once

#include <Arduino.h>

// Board check (lot F2.2, docs/carte-v2-plan.md §10.3). The v0.1 and v2 builds
// put almost every function on other pins; the wrong one would, for example,
// drive IO7 (SW1 on the v2: short to GND when SW1 is closed) or IO47 high
// (amplifier gain on the v0.1). Before touching any other pin, the firmware
// starts the I2C bus, common to both boards (IO35 / IO36), and looks for the
// TCAL6416 rail expander at 0x20, which only the v2 has:
//  - v2 build: the expander must answer;
//  - v0.1 build: nothing may answer at 0x20.
// Whenever the expander answers (either build), every rail is switched off at
// once: after a software restart it keeps its outputs.
namespace BoardCheck {
// First call of setup(). Silent: the console is not up yet.
void Run();
// The build matches the board.
bool IsRightBoard();
// Expander seen at 0x20, and rails switched off (v2 build, right board).
bool HasExpander();
bool RailsOff();
// Boot step 3, after the banner: one line on what was found.
void PrintReport();
// Wrong board: nothing else is started and no pin other than I2C and USB is
// touched. The console (USB) repeats why every few seconds and answers
// "rollback" (boot the other OTA partition, if it holds an app) and "scan".
// Never returns.
[[noreturn]] void HaltWrongBoard();
} // namespace BoardCheck
