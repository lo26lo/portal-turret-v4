#include "Turret.h"
#include "board/Board.h"
#include "board/I2cBus.h"
#include "board/Log.h"
#include "control/Actions.h"
#include "pins.h"
#include "states/StateMachine.h"
#include "web/AccessPoint.h"
#include "web/Station.h"
#include "web/Ota.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

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

// No radar frame for this long -> red LED code 4 (plan §3.2 step 7).
const ulong RADAR_TIMEOUT_MS = 3000;

Turret turret{gantry, motion, radar, audio, light, server, settings, board};

// Boot order: docs/firmware-plan.md §3.2. Steps 1 to 10 here, 11 to 13 in BootState.
void setup() {
  // 1. Safe state (< 5 ms): amp muted, gain pins released, LEDs on, inputs,
  //    SW1, buttons and reset reason read.
  board.Begin();

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

  // 4. Settings (NVS), then LittleFS: mount only, never format automatically,
  //    a failed mount must stay visible instead of silently erasing fire.mp3.
  //    Upload it with: pio run -t uploadfs
  settings.Initialize();
  if (board.IsFactoryResetRequested()) {
    Log.println("Settings: factory reset (A + B held at boot)");
    settings.ResetToDefaults();
  }
  light.ApplySettings(settings, board.IsReducedMode());
  if (!LittleFS.begin(false)) {
    Log.println("LittleFS: mount failed (filesystem image not uploaded?)");
    board.SetFault(Fault::LittleFs, true);
  }

  // 5. I2C on IO35 / IO36 (explicit), bus recovery, scan.
  I2cBus::Begin();

  // 6. IMU.
  motion.Initialize(settings);
  board.SetFault(Fault::Imu, !motion.IsAvailable());

  // 7. Radar: non blocking, it has been running since power-up.
  radar.Initialize();

  // 8. Hall sensors: initial wing state. No servo attached yet.
  gantry.Initialize(settings);

  // 9. Audio: I2S clocks running, gain set with SD low, 10 ms, then SD high (no pop).
  audio.Initialize(settings, board.IsReducedMode());

  // 10. WiFi + web server + OTA, before the servos: RF calibration draws a
  //     current peak that must not add up with theirs.
  actions.Initialize(turret, stateMachine, accessPoint, station);
  accessPoint.Start(settings);
  station.Start(settings); // home network + NTP, if StaSsid is set
  server.Initialize(settings, actions, station);
  ota.Initialize(server.webServer, actions, TurretWebServer::USERNAME, server.GetPassword());

  // 11 to 13: BootState (servos one by one, homing, then Idle).
  stateMachine.Initialize(turret);
  stateMachine.GoToState(StateId::Booting);

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
  bool radarDown = millis() > RADAR_TIMEOUT_MS && !radar.IsAlive(RADAR_TIMEOUT_MS);
  board.SetFault(Fault::Radar, radarDown);
  board.SetFault(Fault::Hall, gantry.HasHallFault());

  // Plan §4: PWR_FLT low, even briefly (interrupt) -> shed the load at once.
  bool powerFault = board.TakePowerFaultEvent() || board.IsPowerFault();
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

  gantry.Update(deltaTime);
  light.Update(deltaTime);
  motion.Update(deltaTime);
  radar.Update(deltaTime);
  audio.Update(deltaTime);
  ota.Update(deltaTime);

  stateMachine.Update(deltaTime);
}
