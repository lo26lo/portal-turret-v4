#pragma once

#include <Arduino.h>

// Switched rails of the v2 board (lot F2.4, docs/carte-v2-plan.md §2), driven
// through the TCAL6416 expander (board/Expander):
//   Servo  5V_SERVO  servos and gun LEDs (J3, J4, J7-J10)  TPS22965 U10, rise ~8 ms
//   Led    5V_LED    LED ring (J14)                         TPS22965 U11, rise ~2 ms
//   Radar  5V_RADAR  RD-03D (J5)                            TPS22965 U12, rise ~2 ms
//   Hall   3V3_HALL  Hall sensors (J17, J18)                P-MOS Q1, gate active low
// All rails are off at reset and after the board check (board/BoardCheck).
//
// Rules (plan v2 §2, §2.1):
//  - switching on: one rail at a time, >= 10 ms apart (inrush currents add up
//    on the eFuse), and On() returns once the rail is up;
//  - switching off: the lines towards a rail must not stay high, or they feed
//    the unpowered module through its protection diodes. The caller first
//    stops what drives them (servos released, LEDs disabled, radar UART ended);
//    Off() then holds the servo lines low;
//  - order when everything goes off: servos, LEDs, radar, Hall (AllOff()).
//
// v0.1 board: no switch, the rails are always on. Every call does nothing and
// IsOn() is always true.
enum class Rail : uint8_t { Servo, Led, Radar, Hall };

namespace Power {
const uint8_t RAIL_COUNT = 4;

// True on a board with switched rails (v2).
bool IsSwitchable();
// Switches the rail on (nothing if it already is), waiting for the gap after
// the previous switch-on and for the rise time. False if the expander did
// not take it.
bool On(Rail rail);
// Switches the rail off (see the rules above). False if the expander did not take it.
bool Off(Rail rail);
// Servos, LEDs, radar, Hall off in that order, then the whole expander port
// written "all off" again. Callers stop the drivers first.
bool AllOff();
bool IsOn(Rail rail);

const char *Name(Rail rail);
// "servo" / "servos", "led" / "leds", "radar", "hall". False if unknown.
bool Parse(const String &name, Rail &rail);
} // namespace Power
