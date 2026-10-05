#pragma once

#include <Arduino.h>

#include "Turret.h"
#include "logic/MenuLogic.h"
#include "ui/Display.h"
#include "web/AccessPoint.h"
#include "web/Station.h"

class Actions;
class StateMachine;

// Debug screen (lot IM).
//  - Debug mode (SW1 closed at power-up): menu of information pages, tests,
//    calibration and settings, driven by buttons A and B (or the "key"
//    command / the web page). See logic/MenuLogic.h for the keys.
//  - Normal mode with a screen plugged in: a single status page, the buttons
//    keep their normal functions.
// The screen is six lines of text; the OLED on J11 shows them, and so does
// the web page (GET /api/screen), with or without a real screen.
class DebugUi {
public:
  // After I2cBus::Begin() and the settings: looks for the OLED.
  void Initialize(Turret &turret, StateMachine &stateMachine, Actions &actions, AccessPoint &accessPoint,
                  Station &station);
  // loop(): redraws at most 5 times per second, sends to the OLED only on a change.
  void Update(ulong now);
  void Key(logic::MenuKey key);

  // Debug mode: the menu is active and the buttons navigate.
  bool IsMenuActive() const;
  bool HasDisplay() const { return display.IsPresent(); }

  // {"present":..,"menu":..,"highlight":n,"lines":[6 strings]} - any task.
  String ScreenJson();
  // The six lines, for the console ("screen" command).
  String ScreenText();

private:
  static void ExecuteHook(void *context, const char *command, char *answer, size_t size);
  static int32_t ValueHook(void *context, const char *key);
  static void InfoHook(void *context, int16_t page, uint8_t line, logic::Lang lang, char *text, size_t size);

  void InfoLine(int16_t page, uint8_t line, bool french, char *text, size_t size);
  void RenderStatus(logic::MenuScreen &screen, bool french);
  logic::Lang Language() const;

  Turret *turret = nullptr;
  StateMachine *stateMachine = nullptr;
  Actions *actions = nullptr;
  AccessPoint *accessPoint = nullptr;
  Station *station = nullptr;

  Display display;
  logic::MenuNav *nav = nullptr;
  logic::MenuScreen shown = {};
  ulong lastRender = 0;
  bool dirty = true;
  bool animating = false;
  ulong animationUntil = 0;
};
