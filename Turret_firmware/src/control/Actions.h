#pragma once

#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <functional>

#include "Turret.h"
#include "logic/MenuLogic.h"
#include "web/AccessPoint.h"
#include "web/Station.h"

class StateMachine;
class SelfTest;
class Sleep;

// Commands shared by the serial console (lot 10), the web page (lot 12) and
// the debug menu. Everything that touches hardware runs in loop(): the web
// server task only queues command lines (plan §10.2).
//
// One command = one row of the COMMANDS table (Actions.cpp): name, usage,
// one-line help, category, handler and what it needs (test mode, full power).
// "help" is generated from the table; scripts/check_commands.py checks that
// the README lists every row. Handlers live in three files by subject:
// Actions.cpp (dispatch, settings, system, audio, network, status),
// ActionsHardware.cpp (servos, wings, guns, LEDs, calibration),
// ActionsLab.cpp (pins, rails, sweeps, load test).
class Actions {
public:
  // What a command needs before its handler runs (bit mask).
  enum Needs : uint8_t {
    None = 0,
    TestMode = 1,  // state machine stopped (Manual); refused in Fault / Booting
    FullPower = 2, // refused in debug mode until "power full" (one servo at a time)
  };
  typedef String (Actions::*Handler)(const String &args);
  struct Command {
    const char *name;
    const char *usage;    // for "help" and the usage answers
    const char *help;     // one line
    const char *category; // "help" groups the commands by category
    Handler handler;
    uint8_t needs;
  };
  static const Command COMMANDS[];
  static const size_t COMMAND_COUNT;

  void Initialize(Turret &turret, StateMachine &stateMachine, AccessPoint &accessPoint, Station &station);
  // loop(): serial console input, queued web commands, OTA shutdown, reboot.
  void Update();

  // Runs one command line in loop() context and returns the answer.
  String Execute(const String &line);
  // Web server task: queues a line for loop(). False if the queue is full.
  bool Enqueue(const String &line);

  // GET /api/status. Cached values only (runs in the web server task).
  String StatusJson();
  // GET /api/meta: the lists the page needs (fault names, IMU axes, voices,
  // servo aliases, states), in the screen language, so that they live in one
  // place (the firmware). Constant: read once by the page.
  String MetaJson() const;

  // Reads every setting again into the modules that cache them (plan §10.5).
  void ApplyAllSettings();
  // Shutdown sequence (plan §3.3): servos detached, amp shut down, I2S stopped, LEDs off.
  void ShutdownForRestart();
  // Web server task, OTA upload start: asks loop() to shut down and waits for it
  // (plan §10.5: stop before accepting the data). False on timeout.
  bool RequestOtaShutdown(uint32_t timeoutMs);
  // L5: what draws current right now (servos attached, sound, amplifier, LEDs),
  // logged with each PWR_FLT event.
  String LoadSnapshot();
  // Debug menu (ui/DebugUi): where the "key" and "screen" commands go.
  void SetDebugUi(std::function<void(logic::MenuKey)> keys, std::function<String()> screen,
                  std::function<bool()> displayPresent) {
    keyHandler = keys;
    screenText = screen;
    hasDisplay = displayPresent;
  }
  // Self-test (control/SelfTest): the "selftest" command.
  void SetSelfTest(SelfTest *test) { selfTest = test; }
  // Lot F2.6: "sleep" command and the sleep state in /api/status.
  void SetSleep(Sleep *sleepIn) { sleep = sleepIn; }
  // Called at the end of ApplyAllSettings (the debug screen reloads its own settings).
  void SetSettingsListener(std::function<void()> listener) { settingsListener = listener; }
  bool IsFrench() const;
  // GET /api/selftest: summary and one line per check (read-only, any task).
  String SelfTestReport() const;

private:
  // ---- Actions.cpp: dispatch, settings, system, audio, network, status
  const Command *Find(const String &name) const;
  // Tests need the state machine stopped (Manual). False (and a message) if not possible.
  bool EnterTestMode(String &error);
  void ReadConsole();
  void SaveVolume();
  String CmdHelp(const String &args);
  String CmdStatus(const String &args);
  String CmdScan(const String &args);
  String CmdFlt(const String &args);
  String CmdResetReason(const String &args);
  String CmdCrashlog(const String &args);
  String CmdCoredump(const String &args);
  String CmdGet(const String &args);
  String CmdSet(const String &args);
  String CmdResetSettings(const String &args);
  String CmdFactoryReset(const String &args);
  String CmdMute(const String &args);
  String CmdVolume(const String &args);
  String CmdGain(const String &args);
  String CmdTone(const String &args);
  String CmdShot(const String &args);
  String CmdWifi(const String &args);
  String CmdTime(const String &args);
  String CmdReboot(const String &args);
  String CmdResume(const String &args);
  String CmdDemo(const String &args);
  String CmdPower(const String &args);
  String CmdKey(const String &args);
  String CmdScreen(const String &args);
  String CmdSelfTest(const String &args);
  String CmdSleep(const String &args);
  String CmdStats(const String &args);

  // ---- ActionsHardware.cpp: servos, wings, guns, LEDs, voice, calibration, reports
  void LimitToChannel(uint8_t index);
  String WingOrGun(bool wing, const String &args);
  String CmdImu(const String &args);
  String CmdHall(const String &args);
  String CmdInputs(const String &args);
  String CmdServo(const String &args);
  String CmdServos(const String &args);
  String CmdWing(const String &args);
  String CmdGun(const String &args);
  String CmdWings(const String &args);
  String CmdGuns(const String &args);
  String CmdTrim(const String &args);
  String CmdLed(const String &args);
  String CmdVoice(const String &args);
  String CmdCal(const String &args);

  // ---- ActionsLab.cpp: pins, rails, sweeps, load test
  String PinsReport() const;
  String PinReport(uint8_t pin) const;
  String RailsReport() const;
  String CmdPins(const String &args);
  String CmdRails(const String &args);
  String CmdRail(const String &args);
  String CmdSweep(const String &args);
  String CmdLoadTest(const String &args);
  void UpdateSweep(ulong now);
  void UpdateLoadTest(ulong now);
  void CancelLabTests();

  // Debug menu: receives the keys of the "key" command (virtual buttons).
  std::function<void(logic::MenuKey)> keyHandler;
  // Debug menu: text of the screen for the "screen" command.
  std::function<String()> screenText;
  std::function<bool()> hasDisplay;
  std::function<void()> settingsListener;
  SelfTest *selfTest = nullptr;
  Sleep *sleep = nullptr;

  bool shotPending = false;
  ulong shotStopAt = 0;
  uint16_t hallCapture[2][2] = {}; // [left, right][open, closed]
  bool hallCaptured[2][2] = {};

  // L2: servo sweep 0 -> 180 -> 0 degrees
  int8_t sweepChannel = -1;
  ulong sweepStart = 0;
  ulong sweepDuration = 0;
  ulong sweepLastWrite = 0;

  // L4: load test phases
  enum class LoadPhase { Off, Attaching, Holding, Returning };
  LoadPhase loadPhase = LoadPhase::Off;
  ulong loadPhaseAt = 0;
  ulong loadGapMs = 0;

  Turret *turret = nullptr;
  StateMachine *stateMachine = nullptr;
  AccessPoint *accessPoint = nullptr;
  Station *station = nullptr;
  QueueHandle_t queue = nullptr;
  char consoleLine[128];
  size_t consoleLength = 0;
  ulong rebootAt = 0;
  bool rebootPending = false;
  // "volume": NVS write deferred until the knob has stopped.
  static const ulong VOLUME_SAVE_DELAY_MS = 2000;
  bool volumeSavePending = false;
  ulong volumeSaveAt = 0;
  std::atomic<bool> otaRequested{false};
  std::atomic<bool> otaReady{false};
};

// Shared by the three Actions*.cpp files.
String FirstWord(const String &text, String &rest);
