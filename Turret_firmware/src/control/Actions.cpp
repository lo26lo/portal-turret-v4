#include "Actions.h"

#include "board/I2cBus.h"
#include "board/Log.h"
#include "states/StateMachine.h"
#include <esp_system.h>

namespace {
const size_t COMMAND_MAX = 128; // not LINE_MAX: that one is a system macro
const UBaseType_t QUEUE_LENGTH = 24; // a whole settings group can be saved at once
const ulong RADAR_ALIVE_MS = 3000;
const ulong REBOOT_DELAY_MS = 500;   // lets the HTTP answer leave first
const ulong TRIM_TEST_MS = 2000;     // plan §10.4: trim slider stops after 2 s

struct QueuedLine {
  char text[COMMAND_MAX];
};

// Aliases for "servo <n|name>", in Gantry channel order.
const char *SERVO_NAMES[Gantry::CHANNEL_COUNT] = {"rotz", "rotx", "gunl", "gunr", "wingl", "wingr"};

String FirstWord(const String &text, String &rest) {
  String trimmed = text;
  trimmed.trim();
  int space = trimmed.indexOf(' ');
  if (space < 0) {
    rest = "";
    return trimmed;
  }
  rest = trimmed.substring(space + 1);
  rest.trim();
  return trimmed.substring(0, space);
}

void AppendJsonString(String &out, const char *text) {
  out += '"';
  for (const char *c = text; *c; c++) {
    if (*c == '"' || *c == '\\') {
      out += '\\';
    }
    if ((uint8_t)*c >= 0x20) {
      out += *c;
    }
  }
  out += '"';
}

const char *Bool(bool value) { return value ? "true" : "false"; }

int ServoIndex(const String &name) {
  if (name.length() == 1 && isDigit(name[0])) {
    int index = name.toInt();
    return index < Gantry::CHANNEL_COUNT ? index : -1;
  }
  for (int i = 0; i < Gantry::CHANNEL_COUNT; i++) {
    if (name.equalsIgnoreCase(SERVO_NAMES[i])) {
      return i;
    }
  }
  return -1;
}

const char *PositionName(WingPosition position) {
  switch (position) {
  case WingPosition::Open: return "open";
  case WingPosition::Closed: return "closed";
  default: return "between";
  }
}
} // namespace

void Actions::Initialize(Turret &turretIn, StateMachine &stateMachineIn, AccessPoint &accessPointIn, Station &stationIn) {
  turret = &turretIn;
  stateMachine = &stateMachineIn;
  accessPoint = &accessPointIn;
  station = &stationIn;
  queue = xQueueCreate(QUEUE_LENGTH, sizeof(QueuedLine));
}

// ------------------------------------------------------------------ loop side

void Actions::Update() {
  ReadConsole();

  QueuedLine item;
  while (queue != nullptr && xQueueReceive(queue, &item, 0) == pdTRUE) {
    Log.printf("web> %s\n", item.text);
    Log.println(Execute(String(item.text)));
  }

  if (otaRequested && !otaReady) {
    Log.println("OTA: stopping everything before accepting the data");
    stateMachine->GoToState(StateId::Manual);
    ShutdownForRestart();
    otaReady = true;
  }

  if (rebootPending && (long)(millis() - rebootAt) >= 0) {
    Log.println("Rebooting");
    ShutdownForRestart();
    Serial.flush();
    ESP.restart();
  }
}

void Actions::ReadConsole() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      consoleLine[consoleLength] = '\0';
      if (consoleLength > 0) {
        Log.printf("> %s\n", consoleLine);
        Log.println(Execute(String(consoleLine)));
      }
      consoleLength = 0;
    } else if (consoleLength < sizeof(consoleLine) - 1) {
      consoleLine[consoleLength++] = c;
    }
  }
}

bool Actions::Enqueue(const String &line) {
  if (queue == nullptr) {
    return false;
  }
  QueuedLine item;
  strlcpy(item.text, line.c_str(), sizeof(item.text));
  return xQueueSend(queue, &item, 0) == pdTRUE;
}

bool Actions::RequestOtaShutdown(uint32_t timeoutMs) {
  otaRequested = true;
  uint32_t waited = 0;
  while (!otaReady && waited < timeoutMs) {
    vTaskDelay(pdMS_TO_TICKS(10));
    waited += 10;
  }
  return otaReady;
}

void Actions::ShutdownForRestart() {
  turret->gantry.DetachAll();
  turret->audio.End();
  turret->light.SetEnabled(false);
}

void Actions::ApplyAllSettings() {
  turret->gantry.ApplySettings();
  turret->audio.ApplySettings();
  turret->light.ApplySettings(turret->settings, turret->board.IsReducedMode());
}

bool Actions::EnterTestMode(String &error) {
  StateId state = stateMachine->GetCurrentStateId();
  if (state == StateId::Fault) {
    error = "refused: power fault";
    return false;
  }
  if (state == StateId::Booting) {
    error = "refused: still booting";
    return false;
  }
  if (state != StateId::Manual) {
    stateMachine->GoToState(StateId::Manual);
  }
  return true;
}

// ------------------------------------------------------------------ commands

String Actions::Execute(const String &line) {
  String args;
  String command = FirstWord(line, args);
  command.toLowerCase();
  String error;

  if (command == "" ) {
    return "";
  }
  if (command == "help" || command == "?") {
    return Help();
  }
  if (command == "status") {
    return StatusJson();
  }
  if (command == "scan") {
    return String(I2cBus::Scan()) + " device(s)";
  }
  if (command == "imu") {
    return ImuReport();
  }
  if (command == "hall") {
    return HallReport();
  }
  if (command == "flt") {
    return String("PWR_FLT ") + (turret->board.IsPowerFault() ? "LOW (fault)" : "high (ok)") +
           ", faults since boot: " + String(turret->board.GetPowerFaultCount());
  }
  if (command == "reset-reason") {
    return String(Board::ResetReasonName(turret->board.GetResetReason())) +
           ", suspicious boots in a row: " + String(turret->board.GetBootLoopCount()) +
           ", brownouts: " + String(turret->board.GetBrownoutCount());
  }
  if (command == "get") {
    return GetCommand(args);
  }
  if (command == "set") {
    return SetCommand(args);
  }
  if (command == "reset-settings") {
    if (args.length() > 0) {
      turret->settings.ResetGroup(args.c_str());
    } else {
      turret->settings.ResetToDefaults();
    }
    ApplyAllSettings();
    return "settings reset" + (args.length() ? " (" + args + ")" : String("")) +
           "; WiFi settings apply at the next boot";
  }
  if (command == "mute") {
    bool muted = args == "off" ? false : (args == "on" ? true : !turret->audio.amp.IsMuted());
    turret->audio.amp.SetMuted(muted);
    return muted ? "muted" : "unmuted";
  }
  if (command == "gain") {
    // RAM only; use "set AmpGain <dB>" to keep it.
    turret->audio.amp.SetGain(args.toInt());
    return "gain " + String(turret->audio.amp.GetGain()) + " dB (not saved)";
  }
  if (command == "tone") {
    String msText;
    uint32_t hz = FirstWord(args, msText).toInt();
    uint32_t ms = msText.length() ? msText.toInt() : 500;
    if (hz < 20 || hz > 20000 || ms == 0 || ms > 10000) {
      return "usage: tone <20..20000 Hz> [ms <= 10000]";
    }
    turret->audio.PlayTone(hz, ms);
    return "tone " + String(hz) + " Hz, " + String(ms) + " ms";
  }
  if (command == "wifi") {
    if (args == "on") {
      accessPoint->Start(turret->settings);
      station->Start(turret->settings);
      return "WiFi on";
    }
    if (args == "off") {
      station->Stop();
      accessPoint->Stop();
      return "WiFi off";
    }
    if (args == "scan") {
      station->StartScan();
      return "scanning WiFi networks";
    }
    // First setup: "set StaSsid ...", "set StaPassword ...", then "wifi join" applies them now.
    if (args == "join") {
      station->Restart(turret->settings);
      return station->IsEnabled() ? String("joining \"") + station->GetSsid() + "\""
                                  : String("home network disabled (StaSsid empty)");
    }
    String home = !station->IsEnabled() ? String("not configured")
                  : station->IsConnected() ? String(station->GetSsid()) + ", " + station->GetIp() + ", " +
                                                 String(station->GetRssi()) + " dBm"
                                           : String(station->GetSsid()) + ", not connected";
    return String("access point ") + (accessPoint->IsOn() ? "on" : "off") + ", clients: " +
           String(accessPoint->GetClientCount()) + "; home network: " + home;
  }
  if (command == "time") {
    return Station::HasTime() ? Station::LocalTime() : String("time not set (no NTP yet)");
  }
  if (command == "reboot") {
    rebootPending = true;
    rebootAt = millis() + REBOOT_DELAY_MS;
    return "rebooting";
  }
  if (command == "resume") {
    StateId state = stateMachine->GetCurrentStateId();
    if (state == StateId::Fault) {
      return "refused: power fault";
    }
    turret->light.ClearTest();
    turret->audio.StopTone();
    // Booting re-homes the mechanics; bench / reduced modes end in Manual again.
    stateMachine->GoToState(StateId::Booting);
    return "resuming (homing, then Idle)";
  }
  if (command == "demo") {
    if (stateMachine->GetCurrentStateId() != StateId::Idle) {
      return "refused: only from Idle (use resume first)";
    }
    if (!turret->motion.CanDeploy()) {
      return "refused: turret not upright";
    }
    stateMachine->GoToState(StateId::Activate);
    return "demo cycle";
  }

  // Hardware tests: state machine stopped first.
  if (command == "servo") {
    if (!EnterTestMode(error)) {
      return error;
    }
    return ServoCommand(args);
  }
  if (command == "led") {
    if (!EnterTestMode(error)) {
      return error;
    }
    return LedCommand(args);
  }
  if (command == "wings") {
    if (!EnterTestMode(error)) {
      return error;
    }
    Gantry &gantry = turret->gantry;
    if (args == "open") {
      if (!turret->motion.CanDeploy()) {
        return "refused: turret not upright";
      }
      gantry.OpenWings();
      return "opening wings";
    }
    if (args == "close") {
      gantry.GetWingLeft().Home();
      gantry.GetWingRight().Home();
      return "closing wings";
    }
    return "usage: wings open|close";
  }
  if (command == "guns") {
    if (!EnterTestMode(error)) {
      return error;
    }
    Gantry &gantry = turret->gantry;
    if (args == "extend") {
      gantry.GetWingLeft().GetGun().Extend();
      gantry.GetWingRight().GetGun().Extend();
      return "extending guns";
    }
    if (args == "retract") {
      gantry.GetWingLeft().GetGun().Retract();
      gantry.GetWingRight().GetGun().Retract();
      return "retracting guns";
    }
    return "usage: guns extend|retract";
  }
  if (command == "trim") {
    if (!EnterTestMode(error)) {
      return error;
    }
    String valueText;
    String side = FirstWord(args, valueText);
    if ((side != "left" && side != "right") || valueText.length() == 0) {
      return "usage: trim left|right <-200..200 us>";
    }
    Wing &wing = side == "left" ? turret->gantry.GetWingLeft() : turret->gantry.GetWingRight();
    wing.TestStop(valueText.toInt(), TRIM_TEST_MS);
    return "wing " + side + " at 1500 " + (valueText.toInt() >= 0 ? "+ " : "- ") +
           String(abs(valueText.toInt())) + " us for 2 s (not saved: set WingTrimL/R)";
  }

  return "unknown command \"" + command + "\" (help)";
}

String Actions::Help() {
  return "Commands:\n"
         "  status | scan | imu | hall | flt | reset-reason\n"
         "  get [key] | set <key> <value> | reset-settings [group]\n"
         "  servo <0-5|rotz|rotx|gunl|gunr|wingl|wingr> <angle 0-180|us 500-2400|off>\n"
         "  wings open|close | guns extend|retract | trim left|right <us>\n"
         "  led ring|left|right|all <RRGGBB|off> | tone <Hz> [ms] | gain 9|12|15 | mute [on|off]\n"
         "  wifi [on|off|scan|join] | time | demo | resume | reboot\n"
         "Tests stop the state machine (Manual); \"resume\" homes and goes back to Idle.";
}

String Actions::ServoCommand(const String &args) {
  String valueText;
  String name = FirstWord(args, valueText);
  int index = ServoIndex(name);
  if (index < 0 || valueText.length() == 0) {
    return "usage: servo <0-5|rotz|rotx|gunl|gunr|wingl|wingr> <angle|us|off>";
  }
  Gantry &gantry = turret->gantry;
  ServoChannel &channel = gantry.GetChannel(index);
  if (valueText == "off") {
    channel.Release();
    return String(channel.GetName()) + " released";
  }
  // Bench mode (D1): a single servo attached at a time.
  if (turret->board.IsBenchMode()) {
    for (uint8_t i = 0; i < Gantry::CHANNEL_COUNT; i++) {
      if (i != index) {
        gantry.GetChannel(i).Release();
      }
    }
  }
  int value = valueText.toInt();
  if (value >= 0 && value <= 180) {
    channel.SetAngle(value);
  } else if (value >= 500 && value <= 2400) {
    channel.SetMicroseconds(value);
  } else {
    return "value: angle 0..180 or pulse 500..2400 us";
  }
  return String(channel.GetName()) + " -> " + String(channel.GetMicroseconds()) + " us" +
         (channel.IsAttached() ? "" : " (attaching)");
}

String Actions::LedCommand(const String &args) {
  String colorText;
  String strip = FirstWord(args, colorText);
  uint8_t mask = strip == "ring" ? 1 : strip == "left" ? 2 : strip == "right" ? 4 : strip == "all" ? 7 : 0;
  if (strip == "off" || colorText == "off") {
    turret->light.ClearTest();
    return "LED test off";
  }
  if (mask == 0 || colorText.length() != 6) {
    return "usage: led ring|left|right|all <RRGGBB|off>";
  }
  uint32_t rgb = strtoul(colorText.c_str(), nullptr, 16);
  turret->light.SetTestColor(mask, CRGB(rgb));
  return "LED " + strip + " = #" + colorText;
}

String Actions::GetCommand(const String &args) {
  Settings &settings = turret->settings;
  String out;
  for (int i = 0; i < SettingId::COUNT; i++) {
    const SettingsEntry &entry = settings.entries[i];
    if (args.length() && !args.equalsIgnoreCase(entry.key)) {
      continue;
    }
    out += String(entry.key) + " = ";
    switch (entry.type) {
    case SettingType::Int: out += String(entry.value.valueInt); break;
    case SettingType::Float: out += String(entry.value.valueFloat, 3); break;
    case SettingType::Bool: out += Bool(entry.value.valueBool); break;
    case SettingType::Str:
      out += (i == SettingId::ApPassword || i == SettingId::StaPassword) ? "********" : entry.value.valueString;
      break;
    }
    out += "\n";
  }
  return out.length() ? out : "unknown key \"" + args + "\"";
}

String Actions::SetCommand(const String &args) {
  String value;
  String key = FirstWord(args, value);
  SettingId id;
  if (key.length() == 0 || !turret->settings.FindId(key.c_str(), id)) {
    return "usage: set <key> <value> (get lists the keys)";
  }
  if (!turret->settings.SetFromString(id, value.c_str())) {
    return "refused: " + key;
  }
  ApplyAllSettings();
  const SettingsEntry *entry = turret->settings.Get(id);
  bool atReboot = strcmp(entry->group, "WiFi") == 0;
  bool homeNetwork = id == SettingId::StaSsid || id == SettingId::StaPassword || id == SettingId::Timezone;
  const char *when = homeNetwork ? "(applied at the next boot, or now with: wifi join)" : "(applied at the next boot)";
  if (id == SettingId::ApPassword || id == SettingId::StaPassword) {
    return key + " saved " + when;
  }
  return GetCommand(key) + (atReboot ? when : "");
}

String Actions::ImuReport() {
  Motion &motion = turret->motion;
  if (!motion.IsAvailable()) {
    return "IMU not available";
  }
  const sensors_vec_t &g = motion.GetAcceleration();
  const sensors_vec_t &r = motion.GetGyro();
  return "gravity " + String(g.x, 2) + " " + String(g.y, 2) + " " + String(g.z, 2) +
         " m/s2, dominant axis " + String(motion.DominantAxis()) + ", ImuUpAxis " +
         String(turret->settings.GetInt(SettingId::ImuUpAxis)) + ", upright " + Bool(motion.IsUpright()) +
         "\ngyro " + String(r.x, 3) + " " + String(r.y, 3) + " " + String(r.z, 3) + " rad/s, " +
         String(motion.GetTemperature(), 1) + " C";
}

String Actions::HallReport() {
  Wing &left = turret->gantry.GetWingLeft();
  Wing &right = turret->gantry.GetWingRight();
  return "left " + String(left.ReadHall()) + " (" + PositionName(left.ReadPosition()) + "), right " +
         String(right.ReadHall()) + " (" + PositionName(right.ReadPosition()) + ")" +
         (turret->gantry.HasHallFault() ? ", FAULT" : "");
}

// ------------------------------------------------------------------ status

String Actions::StatusJson() {
  Board &board = turret->board;
  Motion &motion = turret->motion;
  Gantry &gantry = turret->gantry;
  Audio &audio = turret->audio;

  String json;
  json.reserve(1024);
  json += "{\"version\":";
  AppendJsonString(json, __DATE__ " " __TIME__);
  json += ",\"uptimeMs\":" + String(millis());
  json += ",\"state\":";
  AppendJsonString(json, StateMachine::StateName(stateMachine->GetCurrentStateId()));
  json += ",\"resetReason\":";
  AppendJsonString(json, Board::ResetReasonName(board.GetResetReason()));
  json += ",\"bootLoopCount\":" + String(board.GetBootLoopCount());
  json += ",\"brownouts\":" + String(board.GetBrownoutCount());
  json += ",\"powerFaults\":" + String(board.GetPowerFaultCount());
  json += String(",\"pwrFlt\":") + Bool(board.IsPowerFault());
  json += String(",\"benchMode\":") + Bool(board.IsBenchMode());
  json += String(",\"reducedMode\":") + Bool(board.IsReducedMode());
  json += ",\"faults\":[";
  bool first = true;
  for (uint8_t n = 1; n <= 6; n++) {
    if (board.GetFaults() & (1u << (n - 1))) {
      json += first ? "" : ",";
      json += String(n);
      first = false;
    }
  }
  json += "]";
  json += String(",\"radar\":{\"alive\":") + Bool(turret->radar.IsAlive(RADAR_ALIVE_MS)) +
          ",\"targets\":" + String(turret->radar.GetTargetCount()) + "}";
  const sensors_vec_t &g = motion.GetAcceleration();
  json += String(",\"imu\":{\"available\":") + Bool(motion.IsAvailable()) + ",\"gravity\":[" +
          String(g.x, 2) + "," + String(g.y, 2) + "," + String(g.z, 2) + "]" +
          ",\"dominantAxis\":" + String(motion.DominantAxis()) + ",\"upright\":" + Bool(motion.IsUpright()) +
          ",\"temperature\":" + String(motion.GetTemperature(), 1) + "}";
  json += ",\"hall\":{\"left\":" + String(gantry.GetWingLeft().GetLastHall()) +
          ",\"right\":" + String(gantry.GetWingRight().GetLastHall()) +
          ",\"fault\":" + Bool(gantry.HasHallFault()) + "}";
  json += String(",\"wings\":{\"leftOpen\":") + Bool(gantry.GetWingLeft().IsOpen()) +
          ",\"rightOpen\":" + Bool(gantry.GetWingRight().IsOpen()) +
          ",\"moving\":" + Bool(gantry.GetWingLeft().IsMoving() || gantry.GetWingRight().IsMoving()) + "}";
  json += ",\"servos\":{";
  for (uint8_t i = 0; i < Gantry::CHANNEL_COUNT; i++) {
    ServoChannel &channel = gantry.GetChannel(i);
    json += i ? "," : "";
    AppendJsonString(json, SERVO_NAMES[i]);
    json += ":" + (channel.IsAttached() ? String(channel.GetMicroseconds()) : String("null"));
  }
  json += "}";
  json += ",\"amp\":{\"gainDb\":" + String(audio.amp.GetGain()) + ",\"running\":" + Bool(audio.amp.IsRunning()) +
          ",\"muted\":" + Bool(audio.amp.IsMuted()) + "}";
  json += String(",\"wifi\":{\"on\":") + Bool(accessPoint->IsOn()) + ",\"ssid\":";
  AppendJsonString(json, accessPoint->GetSsid());
  json += ",\"clients\":" + String(accessPoint->GetClientCount()) +
          ",\"defaultPassword\":" + Bool(accessPoint->HasDefaultPassword());
  json += String(",\"sta\":{\"enabled\":") + Bool(station->IsEnabled()) +
          ",\"connected\":" + Bool(station->IsConnected()) + ",\"ssid\":";
  AppendJsonString(json, station->GetSsid());
  json += ",\"ip\":";
  AppendJsonString(json, station->GetIp());
  json += ",\"rssi\":" + String(station->GetRssi()) + "}}";
  json += ",\"time\":";
  AppendJsonString(json, Station::LocalTime().c_str());
  json += ",\"freeHeap\":" + String(ESP.getFreeHeap());
  json += "}";
  return json;
}
