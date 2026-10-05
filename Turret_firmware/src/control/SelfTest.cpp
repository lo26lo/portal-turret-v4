#include "SelfTest.h"

#include "board/Log.h"
#include "control/Actions.h"
#include "logic/HallLogic.h"
#include "states/StateMachine.h"
#include <WiFi.h>

namespace {
enum Entry : uint8_t {
  EntryFlash,
  EntryPwrFlt,
  EntryImu,
  EntryGravity,
  EntryRadar,
  EntryHallLeft,
  EntryHallRight,
  EntryLittleFs,
  EntryOled,
  EntryWifi,
  EntryAmp,
  // Active tests, in this order.
  EntryLeds,
  EntrySound,
  EntryRotZ,
  EntryRotX,
  EntryGunLeft,
  EntryGunRight,
  EntryWingLeft,
  EntryWingRight,
};

const char *NAMES_EN[SelfTest::ENTRY_COUNT] = {
    "flash 8 MB, no PSRAM", "PWR_FLT high", "IMU answers", "gravity ~9.8", "radar frames",
    "Hall left", "Hall right", "LittleFS", "OLED", "WiFi access point", "amplifier",
    "LEDs (look)", "sound (listen)", "servo rotate Z", "servo rotate X", "servo gun left",
    "servo gun right", "wing left", "wing right"};
const char *NAMES_FR[SelfTest::ENTRY_COUNT] = {
    "flash 8 Mo, sans PSRAM", "PWR_FLT haut", "IMU répond", "gravité ~9,8", "trames radar",
    "Hall gauche", "Hall droit", "LittleFS", "OLED", "point d'accès WiFi", "ampli",
    "LEDs (regarder)", "son (écouter)", "servo rotation Z", "servo rotation X", "servo canon G",
    "servo canon D", "aile gauche", "aile droite"};
// Console names of the servos tested one by one (EntryRotZ .. EntryGunRight).
const char *SERVO_NAMES[] = {"rotz", "rotx", "gunl", "gunr"};

const ulong WING_TIMEOUT_MS = 5000; // attach wait + 2 s movement timeout, with margin
} // namespace

void SelfTest::Initialize(Turret &turretIn, StateMachine &stateMachineIn, Actions &actionsIn) {
  turret = &turretIn;
  stateMachine = &stateMachineIn;
  actions = &actionsIn;
}

String SelfTest::Run(const char *command) {
  String answer = actions->Execute(String(command));
  Log.printf("selftest> %s: %s\n", command, answer.c_str());
  return answer;
}

String SelfTest::Start(bool quick, bool displayPresent) {
  if (running) {
    return "self-test already running";
  }
  StateId state = stateMachine->GetCurrentStateId();
  if (!quick && (state == StateId::Booting || state == StateId::Fault)) {
    return state == StateId::Fault ? "refused: power fault" : "refused: still booting";
  }
  for (uint8_t i = 0; i < ENTRY_COUNT; i++) {
    results[i] = quick && i >= EntryLeds ? Result::Skip : Result::Pending;
  }
  hasRun = true;
  RunPassiveChecks(displayPresent);
  if (quick) {
    Log.println("Self-test (quick):");
    Log.print(Report(false));
    return Summary(false);
  }
  running = true;
  entry = EntryLeds;
  sub = 0;
  waitUntil = millis();
  faultsAtStart = turret->board.GetPowerFaultCount();
  return "self-test started (report: selftest report)";
}

void SelfTest::RunPassiveChecks(bool displayPresent) {
  Board &board = turret->board;
  Motion &motion = turret->motion;
  auto pass = [](bool ok) { return ok ? Result::Ok : Result::Fail; };

  Set(EntryFlash, pass(ESP.getFlashChipSize() == 8u * 1024 * 1024 && ESP.getPsramSize() == 0));
  Set(EntryPwrFlt, pass(!board.IsPowerFault()));
  Set(EntryImu, pass(motion.IsAvailable()));
  if (motion.IsAvailable()) {
    const sensors_vec_t &g = motion.GetAcceleration();
    float magnitude = sqrtf(g.x * g.x + g.y * g.y + g.z * g.z);
    Set(EntryGravity, pass(magnitude > 8.0f && magnitude < 11.5f));
  } else {
    Set(EntryGravity, Result::Skip);
  }
  Set(EntryRadar, pass(turret->radar.IsAlive(3000)));
  // A sensor at a rail (0 or 4095) is unplugged or shorted.
  Wing &left = turret->gantry.GetWingLeft();
  Wing &right = turret->gantry.GetWingRight();
  Set(EntryHallLeft, pass(!left.HasHallFault() && !logic::HallAtRail(left.GetLastHall())));
  Set(EntryHallRight, pass(!right.HasHallFault() && !logic::HallAtRail(right.GetLastHall())));
  Set(EntryLittleFs, pass(!board.HasFault(Fault::LittleFs)));
  Set(EntryOled, displayPresent ? Result::Ok : Result::Skip); // optional part
  Set(EntryWifi, pass((WiFi.getMode() & WIFI_AP) != 0));
  Set(EntryAmp, turret->audio.amp.IsMuted() ? Result::Skip : pass(turret->audio.amp.IsRunning()));
}

void SelfTest::Stop() {
  if (running) {
    Finish("stopped");
  }
}

void SelfTest::Next(ulong now) {
  entry++;
  sub = 0;
  waitUntil = now;
  if (entry >= ENTRY_COUNT) {
    Finish(nullptr);
  }
}

void SelfTest::Finish(const char *reason) {
  running = false;
  for (uint8_t i = 0; i < ENTRY_COUNT; i++) {
    if (results[i] == Result::Pending) {
      results[i] = Result::Skip;
    }
  }
  // No Run() on a power fault: the Fault state has shed the load already.
  if (stateMachine->GetCurrentStateId() != StateId::Fault) {
    Run("servos off");
    Run("led off");
    // The tests stopped the state machine; in normal mode, home and go back to Idle.
    if (!turret->board.IsBenchMode()) {
      Run("resume");
    }
  }
  Log.printf("Self-test %s: %s\n", reason != nullptr ? reason : "done", Summary(false).c_str());
  Log.print(Report(false));
}

void SelfTest::Update(ulong now) {
  if (!running) {
    return;
  }
  if (turret->board.GetPowerFaultCount() != faultsAtStart ||
      stateMachine->GetCurrentStateId() == StateId::Fault) {
    Set(entry, Result::Fail);
    Finish("aborted, power fault during the step");
    return;
  }
  if ((long)(now - waitUntil) < 0) {
    return;
  }

  if (entry == EntryLeds) {
    static const char *colours[] = {"led all FF0000", "led all 00FF00", "led all 0000FF"};
    if (sub < 3) {
      Run(colours[sub++]);
      waitUntil = now + 400;
    } else {
      Run("led off");
      Set(EntryLeds, Result::Check);
      Next(now);
    }
  } else if (entry == EntrySound) {
    if (turret->audio.amp.IsMuted()) {
      Set(EntrySound, Result::Skip);
      Next(now);
    } else if (sub == 0) {
      Run("tone 1000 400");
      sub = 1;
      waitUntil = now + 600;
    } else {
      Set(EntrySound, Result::Check);
      Next(now);
    }
  } else if (entry >= EntryRotZ && entry <= EntryGunRight) {
    // Attach, move a little, come back, release. OK = the eFuse did not react.
    const char *name = SERVO_NAMES[entry - EntryRotZ];
    char command[24];
    if (sub == 0) {
      snprintf(command, sizeof(command), "servo %s 100", name);
      if (Run(command).startsWith("refused")) {
        Set(entry, Result::Skip);
        Next(now);
        return;
      }
      sub = 1;
      waitUntil = now + 500;
    } else if (sub == 1) {
      snprintf(command, sizeof(command), "servo %s 90", name);
      Run(command);
      sub = 2;
      waitUntil = now + 400;
    } else {
      snprintf(command, sizeof(command), "servo %s off", name);
      Run(command);
      Set(entry, Result::Ok);
      Next(now);
    }
  } else {
    // Wings: open then close, each must end on its Hall threshold, not on the timeout.
    bool leftSide = entry == EntryWingLeft;
    Wing &wing = leftSide ? turret->gantry.GetWingLeft() : turret->gantry.GetWingRight();
    if (sub == 0) {
      if (!turret->motion.CanDeploy() || wing.HasHallFault()) {
        Set(entry, Result::Skip);
        Next(now);
        return;
      }
      wingFailed = false;
      if (Run(leftSide ? "wing left open" : "wing right open").startsWith("refused")) {
        Set(entry, Result::Skip);
        Next(now);
        return;
      }
      sub = 1;
      stepStarted = now;
    } else if (wing.IsMoving() && now - stepStarted < WING_TIMEOUT_MS) {
      return; // still moving
    } else if (sub == 1) {
      wingFailed = wing.LastMoveTimedOut() || wing.IsMoving();
      Run(leftSide ? "wing left close" : "wing right close");
      sub = 2;
      stepStarted = now;
    } else {
      wingFailed = wingFailed || wing.LastMoveTimedOut() || wing.IsMoving();
      Set(entry, wingFailed ? Result::Fail : Result::Ok);
      Next(now);
    }
  }
}

// ------------------------------------------------------------------ report

const char *SelfTest::ResultLabel(Result result, bool french) {
  switch (result) {
  case Result::Ok: return "OK";
  case Result::Fail: return french ? "ECHEC" : "FAIL";
  case Result::Skip: return french ? "passé" : "SKIP";
  case Result::Check: return french ? "voir" : "CHECK";
  default: return "...";
  }
}

uint8_t SelfTest::Count(Result result) const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < ENTRY_COUNT; i++) {
    n += results[i] == result ? 1 : 0;
  }
  return n;
}

String SelfTest::Summary(bool french) const {
  return String(Count(Result::Ok)) + " OK " + String(Count(Result::Fail)) + (french ? " ECHEC " : " FAIL ") +
         String(Count(Result::Check)) + (french ? " voir " : " CHECK ") + String(Count(Result::Skip)) +
         (french ? " passé" : " SKIP");
}

String SelfTest::Report(bool french) const {
  String report;
  for (uint8_t i = 0; i < ENTRY_COUNT; i++) {
    char line[48];
    snprintf(line, sizeof(line), "%-6s%s\n", ResultLabel(results[i], false), (french ? NAMES_FR : NAMES_EN)[i]);
    report += line;
  }
  return report;
}

const char *SelfTest::CurrentName(bool french) const {
  return running && entry < ENTRY_COUNT ? (french ? NAMES_FR : NAMES_EN)[entry] : "";
}

const char *SelfTest::NameWith(Result result, uint8_t ordinal, bool french) const {
  for (uint8_t i = 0; i < ENTRY_COUNT; i++) {
    if (results[i] == result && ordinal-- == 0) {
      return (french ? NAMES_FR : NAMES_EN)[i];
    }
  }
  return "";
}
