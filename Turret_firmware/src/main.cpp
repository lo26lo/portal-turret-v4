#include "Turret.h"
#include "board/Board.h"
#include "board/CoreDump.h"
#include "board/I2cBus.h"
#include "board/Log.h"
#include "control/Actions.h"
#include "control/SelfTest.h"
#include "control/Stats.h"
#include "pins.h"
#include "states/StateMachine.h"
#include "ui/DebugUi.h"
#include "web/AccessPoint.h"
#include "web/Station.h"
#include "web/Ota.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>

ulong prevTime;

TurretWebServer server;
StateMachine stateMachine;
Gantry gantry;
Motion motion;
Radar radar;
Audio audio;
Light light;
Ota ota;
Settings settings;
Board board;
AccessPoint accessPoint;
Station station;
Actions actions;
DebugUi debugUi;
Stats stats;
SelfTest selfTest;

// No radar frame for this long -> red LED code 4 (plan §3.2 step 7).
const ulong RADAR_TIMEOUT_MS = 3000;

// A4: loop() must come back within this time, or the task watchdog panics.
const uint32_t LOOP_WATCHDOG_S = 8;

Turret turret{gantry, motion, radar, audio, light, server, settings, board, stats};

// A3: after a crash, the core dump summary and the last lines logged before the reset.
void PrintCrashReport() {
  CoreDump::Begin();
  esp_reset_reason_t reason = board.GetResetReason();
  bool crash = reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT ||
               reason == ESP_RST_WDT || reason == ESP_RST_BROWNOUT || reason == ESP_RST_UNKNOWN;
  const String &previous = CrashLog::PreviousRun();
  if (crash && previous.length() > 0) {
    Log.println("---- last lines before the reset ----");
    Log.print(previous);
    if (!previous.endsWith("\n")) {
      Log.println();
    }
    Log.println("---- end (console: crashlog) ----");
  }
}

// Boot order: docs/firmware-plan.md §3.2. Steps 1 to 10 here, 11 to 13 in BootState.
void setup() {
  // 1. Safe state (< 5 ms): amp muted, gain pins released, LEDs on, inputs,
  //    SW1, buttons and reset reason read.
  board.Begin();
  // A3: keep the log tail of the previous run (RTC memory) before any message.
  CrashLog::Begin(board.GetResetReason());

  // 2. NeoPixels: black frame and power cap before anything else draws current.
  light.Initialize();

  // 3. Console. Waiting for the host only in the bring-up build.
  Serial.begin(115200);
#ifdef TURRET_BRINGUP
  while (!Serial && millis() < 3000) {
    delay(10);
  }
#endif
  Log.println("This is a triumph");
  board.PrintBanner();
  PrintCrashReport();

  // 4. Settings (NVS), then LittleFS: mount only, never format automatically,
  //    a failed mount must stay visible instead of silently erasing fire.mp3.
  //    Upload it with: pio run -t uploadfs
  settings.Initialize();
  stats.Begin();
  if (board.IsFactoryResetRequested()) {
    Log.println("Settings: factory reset (A + B held at boot)");
    settings.ResetToDefaults();
  }
  light.ApplySettings(settings, board.IsReducedMode());
  // L1: from here on, each step can toggle the green LED (lab markers).
  board.SetLabMarkers(settings.GetBool(SettingId::LabMarkers));
  if (!LittleFS.begin(false)) {
    Log.println("LittleFS: mount failed (filesystem image not uploaded?)");
    board.SetFault(Fault::LittleFs, true);
  }
  Board::Mark("step 4 settings + LittleFS");

  // 5. I2C on IO35 / IO36 (explicit), bus recovery, scan.
  I2cBus::Begin();
  Board::Mark("step 5 I2C");

  // 6. IMU.
  motion.Initialize(settings);
  board.SetFault(Fault::Imu, !motion.IsAvailable());
  Board::Mark("step 6 IMU");

  // 7. Radar: non blocking, it has been running since power-up.
  radar.Initialize();
  Board::Mark("step 7 radar");

  // 8. Hall sensors: initial wing state. No servo attached yet.
  gantry.Initialize(settings);
  Board::Mark("step 8 Hall");

  // 9. Audio: I2S clocks running, gain set with SD low, 10 ms, then SD high (no pop).
  audio.Initialize(settings, board.IsReducedMode());
  Board::Mark("step 9 audio");

  // 10. WiFi + web server + OTA, before the servos: RF calibration draws a
  //     current peak that must not add up with theirs.
  actions.Initialize(turret, stateMachine, accessPoint, station);
  // Lot IM: OLED on J11. Debug mode (SW1) = menu, buttons A / B navigate when
  // a screen is there; normal mode = status page only.
  selfTest.Initialize(turret, stateMachine, actions);
  actions.SetSelfTest(&selfTest);
  debugUi.Initialize(turret, stateMachine, actions, accessPoint, station, selfTest);
  actions.SetDebugUi([](logic::MenuKey key) { debugUi.Key(key); }, []() { return debugUi.ScreenText(); },
                     []() { return debugUi.HasDisplay(); });
  actions.SetSettingsListener([]() { debugUi.ApplySettings(); });
  server.SetScreenProvider([]() { return debugUi.ScreenJson(); });
  board.SetNavigationButtons(debugUi.IsMenuActive() && debugUi.HasDisplay());
  accessPoint.Start(settings);
  station.Start(settings); // home network + NTP, if StaSsid is set
  server.Initialize(settings, actions, station);
  ota.Initialize(server.webServer, actions, TurretWebServer::USERNAME, server.GetPassword());
  Board::Mark("step 10 WiFi + web");

  // 11 to 13: BootState (servos one by one, homing, then Idle).
  stateMachine.Initialize(turret);
  stateMachine.GoToState(StateId::Booting);

  // A4: task watchdog on loop(). A loop blocked for 8 s (I2C, radar, audio...)
  // panics: the turret restarts, the core dump and the crash log tell why.
  esp_task_wdt_init(LOOP_WATCHDOG_S, true);
  enableLoopWDT();

  Log.println("Console ready: type help");
  prevTime = millis();
}

// D3: A = demo cycle, B short = mute / unmute, B long = WiFi on / off.
void HandleButtons() {
  ButtonEvent a = board.buttonA.TakeEvent();
  ButtonEvent b = board.buttonB.TakeEvent();
#ifdef TURRET_BRINGUP
  if (a != ButtonEvent::None) {
    Log.printf("Button A: %s\n", a == ButtonEvent::LongPress ? "long" : "short");
  }
  if (b != ButtonEvent::None) {
    Log.printf("Button B: %s\n", b == ButtonEvent::LongPress ? "long" : "short");
  }
#endif
  // Debug mode with a screen: A and B drive the menu (A next / previous,
  // B enter / back) instead of their normal functions.
  if (debugUi.IsMenuActive() && debugUi.HasDisplay()) {
    if (a != ButtonEvent::None) {
      debugUi.Key(a == ButtonEvent::LongPress ? logic::MenuKey::ALong : logic::MenuKey::A);
    }
    if (b != ButtonEvent::None) {
      debugUi.Key(b == ButtonEvent::LongPress ? logic::MenuKey::BLong : logic::MenuKey::B);
    }
    return;
  }
  if (a == ButtonEvent::ShortPress && stateMachine.GetCurrentStateId() == StateId::Idle) {
    Log.println(actions.Execute("demo"));
  }
  if (b == ButtonEvent::ShortPress) {
    Log.println(actions.Execute("mute"));
  }
  if (b == ButtonEvent::LongPress) {
    Log.println(actions.Execute(accessPoint.IsOn() ? "wifi off" : "wifi on"));
  }
}

void UpdateFaults() {
  // Statistics: a target that appears on the radar.
  static uint8_t lastTargetCount = 0;
  uint8_t targetCount = radar.GetTargetCount();
  if (targetCount > lastTargetCount) {
    stats.CountTarget();
  }
  lastTargetCount = targetCount;

  bool radarDown = millis() > RADAR_TIMEOUT_MS && !radar.IsAlive(RADAR_TIMEOUT_MS);
  board.SetFault(Fault::Radar, radarDown);
  board.SetFault(Fault::Hall, gantry.HasHallFault());

  // Plan §4: PWR_FLT low, even briefly (interrupt) -> shed the load at once.
  bool powerFaultEvent = board.TakePowerFaultEvent();
  if (powerFaultEvent) {
    // L5: timestamped, with what was running, before the load is shed.
    String when = Station::HasTime() ? " (" + Station::LocalTime() + ")" : String("");
    Log.printf("PWR_FLT event #%u at %lu ms%s: %s\n", (unsigned)board.GetPowerFaultCount(), millis(),
               when.c_str(), actions.LoadSnapshot().c_str());
  }
  bool powerFault = powerFaultEvent || board.IsPowerFault();
  if (powerFault && stateMachine.GetCurrentStateId() != StateId::Fault) {
    stateMachine.GoToState(StateId::Fault);
  }
#ifdef TURRET_BRINGUP
  static bool lastPowerFault = false;
  bool powerFaultLevel = board.IsPowerFault();
  if (powerFaultLevel != lastPowerFault) {
    lastPowerFault = powerFaultLevel;
    Log.printf("PWR_FLT: %s\n", powerFaultLevel ? "LOW (fault)" : "high (ok)");
  }
#endif
}

void loop() {
  ulong currentTime = millis();
  ulong deltaTime = currentTime - prevTime;

  prevTime = currentTime;

  board.Update(deltaTime);
  HandleButtons();
  UpdateFaults();
  actions.Update(); // console, web commands, OTA shutdown, reboot
  station.Update();
  accessPoint.Update(); // captive portal DNS
  selfTest.Update(currentTime);
  debugUi.Update(currentTime);

  gantry.Update(deltaTime);
  light.Update(deltaTime);
  motion.Update(deltaTime);
  radar.Update(deltaTime);
  audio.Update(deltaTime);
  ota.Update(deltaTime);

  stateMachine.Update(deltaTime);
}
