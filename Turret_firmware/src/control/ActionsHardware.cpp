// Hardware test commands: servos, wings, guns, LEDs, voice, calibration and
// the sensor reports. Dispatch and the command table: Actions.cpp.
#include "Actions.h"

#include "board/Log.h"
#include "board/Power.h"
#include "pins.h"
#include "states/StateMachine.h"

namespace {
const ulong TRIM_TEST_MS = 2000; // plan §10.4: trim slider stops after 2 s

const char *Bool(bool value) { return value ? "true" : "false"; }

const char *PositionName(WingPosition position) {
  switch (position) {
  case WingPosition::Open: return "open";
  case WingPosition::Closed: return "closed";
  default: return "between";
  }
}
} // namespace

// ------------------------------------------------------------------ reports

String Actions::CmdImu(const String &) {
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

String Actions::CmdHall(const String &) {
  Power::On(Rail::Hall); // v2: the sensors may be off between wing moves
  Wing &left = turret->gantry.GetWingLeft();
  Wing &right = turret->gantry.GetWingRight();
  return "left " + String(left.ReadHall()) + " (" + PositionName(left.ReadPosition()) + "), right " +
         String(right.ReadHall()) + " (" + PositionName(right.ReadPosition()) + ")" +
         (turret->gantry.HasHallFault() ? ", FAULT" : "");
}

String Actions::CmdInputs(const String &) {
  // Raw levels (bring-up step C, factory reset gesture): all active low.
  auto level = [](uint8_t pin) {
    return "(IO" + String(pin) + "): " + (digitalRead(pin) == LOW ? "pressed/closed" : "released/open");
  };
  String text = "PROG " + level(PIN_PROG) + ", SW1 " + level(PIN_BENCH_MODE);
  if (BOARD_HAS_BUTTONS) {
    text += ", button A " + level(PIN_BUTTON_A) + ", button B " + level(PIN_BUTTON_B);
  }
  if (PIN_BUTTON_EXT != PIN_ABSENT) { // active high
    text += ", remote button J20 (IO" + String(PIN_BUTTON_EXT) + "): " +
            (digitalRead(PIN_BUTTON_EXT) == HIGH ? "pressed" : "released");
  }
  return text;
}

// ------------------------------------------------------------------ servos

// Debug mode with a weak supply: a single servo attached at a time (D1).
void Actions::LimitToChannel(uint8_t index) {
  if (!turret->board.IsServoLimited()) {
    return;
  }
  for (uint8_t i = 0; i < Gantry::CHANNEL_COUNT; i++) {
    if (i != index) {
      turret->gantry.GetChannel(i).Release("one servo at a time");
    }
  }
}

String Actions::CmdServo(const String &args) {
  String valueText;
  String name = FirstWord(args, valueText);
  int index = Gantry::ChannelIndex(name);
  if (index < 0 || valueText.length() == 0) {
    return "usage: servo <0-5|rotz|rotx|gunl|gunr|wingl|wingr> <angle|us|off>";
  }
  ServoChannel &channel = turret->gantry.GetChannel(index);
  if (valueText == "off") {
    channel.Release("servo off command");
    return String(channel.GetName()) + " released";
  }
  LimitToChannel(index);
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

String Actions::CmdServos(const String &args) {
  if (args != "off") {
    return "usage: servos off";
  }
  CancelLabTests();
  turret->gantry.DetachAll();
  return "all servos released";
}

String Actions::WingOrGun(bool isWing, const String &args) {
  String what;
  String side = FirstWord(args, what);
  const char *usage = isWing ? "usage: wing left|right open|close" : "usage: gun left|right extend|retract";
  if (side != "left" && side != "right") {
    return usage;
  }
  Wing &wing = side == "left" ? turret->gantry.GetWingLeft() : turret->gantry.GetWingRight();
  // Gantry channels: 2 / 3 = guns, 4 / 5 = wings (left, right).
  uint8_t channel = (isWing ? 4 : 2) + (side == "left" ? 0 : 1);
  if (isWing && (what == "open" || what == "close")) {
    if (what == "open" && !turret->motion.CanDeploy()) {
      return "refused: turret not upright";
    }
    LimitToChannel(channel);
    if (what == "open") {
      wing.Open();
    } else {
      wing.Home();
    }
    return "wing " + side + (what == "open" ? ": opening" : ": closing");
  }
  if (!isWing && (what == "extend" || what == "retract")) {
    LimitToChannel(channel);
    if (what == "extend") {
      wing.GetGun().Extend();
    } else {
      wing.GetGun().Retract();
    }
    return "gun " + side + (what == "extend" ? ": out" : ": in");
  }
  return usage;
}

String Actions::CmdWing(const String &args) { return WingOrGun(true, args); }

String Actions::CmdGun(const String &args) { return WingOrGun(false, args); }

String Actions::CmdWings(const String &args) {
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

String Actions::CmdGuns(const String &args) {
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

String Actions::CmdTrim(const String &args) {
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

// ------------------------------------------------------------------ LEDs

String Actions::CmdLed(const String &args) {
  String colorText;
  String strip = FirstWord(args, colorText);
  if (strip == "pattern" && colorText == "bit") {
    turret->light.SetBitPattern(true);
    return "LED pattern: ring LED 1 = red 0x80, all else 0 (9th bit of 24 set, GRB order); \"led off\" to leave";
  }
  uint8_t mask = strip == "ring" ? 1 : strip == "left" ? 2 : strip == "right" ? 4 : strip == "all" ? 7 : 0;
  if (strip == "off" || colorText == "off") {
    turret->light.ClearTest();
    return "LED test off";
  }
  if (strip == "rainbow") {
    strip = "all";
    mask = 7;
    colorText = "rainbow";
  }
  if (mask != 0 && colorText == "rainbow") {
    turret->light.SetRainbow(mask);
    return "LED " + strip + " = moving rainbow; \"led off\" to leave";
  }
  if (mask == 0 || colorText.length() != 6) {
    return "usage: led ring|left|right|all <RRGGBB|rainbow|off>, led rainbow";
  }
  uint32_t rgb = strtoul(colorText.c_str(), nullptr, 16);
  turret->light.SetTestColor(mask, CRGB(rgb));
  return "LED " + strip + " = #" + colorText;
}

// ------------------------------------------------------------------ voice

// voice: the categories and their file counts; voice <name>: a random line of
// it; voice stop.
String Actions::CmdVoice(const String &args) {
  Audio &audio = turret->audio;
  bool enabled = turret->settings.GetBool(SettingId::Voices);
  if (args.length() == 0) {
    String out = String("voice lines ") + (enabled ? "on" : "off (set Voices true)") + ":";
    for (int i = 0; i < (int)Voice::Count; i++) {
      out += String(" ") + Audio::VoiceName((Voice)i) + " " + String(audio.VoiceFileCount((Voice)i));
    }
    if (audio.IsVoicePlaying()) {
      out += String("\nplaying ") + audio.CurrentVoiceFile();
    }
    return out + "\nusage: voice <name> | voice stop";
  }
  if (args == "stop") {
    audio.StopVoice();
    return "voice stopped";
  }
  Voice voice;
  if (!Audio::VoiceFromName(args.c_str(), voice)) {
    return "unknown voice \"" + args + "\" (voice lists them)";
  }
  if (audio.PlayVoice(voice)) {
    return String("voice ") + Audio::VoiceName(voice) + ": " + audio.CurrentVoiceFile();
  }
  if (!enabled) {
    return "refused: voice lines are off (set Voices true)";
  }
  if (audio.VoiceFileCount(voice) == 0) {
    return String("refused: no MP3 for ") + Audio::VoiceName(voice) + " on LittleFS (pio run -t uploadfs)";
  }
  if (audio.IsPlaying()) {
    return "refused: the gunshot or a tone is playing";
  }
  return "refused: audio not running or no memory for the decoder (log)";
}

// ------------------------------------------------------------------ calibration

// Calibration without the web page (debug menu, console):
//   cal hall left|right open|closed   captures the present Hall value (RAM)
//   cal hall save                      thresholds at 25 % / 75 % between the two, saved
//   cal imu                            the turret stands upright: saves the dominant axis
String Actions::CmdCal(const String &args) {
  String rest;
  String what = FirstWord(args, rest);
  if (what == "imu") {
    uint8_t axis = turret->motion.DominantAxis();
    if (!turret->motion.IsAvailable() || axis == 0) {
      return "refused: no IMU reading";
    }
    turret->settings.Set(SettingId::ImuUpAxis, (int32_t)axis);
    return "ImuUpAxis = " + String(axis);
  }
  if (what != "hall") {
    return "usage: cal hall left|right open|closed | cal hall save | cal imu";
  }
  String position;
  String side = FirstWord(rest, position);
  if (side == "save") {
    String done;
    for (uint8_t s = 0; s < 2; s++) {
      if (!hallCaptured[s][0] || !hallCaptured[s][1]) {
        continue;
      }
      int32_t open = hallCapture[s][0];
      int32_t closed = hallCapture[s][1];
      if (abs(open - closed) < 200) {
        return String(s == 0 ? "left" : "right") + ": open and closed too close";
      }
      turret->settings.Set(s == 0 ? SettingId::HallOpenL : SettingId::HallOpenR,
                           (int32_t)(closed + (open - closed) * 3 / 4));
      turret->settings.Set(s == 0 ? SettingId::HallCloseL : SettingId::HallCloseR,
                           (int32_t)(closed + (open - closed) / 4));
      done += s == 0 ? "L " : "R ";
    }
    if (done.length() == 0) {
      return "capture open and closed first";
    }
    ApplyAllSettings();
    return "Hall saved: " + done;
  }
  if ((side != "left" && side != "right") || (position != "open" && position != "closed")) {
    return "usage: cal hall left|right open|closed | cal hall save | cal imu";
  }
  uint8_t s = side == "left" ? 0 : 1;
  uint8_t p = position == "open" ? 0 : 1;
  Wing &wing = s == 0 ? turret->gantry.GetWingLeft() : turret->gantry.GetWingRight();
  Power::On(Rail::Hall); // v2: the sensors may be off between wing moves
  hallCapture[s][p] = wing.ReadHall();
  hallCaptured[s][p] = true;
  return side + " " + position + " = " + String(hallCapture[s][p]);
}
