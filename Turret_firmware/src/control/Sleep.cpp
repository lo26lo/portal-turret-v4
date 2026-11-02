#include "Sleep.h"

#include "Turret.h"
#include "board/Log.h"
#include "board/Power.h"
#include "control/Actions.h"
#include "pins.h"
#include "states/StateMachine.h"
#include "web/AccessPoint.h"
#include "web/Station.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include <time.h>

namespace {
// Kept across the deep sleep (RTC slow memory), lost at power-up.
const uint32_t RTC_MAGIC = 0x51EE7A11;
RTC_DATA_ATTR uint32_t rtcMagic = 0;
RTC_DATA_ATTR bool rtcPatrol = false;       // the deep sleep has patrols
RTC_DATA_ATTR time_t rtcNightEndEpoch = 0;  // > 0: deep sleep for the night, until then

bool quietBoot = false;
StateId sleepState = StateId::Idle; // state when the radar sleep began

const char *LevelName(logic::SleepLevel level) {
  switch (level) {
  case logic::SleepLevel::Radar: return "radar";
  case logic::SleepLevel::Deep: return "deep";
  default: return "awake";
  }
}

// Local hour 0..23, -1 without a valid time (NTP through the home WiFi).
int LocalHour() {
  if (!Station::HasTime()) {
    return -1;
  }
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  return local.tm_hour;
}

uint32_t LocalSecondOfDay() {
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  return (uint32_t)(local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec);
}
} // namespace

bool Sleep::BeginBoot() {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  bool valid = rtcMagic == RTC_MAGIC;
  // A timer wake-up is a patrol, unless it is the end of the night.
  bool nightOver = rtcNightEndEpoch > 0 && time(nullptr) >= rtcNightEndEpoch;
  quietBoot = valid && cause == ESP_SLEEP_WAKEUP_TIMER && rtcPatrol && !nightOver;
  if (!quietBoot) {
    rtcPatrol = false;
    rtcNightEndEpoch = 0;
  }
  return quietBoot;
}

bool Sleep::IsQuietBoot() { return quietBoot; }

void Sleep::Initialize(Turret &turretIn, StateMachine &stateMachineIn, Actions &actionsIn, AccessPoint &accessPointIn,
                       Station &stationIn) {
  turret = &turretIn;
  stateMachine = &stateMachineIn;
  actions = &actionsIn;
  accessPoint = &accessPointIn;
  station = &stationIn;
  ulong now = millis();
  lastActivity = now;
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    Log.println("Sleep: woken up by PROG");
  } else if (cause == ESP_SLEEP_WAKEUP_EXT1) {
    uint64_t pins = esp_sleep_get_ext1_wakeup_status();
    bool button = PIN_BUTTON_EXT != PIN_ABSENT && (pins & (1ULL << PIN_BUTTON_EXT));
    Log.println(button ? "Sleep: woken up by the remote button" : "Sleep: woken up by the IMU (turret moved)");
  } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    Log.println(quietBoot ? "Sleep: patrol, looking with the radar" : "Sleep: end of the night");
  }
  if (quietBoot) {
    // setup() kept the rails off: the radar sleep starts at once.
    level = logic::SleepLevel::Radar;
    radarSleepSince = now;
    sleepState = StateId::Idle;
    ApplyRadarSleep();
    patrol = true;
    patrolUntil = now + (ulong)turret->settings.GetInt(SettingId::PatrolOnS) * 1000;
  }
}

void Sleep::NoteActivity() {
  lastActivity = millis();
  if (level == logic::SleepLevel::Radar) {
    WakeUp("key");
  }
}

bool Sleep::TargetInZone() const {
  Settings &settings = turret->settings;
  return turret->radar.FirstTargetInZone(settings.GetInt(SettingId::DetectMaxMm),
                                         settings.GetInt(SettingId::DetectAngle)) >= 0;
}

bool Sleep::IsNight() const {
  Settings &settings = turret->settings;
  return settings.GetBool(SettingId::NightSleep) &&
         logic::InNightWindow(LocalHour(), settings.GetInt(SettingId::NightStart), settings.GetInt(SettingId::NightEnd));
}

bool Sleep::DeepAllowed(String &refused) const {
  if (!Power::IsSwitchable()) {
    refused = "refused: no deep sleep on " BOARD_NAME " (5 V not switched, IMU cannot wake it)";
    return false;
  }
  if (turret->board.IsBenchMode()) {
    refused = "refused: debug mode (SW1 closed)";
    return false;
  }
  return true;
}

logic::SleepInputs Sleep::Inputs(ulong now) const {
  Settings &settings = turret->settings;
  logic::SleepInputs in;
  in.mode = (logic::SleepMode)settings.GetInt(SettingId::SleepMode);
  // Signed, clamped at 0: NoteActivity() stamps millis(), later than the
  // "now" of this loop (the bug of the OLED sleep, 02.11.2026).
  long idle = (long)(now - lastActivity);
  long asleep = (long)(now - radarSleepSince);
  in.idleMs = idle > 0 ? (uint32_t)idle : 0;
  in.inRadarSleepMs = level == logic::SleepLevel::Radar && asleep > 0 ? (uint32_t)asleep : 0;
  in.radarSleepAfterMs = (uint32_t)settings.GetInt(SettingId::RadarSleepMin) * 60000u;
  in.deepSleepAfterMs = (uint32_t)settings.GetInt(SettingId::DeepSleepMin) * 60000u;
  in.night = IsNight();
  String refused;
  in.deepAllowed = DeepAllowed(refused);
  return in;
}

void Sleep::Update(ulong now) {
  if (turret == nullptr) {
    return;
  }
  StateId state = stateMachine->GetCurrentStateId();
  bool wingMoving = turret->gantry.GetWingLeft().IsMoving() || turret->gantry.GetWingRight().IsMoving();

  if (level == logic::SleepLevel::Awake) {
    // Activity: a target, a cycle, a moving wing. Only Idle in normal mode sleeps on its own.
    if (state != StateId::Idle || wingMoving || TargetInZone() || turret->board.IsBenchMode()) {
      lastActivity = now;
      return;
    }
    if (logic::WantedSleepLevel(Inputs(now), level) == logic::SleepLevel::Radar) {
      EnterRadarSleep("no target");
    }
    return;
  }

  // Radar sleep: a target, or a state change (a cycle started from the web page).
  if (TargetInZone()) {
    WakeUp("target");
    return;
  }
  // Booting: the first loops of a patrol boot, before Idle.
  if (state != sleepState && state != StateId::Booting) {
    WakeUp("state change");
    return;
  }
  String refused;
  if (patrol && (long)(now - patrolUntil) >= 0) {
    if (!EnterDeepSleep("patrol: nobody", refused)) {
      Log.printf("Sleep: %s, staying in radar sleep\n", refused.c_str());
      patrol = false;
    }
    return;
  }
  if (!patrol && logic::WantedSleepLevel(Inputs(now), level) == logic::SleepLevel::Deep) {
    if (!EnterDeepSleep(IsNight() ? "night" : "DeepSleepMin", refused)) {
      // Not possible any more (SW1 closed since): stay in radar sleep, retry later.
      radarSleepSince = now;
    }
  }
}

void Sleep::EnterRadarSleep(const char *why) {
  level = logic::SleepLevel::Radar;
  radarSleepSince = millis();
  sleepState = stateMachine->GetCurrentStateId();
  Log.printf("Sleep: radar sleep (%s)\n", why);
  ApplyRadarSleep();
}

// Everything but the radar and the ESP32 sheds its load (plan v2 §1).
void Sleep::ApplyRadarSleep() {
  Audio &audio = turret->audio;
  audio.StopVoice();
  audio.ShootAudio.Stop();
  audio.amp.Shutdown(); // I2S clocks keep running: SD low is enough
  turret->gantry.DetachAll();
  Power::Off(Rail::Servo);
  turret->light.SetEnabled(false);
  Power::Off(Rail::Led);
  // Hall sensors: main sets the Hall power to Off while asleep.

  logic::SleepWifi wifi = (logic::SleepWifi)turret->settings.GetInt(SettingId::SleepWifi);
  wifiChanged = false;
  apWasOn = accessPoint->IsOn();
  staWasOn = station->IsEnabled();
  if (wifi != logic::SleepWifi::Unchanged && apWasOn) {
    accessPoint->Stop();
    wifiChanged = true;
  }
  if (wifi == logic::SleepWifi::Off && staWasOn) {
    station->Stop();
    wifiChanged = true;
  }
}

void Sleep::WakeUp(const char *why) {
  level = logic::SleepLevel::Awake;
  lastActivity = millis();
  patrol = false;
  Log.printf("Sleep: awake (%s)\n", why);
  // Power fault: FaultState keeps the load shed and restores it when it ends.
  if (stateMachine->GetCurrentStateId() != StateId::Fault) {
    // Rails one at a time, LEDs first, servos last (as at boot).
    Power::On(Rail::Led);
    turret->light.SetEnabled(true);
    if (!turret->board.IsReducedMode()) {
      Power::On(Rail::Servo);
    }
    turret->audio.amp.Start();
  }
  if (wifiChanged) {
    if (staWasOn && !station->IsEnabled()) {
      station->Start(turret->settings);
    }
    if (apWasOn && !accessPoint->IsOn()) {
      accessPoint->Start(turret->settings);
    }
    wifiChanged = false;
  }
}

bool Sleep::EnterDeepSleep(const char *why, String &refused) {
  if (!DeepAllowed(refused)) {
    return false;
  }
#if BOARD_HAS_EXPANDER
  Settings &settings = turret->settings;
  // Timer: the next patrol, or the end of the night, whichever comes first.
  uint64_t timerS = 0;
  uint32_t patrolS = (uint32_t)settings.GetInt(SettingId::PatrolPeriodS);
  if (patrolS > 0) {
    timerS = patrolS;
  }
  rtcNightEndEpoch = 0;
  if (IsNight()) {
    uint32_t untilEnd = logic::SecondsUntilHour(LocalSecondOfDay(), settings.GetInt(SettingId::NightEnd));
    rtcNightEndEpoch = time(nullptr) + untilEnd;
    if (timerS == 0 || untilEnd < timerS) {
      timerS = untilEnd;
    }
  }
  rtcPatrol = patrolS > 0;
  rtcMagic = RTC_MAGIC;
  bool wakeImu = settings.GetBool(SettingId::WakeImu);
  Log.printf("Sleep: deep sleep (%s); wake-up: PROG, remote button%s%s\n", why, wakeImu ? ", IMU" : "",
             timerS ? (String(", timer ") + String((uint32_t)timerS) + " s").c_str() : "");

  // Same order as a restart (amp off before the I2S stops, rails off after
  // their drivers), then the radio, the status LEDs and the IMU.
  actions->ShutdownForRestart();
  station->Stop();
  accessPoint->Stop();
  WiFi.mode(WIFI_OFF);
  turret->board.LedsOff();
  // EXT1 "any high": the remote button (J20, IO2, active high, R49 pull-down)
  // and INT1 of the IMU (IO14), latched high by a movement. Both RTC pins.
  uint64_t ext1 = 1ULL << PIN_BUTTON_EXT;
  if (wakeImu && turret->motion.ArmWakeup(logic::ImuWakeThreshold(settings.GetInt(SettingId::WakeImuLevel)))) {
    ext1 |= 1ULL << PIN_IMU_INT1;
  }
  esp_sleep_enable_ext1_wakeup(ext1, ESP_EXT1_WAKEUP_ANY_HIGH);
  // PROG (IO0, RTC pin, R21 pull-up): low = pressed.
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  if (timerS > 0) {
    esp_sleep_enable_timer_wakeup(timerS * 1000000ULL);
  }
  Serial.flush();
  delay(50);
  esp_deep_sleep_start(); // does not return
#else
  (void)why;
  refused = "refused: no deep sleep on " BOARD_NAME;
  return false;
#endif
}

String Sleep::Command(const String &args) {
  String what = args;
  what.trim();
  what.toLowerCase();
  if (what == "radar") {
    if (level != logic::SleepLevel::Awake) {
      return "already in radar sleep";
    }
    StateId state = stateMachine->GetCurrentStateId();
    if (state != StateId::Idle && state != StateId::Manual) {
      return "refused: only from Idle (or Manual in debug mode)";
    }
    EnterRadarSleep("command");
    return "radar sleep";
  }
  if (what == "deep") {
    if (level == logic::SleepLevel::Awake && stateMachine->GetCurrentStateId() != StateId::Idle) {
      return "refused: only from Idle or the radar sleep";
    }
    String refused;
    if (!EnterDeepSleep("command", refused)) {
      return refused;
    }
    return "deep sleep"; // not reached
  }
  if (what == "off") {
    lastActivity = millis();
    if (level == logic::SleepLevel::Awake) {
      return "awake";
    }
    WakeUp("command");
    return "awake";
  }
  if (what.length() > 0) {
    return "usage: sleep [radar|deep|off]";
  }
  return StatusJson();
}

String Sleep::StatusJson() const {
  if (turret == nullptr) {
    return "{}";
  }
  ulong now = millis();
  logic::SleepInputs in = Inputs(now);
  // Seconds before the next step, -1 if none is due on its own.
  long nextS = -1;
  if (level == logic::SleepLevel::Awake && in.mode != logic::SleepMode::Never && in.radarSleepAfterMs > 0) {
    nextS = in.idleMs >= in.radarSleepAfterMs ? 0 : (long)((in.radarSleepAfterMs - in.idleMs) / 1000);
  } else if (level == logic::SleepLevel::Radar && patrol) {
    nextS = (long)(patrolUntil - now) > 0 ? (long)((patrolUntil - now) / 1000) : 0;
  } else if (level == logic::SleepLevel::Radar && in.mode == logic::SleepMode::RadarThenDeep && in.deepAllowed) {
    if (in.night) {
      nextS = 0;
    } else if (in.deepSleepAfterMs > 0) {
      nextS = in.inRadarSleepMs >= in.deepSleepAfterMs ? 0 : (long)((in.deepSleepAfterMs - in.inRadarSleepMs) / 1000);
    }
  }
  String json = "{\"level\":\"";
  json += LevelName(level);
  json += "\",\"mode\":" + String((int)in.mode) + ",\"idleS\":" + String(in.idleMs / 1000) +
          ",\"nextS\":" + String(nextS) + ",\"deepAllowed\":" + (in.deepAllowed ? "true" : "false") +
          ",\"night\":" + (in.night ? "true" : "false") + ",\"patrol\":" + (patrol ? "true" : "false") + "}";
  return json;
}
