#pragma once

#include <Arduino.h>

#include "Turret.h"
#include "logic/MenuLogic.h"
#include "states/StateId.h"
#include "ui/Display.h"
#include "web/AccessPoint.h"
#include "web/Station.h"

class Actions;
class SelfTest;
class StateMachine;

// Debug screen (lot IM).
//  - Debug mode (SW1 closed at power-up): menu of information pages, tests,
//    calibration and settings, driven by buttons A and B (or the "key"
//    command / the web page). See logic/MenuLogic.h for the keys.
//  - Normal mode with a screen plugged in: the eye of the turret with its
//    subtitles (OledFace), or a plain status page; the buttons keep their
//    normal functions.
//  - In both modes: wing animation (OledAnim), screen off after OledSleepS
//    without activity, rotation and contrast (OledFlip, OledContrast).
// The screen is a logic::MenuScreen: the OLED on J11 draws it, and the web
// page shows its text lines (GET /api/screen), with or without a real screen.
class DebugUi {
public:
  // After I2cBus::Begin() and the settings: looks for the OLED.
  void Initialize(Turret &turret, StateMachine &stateMachine, Actions &actions, AccessPoint &accessPoint,
                  Station &station, SelfTest &selfTest);
  // loop(): redraws at most 5 times per second (10 during the animation);
  // the OLED only gets a frame when something changed.
  void Update(ulong now);
  void Key(logic::MenuKey key);
  // Reads OledFlip and OledContrast again.
  void ApplySettings();

  // Debug mode: the menu is active and the buttons navigate.
  bool IsMenuActive() const;
  bool HasDisplay() const { return display.IsPresent(); }

  // {"present":..,"menu":..,"on":..,"graphic":n,"highlight":n,"lines":[6 strings]} - any task.
  String ScreenJson();
  // The six lines, for the console ("screen" command).
  String ScreenText();

private:
  static void ExecuteHook(void *context, const char *command, char *answer, size_t size);
  static int32_t ValueHook(void *context, const char *key);
  static void InfoHook(void *context, int16_t page, uint8_t line, logic::Lang lang, char *text, size_t size);
  static void GraphicHook(void *context, int16_t page, logic::MenuScreen &screen);

  void InfoLine(int16_t page, uint8_t line, bool french, char *text, size_t size);
  void InfoGraphic(int16_t page, logic::MenuScreen &screen);
  void RenderStatus(logic::MenuScreen &screen, bool french);
  void RenderFace(logic::MenuScreen &screen, bool french, ulong now);
  void FillQr(logic::MenuScreen &screen, const char *text);
  void SampleHall(ulong now);
  void UpdateSleep(ulong now, bool moving);
  void Wake(ulong now);
  logic::Lang Language() const;

  Turret *turret = nullptr;
  StateMachine *stateMachine = nullptr;
  Actions *actions = nullptr;
  AccessPoint *accessPoint = nullptr;
  Station *station = nullptr;
  SelfTest *selfTest = nullptr;

  Display display;
  logic::MenuNav *nav = nullptr;
  logic::MenuScreen shown = {};
  ulong lastRender = 0;
  bool dirty = true;
  bool animating = false;
  ulong animationUntil = 0;

  // Sleep
  ulong lastActivity = 0;
  bool asleep = false;
  StateId lastState = StateId::Booting;

  // Subtitles of the eye
  const char *subtitle = "";
  ulong subtitleUntil = 0;

  // Hall curves: one sample every 50 ms, 100 samples = 5 s, value / 16 (0..255)
  static const uint8_t GRAPH_SAMPLES = 100;
  uint8_t hallSamples[2][GRAPH_SAMPLES] = {};
  uint8_t hallHead = 0;
  ulong lastSample = 0;

  // QR code cache (generated once per text)
  char qrText[160] = "";
};
