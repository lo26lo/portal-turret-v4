#pragma once

// The console commands, as data without Arduino: name, usage, one-line help,
// category and what the command needs before it runs. Used by control/Actions
// (which maps each CommandId to a handler, same order), by the native tests
// (test/test_logic) and by scripts/check_commands.py, which checks that the
// README lists every row. "help" is generated from this table, one heading per
// category: keep the categories contiguous.

#include <stdint.h>
#include <string.h>
#include <strings.h>

namespace commands {

// What a command needs before its handler runs (bit mask).
enum Needs : uint8_t {
  None = 0,
  TestMode = 1,  // state machine stopped (Manual); refused in Fault / Booting
  FullPower = 2, // refused in debug mode until "power full" (one servo at a time)
};

// One value per row below, in the same order (Actions::HANDLERS follows it).
enum class Id : uint8_t {
  Help,
  Status,
  Scan,
  Imu,
  Hall,
  Flt,
  Inputs,
  Pins,
  Rails,
  Rail,
  ResetReason,
  Crashlog,
  Coredump,
  Stats,
  Aim,
  Get,
  Set,
  ResetSettings,
  FactoryReset,
  Servo,
  Servos,
  Wings,
  Guns,
  Wing,
  Gun,
  Trim,
  Power,
  Cal,
  Led,
  Tone,
  Shot,
  Gain,
  Mute,
  Volume,
  Voice,
  Wifi,
  Time,
  Key,
  Screen,
  SelfTest,
  Demo,
  Resume,
  Sleep,
  Reboot,
  Sweep,
  LoadTest,
  Count
};

struct Spec {
  const char *name;
  const char *usage;    // for "help" and the usage answers; starts with the name
  const char *help;     // one line
  const char *category; // "help" groups the commands by category
  Id id;
  uint8_t needs;
};

// scripts/check_commands.py reads this table: one row per line, starting with {"name", "usage", "help", "category".
static const Spec TABLE[] = {
    {"help", "help [category]", "this list, or one category of it", "Status", Id::Help, None},
    {"status", "status", "the status JSON (same as GET /api/status)", "Status", Id::Status, None},
    {"scan", "scan", "I2C scan", "Status", Id::Scan, None},
    {"imu", "imu", "gravity, dominant axis, upright, gyro, temperature", "Status", Id::Imu, None},
    {"hall", "hall", "raw Hall values and wing positions", "Status", Id::Hall, None},
    {"flt", "flt", "PWR_FLT level and fault count since boot", "Status", Id::Flt, None},
    {"inputs", "inputs", "raw levels of PROG, SW1, the buttons", "Status", Id::Inputs, None},
    {"pins", "pins [n]", "green LED / USB registers, or the routing and the pad activity of GPIO n", "Status", Id::Pins,
     None},
    {"rails", "rails", "v2: rails read back from the expander", "Status", Id::Rails, None},
    {"rail", "rail servo|led|radar|hall on|off", "v2: switches one rail (its driver stopped / started)", "Status",
     Id::Rail, None},
    {"reset-reason", "reset-reason", "reset reason, suspicious boots, brownouts", "Status", Id::ResetReason, None},
    {"crashlog", "crashlog", "log tail of the previous run", "Status", Id::Crashlog, None},
    {"coredump", "coredump [erase]", "core dump summary, or erase it", "Status", Id::Coredump, None},
    {"stats", "stats", "boots, cycles, targets seen, last cycle", "Status", Id::Stats, None},
    {"aim", "aim", "radar targets with their aim angle and the rotation Z pulse (nothing moves)", "Status", Id::Aim,
     None},

    {"get", "get [key]", "read one setting, or all", "Settings", Id::Get, None},
    {"set", "set <key> <value>", "change a setting (saved; a choice by name or number)", "Settings", Id::Set, None},
    {"reset-settings", "reset-settings [group]", "settings back to their defaults", "Settings", Id::ResetSettings,
     None},
    {"factory-reset", "factory-reset", "everything back to factory state, then a restart", "Settings", Id::FactoryReset,
     None},

    {"servo", "servo <0-5|rotz|rotx|gunl|gunr|wingl|wingr> <angle 0-180|us 500-2400|off>", "drive one servo",
     "Mechanics", Id::Servo, TestMode},
    {"servos", "servos off", "release every servo", "Mechanics", Id::Servos, None},
    {"wings", "wings open|close", "both wings", "Mechanics", Id::Wings, TestMode | FullPower},
    {"guns", "guns extend|retract", "both guns", "Mechanics", Id::Guns, TestMode | FullPower},
    {"wing", "wing left|right open|close", "one wing", "Mechanics", Id::Wing, TestMode},
    {"gun", "gun left|right extend|retract", "one gun", "Mechanics", Id::Gun, TestMode},
    {"trim", "trim left|right <-200..200 us>", "wing at its stop point with this trim for 2 s (not saved)", "Mechanics",
     Id::Trim, TestMode},
    {"power", "power [full|limited]", "debug mode: all servos (3 A supply) or one at a time", "Mechanics", Id::Power,
     None},
    {"cal", "cal hall left|right open|closed | cal hall save | cal imu", "calibration without the web page",
     "Mechanics", Id::Cal, None},

    {"led", "led ring|left|right|all <RRGGBB|rainbow|off> | led rainbow | led pattern bit", "LED test colours",
     "Lights", Id::Led, TestMode},

    {"tone", "tone <20..20000 Hz> [ms <= 10000]", "test tone", "Audio", Id::Tone, None},
    {"shot", "shot", "gunshot sound for 1 s", "Audio", Id::Shot, None},
    {"gain", "gain 9|12|15", "amplifier gain (not saved: set AmpGain)", "Audio", Id::Gain, None},
    {"mute", "mute [on|off]", "mute / unmute", "Audio", Id::Mute, None},
    {"volume", "volume [0-100|+n|-n]", "volume, saved 2 s after the last change", "Audio", Id::Volume, None},
    {"voice", "voice [activate|collision|shutdown|pickup|retire|search|tipped|alarm|stop]",
     "a voice line, or the folders", "Audio", Id::Voice, None},

    {"wifi", "wifi [on|off|scan|join|forget]", "WiFi state and control", "Network", Id::Wifi, None},
    {"time", "time", "local time (NTP)", "Network", Id::Time, None},

    {"key", "key a|al|b|bl|cw|ccw|push|hold", "debug menu: a button or the knob", "Debug", Id::Key, None},
    {"screen", "screen", "the six lines of the debug screen", "Debug", Id::Screen, None},
    {"selftest", "selftest [quick|report|stop]", "self-test: full, passive only, report, stop", "Debug", Id::SelfTest,
     None},

    {"demo", "demo", "one activation cycle (from Idle)", "System", Id::Demo, None},
    {"resume", "resume", "leave the tests: homing, then Idle", "System", Id::Resume, None},
    {"sleep", "sleep [radar|deep|off]", "sleep state, or go to sleep / wake up", "System", Id::Sleep, None},
    {"reboot", "reboot", "restart with the shutdown sequence", "System", Id::Reboot, None},

    {"sweep", "sweep servo <n> <ms> | sweep tone <Hz> <Hz> <ms> | sweep stop", "servo 0-180-0 or frequency sweep",
     "Lab", Id::Sweep, None},
    {"loadtest", "loadtest <gap 0..2000 ms>", "six servos attached gap ms apart, PWR_FLT watched", "Lab", Id::LoadTest,
     TestMode | FullPower},
};

const size_t COUNT = sizeof(TABLE) / sizeof(TABLE[0]);
static_assert(sizeof(TABLE) / sizeof(TABLE[0]) == (size_t)Id::Count, "one row per CommandId");

// The row named `name` (any case), nullptr if none. "?" is "help".
inline const Spec *Find(const char *name) {
  if (strcmp(name, "?") == 0) {
    name = "help";
  }
  for (size_t i = 0; i < COUNT; i++) {
    if (strcasecmp(TABLE[i].name, name) == 0) {
      return &TABLE[i];
    }
  }
  return nullptr;
}

// True if `category` names a category of the table (any case).
inline bool HasCategory(const char *category) {
  for (size_t i = 0; i < COUNT; i++) {
    if (strcasecmp(TABLE[i].category, category) == 0) {
      return true;
    }
  }
  return false;
}

} // namespace commands
