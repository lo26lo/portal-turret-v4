#pragma once

#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "Turret.h"
#include "web/AccessPoint.h"
#include "web/Station.h"

class StateMachine;

// Commands shared by the serial console (lot 10) and the web page (lot 12).
// Everything that touches hardware runs in loop(): the web server task only
// queues command lines (plan §10.2). Type "help" on the console for the list.
class Actions {
public:
  void Initialize(Turret &turret, StateMachine &stateMachine, AccessPoint &accessPoint, Station &station);
  // loop(): serial console input, queued web commands, OTA shutdown, reboot.
  void Update();

  // Runs one command line in loop() context and returns the answer.
  String Execute(const String &line);
  // Web server task: queues a line for loop(). False if the queue is full.
  bool Enqueue(const String &line);

  // GET /api/status. Cached values only (runs in the web server task).
  String StatusJson();

  // Reads every setting again into the modules that cache them (plan §10.5).
  void ApplyAllSettings();
  // Shutdown sequence (plan §3.3): servos detached, amp shut down, I2S stopped, LEDs off.
  void ShutdownForRestart();
  // Web server task, OTA upload start: asks loop() to shut down and waits for it
  // (plan §10.5: stop before accepting the data). False on timeout.
  bool RequestOtaShutdown(uint32_t timeoutMs);

private:
  String Help();
  String ServoCommand(const String &args);
  String LedCommand(const String &args);
  String SetCommand(const String &args);
  String GetCommand(const String &args);
  String ImuReport();
  String HallReport();
  // Tests need the state machine stopped (Manual). False (and a message) if not possible.
  bool EnterTestMode(String &error);
  void ReadConsole();

  Turret *turret = nullptr;
  StateMachine *stateMachine = nullptr;
  AccessPoint *accessPoint = nullptr;
  Station *station = nullptr;
  QueueHandle_t queue = nullptr;
  char consoleLine[128];
  size_t consoleLength = 0;
  ulong rebootAt = 0;
  bool rebootPending = false;
  std::atomic<bool> otaRequested{false};
  std::atomic<bool> otaReady{false};
};
