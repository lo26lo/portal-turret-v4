#include "Turret.h"
#include "board/Board.h"
#include "board/BoardCheck.h"
#include "board/Power.h"
#include "board/CoreDump.h"
#include "board/I2cBus.h"
#include "board/Log.h"
#include "control/Actions.h"
#include "control/SelfTest.h"
#include "control/Sleep.h"
#include "control/Stats.h"
#include "pins.h"
#include "sensors/RotaryEncoder.h"
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
RotaryEncoder encoder;
Sleep sleepControl; // lot F2.6 (not "sleep": the POSIX function)

// Rotary encoder: long press (debug menu: back), volume step per detent (normal mode).
const uint32_t ENCODER_LONG_PRESS_MS = 600;
const int32_t VOLUME_STEP = 5;

// No radar frame for this long -> red LED code 4 (plan §3.2 step 7).
const ulong RADAR_TIMEOUT_MS = 3000;

// A4: loop() must come back within this time, or the task watchdog panics.
const uint32_t LOOP_WATCHDOG_S = 8;

Turret turret{gantry, motion, radar, audio, light, server, settings, board, stats};

// Voice line when the turret is knocked over (IMU, ImuUpAxis calibrated).
logic::TipWatch tipWatch;

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
  // 0. Board check (F2.2), before any other pin: I2C (IO35 / IO36 on both
  //    boards) and the v2 rail expander at 0x20, rails off if it is there.
  //    Wrong board: console only, nothing else is touched (never returns).
  BoardCheck::Run();
  if (!BoardCheck::IsRightBoard()) {
    BoardCheck::HaltWrongBoard();
  }

  // 1. Safe state (< 5 ms): amp muted, gain pins released, LEDs on, inputs,
  //    SW1, buttons and reset reason read.
  board.Begin();
  // A3: keep the log tail of the previous run (RTC memory) before any message.
  CrashLog::Begin(board.GetResetReason());
  // F2.6: a patrol from the deep sleep boots quiet (rails off, radar apart).
  bool quietBoot = Sleep::BeginBoot();

  // 2. NeoPixels: black frame and power cap before anything else draws current.
  //    v2: LED rail on first (its data lines are held low by R22-R24 until then).
  if (!quietBoot) {
    Power::On(Rail::Led);
  }
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
  BoardCheck::PrintReport();
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
  // Also the status LED modes, used once the boot is done.
  board.ApplySettings(settings);
  if (!LittleFS.begin(false)) {
    Log.println("LittleFS: mount failed (filesystem image not uploaded?)");
    board.SetFault(Fault::LittleFs, true);
  }
  Board::Mark("step 4 settings + LittleFS");

  // 5. I2C on IO35 / IO36 (explicit; started at step 0), bus recovery logged, scan.
  I2cBus::Begin();
  // Optional rotary encoder on J11 (Adafruit 4991 / 5752, Soldered 333188).
  encoder.Begin();
  encoder.SetLongPressMs(ENCODER_LONG_PRESS_MS);
  Board::Mark("step 5 I2C");

  // 6. IMU.
  motion.Initialize(settings);
  board.SetFault(Fault::Imu, !motion.IsAvailable());
  Board::Mark("step 6 IMU");

  // 7. Radar: non blocking. v0.1: it has been running since power-up; v2: its
  //    rail goes on now, before UART1 drives the TX line.
  Power::On(Rail::Radar);
  radar.Initialize();
  radar.ApplySettings(settings); // detection zone (DetectMaxMm, DetectAngle, MaxSpeedCms)
  Board::Mark("step 7 radar");

  // 8. Hall sensors: initial wing state. No servo attached yet. v2: Hall rail on first.
  if (!quietBoot) {
    Power::On(Rail::Hall);
  }
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
  ota.Initialize(server.webServer, actions, server.GetUsername(), server.GetPassword());
  Board::Mark("step 10 WiFi + web");

  // v2: servo rail on, last (inrush of C9 + C18 after the RF calibration).
  // Reduced mode (reboot loop, maybe the supply): kept off, "rail servo on" to override.
  if (quietBoot) {
    Log.println("Power: patrol, servo rail kept off");
  } else if (board.IsReducedMode() && Power::IsSwitchable()) {
    Log.println("Power: reduced mode, servo rail kept off (console: rail servo on)");
  } else {
    Power::On(Rail::Servo);
  }

  // 11 to 13: BootState (servos one by one, homing, then Idle).
  stateMachine.Initialize(turret);
  stateMachine.GoToState(StateId::Booting);
  // F2.6: sleep modes (settings: group Sleep). A patrol starts in radar sleep.
  sleepControl.Initialize(turret, stateMachine, actions, accessPoint, station);
  actions.SetSleep(&sleepControl);

  // A4: task watchdog on loop(). A loop blocked for 8 s (I2C, radar, audio...)
  // panics: the turret restarts, the core dump and the crash log tell why.
  esp_task_wdt_init(LOOP_WATCHDOG_S, true);
  enableLoopWDT();

  Log.println("Console ready: type help");
  prevTime = millis();
}

// D3: A = demo cycle, B short = mute / unmute, B long = WiFi on / off (v0.1).
// F2.5, both boards: PROG held 3 s (SW1 open) = WiFi on / off; v2: also the
// remote button (J20) held 3 s.
void HandleButtons() {
  // PROG alone or the v2 remote button (J20): any press wakes the radar sleep up.
  if (board.TakeUserPress()) {
    sleepControl.NoteActivity();
  }
  if (board.TakeProgEvent() == ButtonEvent::LongPress) {
    Log.println(actions.Execute(accessPoint.IsOn() ? "wifi off" : "wifi on"));
  }
  ButtonEvent a = board.buttonA.TakeEvent();
  ButtonEvent b = board.buttonB.TakeEvent();
  if (a != ButtonEvent::None || b != ButtonEvent::None) {
    sleepControl.NoteActivity();
  }
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

// Rotary encoder. Debug mode with a screen: it drives the menu like the
// buttons (turn = next / previous or value -/+, push = enter, push held =
// back). Otherwise: turn = volume, push = mute / unmute, shown on the screen,
// push held = demo cycle (F2.5: the v2 has no button A).
void HandleEncoder() {
  int32_t steps = encoder.TakeSteps();
  ButtonEvent push = encoder.TakeEvent();
  if (steps == 0 && push == ButtonEvent::None) {
    return;
  }
  sleepControl.NoteActivity();
  if (debugUi.IsMenuActive() && debugUi.HasDisplay()) {
    for (; steps > 0; steps--) {
      debugUi.Key(logic::MenuKey::Clockwise);
    }
    for (; steps < 0; steps++) {
      debugUi.Key(logic::MenuKey::Anticlockwise);
    }
    if (push != ButtonEvent::None) {
      debugUi.Key(push == ButtonEvent::LongPress ? logic::MenuKey::PushLong : logic::MenuKey::Push);
    }
    return;
  }
  if (push == ButtonEvent::LongPress) {
    Log.println(actions.Execute("demo"));
    if (steps == 0) {
      return;
    }
  }
  if (steps != 0) {
    actions.Execute("volume " + String(steps > 0 ? "+" : "") + String(steps * VOLUME_STEP));
  }
  if (push == ButtonEvent::ShortPress) {
    Log.println(actions.Execute("mute"));
  }
  debugUi.ShowVolume(audio.GetVolume(), audio.amp.IsMuted());
}

void UpdateFaults() {
  // Statistics: a target that appears on the radar.
  static uint8_t lastTargetCount = 0;
  uint8_t targetCount = radar.GetTargetCount();
  if (targetCount > lastTargetCount) {
    stats.CountTarget();
  }
  lastTargetCount = targetCount;

  // Counted from the start of the radar (v2: its rail may go off and on).
  bool radarDown =
      radar.IsRunning() && millis() - radar.GetStartedAt() > RADAR_TIMEOUT_MS && !radar.IsAlive(RADAR_TIMEOUT_MS);
  board.SetFault(Fault::Radar, radarDown);
  board.SetFault(Fault::Hall, gantry.HasHallFault());

  // Plan §4: PWR_FLT low, even briefly (interrupt) -> shed the load at once.
  bool powerFaultEvent = board.TakePowerFaultEvent();
  if (powerFaultEvent) {
    // L5: timestamped, with what was running, before the load is shed.
    String when = Station::HasTime() ? " (" + Station::LocalTime() + ")" : String("");
    Log.printf("PWR_FLT event #%u at %lu ms%s: %s\n", (unsigned)board.GetPowerFaultCount(), millis(), when.c_str(),
               actions.LoadSnapshot().c_str());
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
  // SW1 closed + PROG held 5 s: everything back to factory state, then a restart.
  if (board.TakeFactoryResetRequest()) {
    Log.println(actions.Execute("factory-reset"));
  }
  HandleButtons();
  encoder.Update(currentTime);
  HandleEncoder();
  UpdateFaults();
  actions.Update(); // console, web commands, OTA shutdown, reboot
  station.Update();
  accessPoint.Update(); // captive portal DNS
  selfTest.Update(currentTime);
  debugUi.Update(currentTime);

  // Weak supply (debug mode without "power full"): positional servos ramp (ServoRamp).
  gantry.SetRampActive(board.IsServoLimited());
  // F2.6 Hall power: off asleep; on in debug mode (calibration) or with HallPower "Always".
  if (sleepControl.IsAsleep()) {
    gantry.SetHallPower(Gantry::HallPower::Off);
  } else if (board.IsBenchMode() || settings.GetInt(SettingId::HallPower) == 0) {
    gantry.SetHallPower(Gantry::HallPower::Always);
  } else {
    gantry.SetHallPower(Gantry::HallPower::WhileMoving);
  }
  gantry.Update(deltaTime);
  // B6, LedEffect "living eye": the ring follows the state machine.
  switch (stateMachine.GetCurrentStateId()) {
  case StateId::Activate: light.SetMood(Light::Mood::Acquired); break;
  case StateId::FiringState: light.SetMood(Light::Mood::Firing); break;
  case StateId::Searching: light.SetMood(Light::Mood::Searching); break;
  default: light.SetMood(Light::Mood::Idle); break;
  }
  light.Update(deltaTime);
  motion.Update(deltaTime);
  if (tipWatch.Update(motion.CanCheckUpright(), motion.IsUpright(), currentTime)) {
    audio.PlayVoice(Voice::Tipped);
  }
  radar.Update(deltaTime);
  audio.Update(deltaTime);
  ota.Update(deltaTime);

  // Before the state machine: a target wakes the radar sleep up (rails on)
  // before Idle reacts to it.
  sleepControl.Update(currentTime);
  stateMachine.Update(deltaTime);
}
