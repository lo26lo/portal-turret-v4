#pragma once

#include "audio/Audio.h"
#include "board/Board.h"
#include "light/Light.h"
#include "motion/Gantry.h"
#include "sensors/Motion.h"
#include "sensors/Radar.h"
#include "settings/Settings.h"
#include "web/TurretWebServer.h"

struct Turret {
  Gantry &gantry;
  Motion &motion;
  Radar &radar;
  Audio &audio;
  Light &light;
  TurretWebServer &server;
  Settings &settings;
  Board &board;
};