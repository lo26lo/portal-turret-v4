#include "board/Log.h"
#include "Settings.h"

namespace {
const char *NVS_NAMESPACE = "turret";
const size_t NVS_KEY_MAX = 15;
} // namespace

SettingValue::SettingValue(const char *v) { strlcpy(valueString, v, SETTING_STRING_MAX); }

SettingsEntry::SettingsEntry(const char *key, const char *label, const char *group, SettingType type, SettingValue defaultValue, SettingValue min, SettingValue max) : key(key), label(label), group(group), type(type), value(defaultValue), defaultValue(defaultValue), min(min), max(max) {}

SettingsEntry::SettingsEntry(const char *key, const char *label, const char *group, SettingType type, SettingValue defaultValue) : key(key), label(label), group(group), type(type), value(defaultValue), defaultValue(defaultValue), min((int32_t)0), max((int32_t)0) {}

// The rows must stay in the same order as SettingId.
Settings::Settings()
    : entries{
          {"AngleOffsetX", "Angle offset X", "Motion", SettingType::Int, (int32_t)0, (int32_t)-90, (int32_t)90},
          {"AngleOffsetZ", "Angle offset Z", "Motion", SettingType::Int, (int32_t)0, (int32_t)-90, (int32_t)90},
          // 0 = not calibrated (no upright check), 1..6 = +X, -X, +Y, -Y, +Z, -Z
          {"ImuUpAxis", "IMU up axis (0 = off, 1..6 = +X -X +Y -Y +Z -Z)", "Motion", SettingType::Int, (int32_t)0, (int32_t)0, (int32_t)6},
          // MAX98357A gain: 9, 12 or 15 dB (other values rounded to the nearest)
          {"AmpGain", "Amplifier gain (dB: 9, 12, 15)", "Audio", SettingType::Int, (int32_t)9, (int32_t)9, (int32_t)15},
          {"Volume", "Volume (%)", "Audio", SettingType::Int, (int32_t)80, (int32_t)0, (int32_t)100},
          // Hall thresholds (raw ADC 0..4095). V4 values; Turret2 sensors run at
          // 3.3 V and must be calibrated. Open below closed = reversed magnet.
          {"HallOpenL", "Hall left: open threshold", "Wings", SettingType::Int, (int32_t)2500, (int32_t)0, (int32_t)4095},
          {"HallOpenR", "Hall right: open threshold", "Wings", SettingType::Int, (int32_t)2500, (int32_t)0, (int32_t)4095},
          {"HallCloseL", "Hall left: closed threshold", "Wings", SettingType::Int, (int32_t)1500, (int32_t)0, (int32_t)4095},
          {"HallCloseR", "Hall right: closed threshold", "Wings", SettingType::Int, (int32_t)1500, (int32_t)0, (int32_t)4095},
          // Continuous servo neutral: 1500 us + trim
          {"WingTrimL", "Wing left: stop trim (us)", "Wings", SettingType::Int, (int32_t)0, (int32_t)-200, (int32_t)200},
          {"WingTrimR", "Wing right: stop trim (us)", "Wings", SettingType::Int, (int32_t)0, (int32_t)-200, (int32_t)200},
          {"ServoStagger", "Delay between two servo starts (ms)", "Power", SettingType::Int, (int32_t)250, (int32_t)50, (int32_t)1000},
          {"ServoIdleMs", "Rotation servos released after (ms, 0 = never)", "Power", SettingType::Int, (int32_t)5000, (int32_t)0, (int32_t)60000},
          {"LedBright", "LED brightness (0..255)", "Lights", SettingType::Int, (int32_t)255, (int32_t)0, (int32_t)255},
          // FastLED power cap on the 5 V rail: 13 LEDs at full white draw ~0.8 A
          {"LedMaxmA", "LED current limit (mA)", "Power", SettingType::Int, (int32_t)500, (int32_t)100, (int32_t)1500},
          // D7. Group "WiFi" = applied at the next boot. Passwords: 8..63 chars (WPA2).
          {"ApSsid", "Access point name", "WiFi", SettingType::Str, "Portal Turret"},
          {"ApPassword", "Access point and web password (8..63)", "WiFi", SettingType::Str, "stillalive"},
          // Home network (station mode): empty name = not used. The access point stays on.
          {"StaSsid", "Home WiFi name (empty = off)", "WiFi", SettingType::Str, ""},
          {"StaPassword", "Home WiFi password", "WiFi", SettingType::Str, ""},
          // POSIX TZ string, default Europe/Paris with daylight saving time
          {"Timezone", "Time zone (POSIX TZ)", "WiFi", SettingType::Str, "CET-1CEST,M3.5.0,M10.5.0/3"},
      },
      prefsReady(false) {}

void Settings::Initialize() {
  for (int i = 0; i < SettingId::COUNT; i++) {
    if (strlen(entries[i].key) > NVS_KEY_MAX) {
      Log.print("Settings: key too long for NVS: ");
      Log.println(entries[i].key);
    }
  }

  prefsReady = prefs.begin(NVS_NAMESPACE, false);
  if (!prefsReady) {
    Log.println("Settings: could not open NVS, using defaults");
    return;
  }
  Load();
}

void Settings::Load() {
  for (int i = 0; i < SettingId::COUNT; i++) {
    SettingsEntry &entry = entries[i];
    if (!prefs.isKey(entry.key)) {
      continue;
    }
    switch (entry.type) {
    case SettingType::Int:
      entry.value.valueInt = prefs.getInt(entry.key, entry.value.valueInt);
      break;
    case SettingType::Float:
      entry.value.valueFloat =
          prefs.getFloat(entry.key, entry.value.valueFloat);
      break;
    case SettingType::Bool:
      entry.value.valueBool = prefs.getBool(entry.key, entry.value.valueBool);
      break;
    case SettingType::Str:
      prefs.getString(entry.key, entry.value.valueString, SETTING_STRING_MAX);
      break;
    }
  }
}

void Settings::Persist(const SettingsEntry &entry) {
  if (!prefsReady) {
    return;
  }
  switch (entry.type) {
  case SettingType::Int:
    prefs.putInt(entry.key, entry.value.valueInt);
    break;
  case SettingType::Float:
    prefs.putFloat(entry.key, entry.value.valueFloat);
    break;
  case SettingType::Bool:
    prefs.putBool(entry.key, entry.value.valueBool);
    break;
  case SettingType::Str:
    prefs.putString(entry.key, entry.value.valueString);
    break;
  }
}

const SettingsEntry *Settings::Get(SettingId id) const {
  if (id < 0 || id >= SettingId::COUNT) {
    Log.print("Settings: unknown id ");
    Log.println((int)id);
    return nullptr;
  }
  return &entries[id];
}

SettingsEntry *Settings::Get(SettingId id) {
  return const_cast<SettingsEntry *>(
      static_cast<const Settings *>(this)->Get(id));
}

int32_t Settings::GetInt(SettingId id) const {
  const SettingsEntry *entry = Get(id);
  if (entry == nullptr) {
    return 0;
  }
  if (entry->type != SettingType::Int) {
    Log.print("Settings: not an int: ");
    Log.println(entry->key);
    return 0;
  }
  return entry->value.valueInt;
}

float Settings::GetFloat(SettingId id) const {
  const SettingsEntry *entry = Get(id);
  if (entry == nullptr) {
    return 0.0f;
  }
  // Ints read fine as floats; anything else does not.
  if (entry->type == SettingType::Int) {
    return (float)entry->value.valueInt;
  }
  if (entry->type != SettingType::Float) {
    Log.print("Settings: not a float: ");
    Log.println(entry->key);
    return 0.0f;
  }
  return entry->value.valueFloat;
}

bool Settings::GetBool(SettingId id) const {
  const SettingsEntry *entry = Get(id);
  if (entry == nullptr) {
    return false;
  }
  if (entry->type != SettingType::Bool) {
    Log.print("Settings: not a bool: ");
    Log.println(entry->key);
    return false;
  }
  return entry->value.valueBool;
}

const char *Settings::GetString(SettingId id) const {
  const SettingsEntry *entry = Get(id);
  if (entry == nullptr) {
    return "";
  }
  if (entry->type != SettingType::Str) {
    Log.print("Settings: not a string: ");
    Log.println(entry->key);
    return "";
  }
  return entry->value.valueString;
}

bool Settings::Set(SettingId id, int32_t value) {
  SettingsEntry *entry = Get(id);
  if (entry == nullptr) {
    return false;
  }
  if (entry->type != SettingType::Int) {
    Log.print("Settings: not an int: ");
    Log.println(entry->key);
    return false;
  }
  if (value < entry->min.valueInt) {
    value = entry->min.valueInt;
  }
  if (value > entry->max.valueInt) {
    value = entry->max.valueInt;
  }
  entry->value.valueInt = value;
  Persist(*entry);
  return true;
}

bool Settings::Set(SettingId id, float value) {
  SettingsEntry *entry = Get(id);
  if (entry == nullptr) {
    return false;
  }
  if (entry->type != SettingType::Float) {
    Log.print("Settings: not a float: ");
    Log.println(entry->key);
    return false;
  }
  if (value < entry->min.valueFloat) {
    value = entry->min.valueFloat;
  }
  if (value > entry->max.valueFloat) {
    value = entry->max.valueFloat;
  }
  entry->value.valueFloat = value;
  Persist(*entry);
  return true;
}

bool Settings::Set(SettingId id, bool value) {
  SettingsEntry *entry = Get(id);
  if (entry == nullptr) {
    return false;
  }
  if (entry->type != SettingType::Bool) {
    Log.print("Settings: not a bool: ");
    Log.println(entry->key);
    return false;
  }
  entry->value.valueBool = value;
  Persist(*entry);
  return true;
}

bool Settings::Set(SettingId id, const char *value) {
  SettingsEntry *entry = Get(id);
  if (entry == nullptr) {
    return false;
  }
  if (entry->type != SettingType::Str) {
    Log.print("Settings: not a string: ");
    Log.println(entry->key);
    return false;
  }
  // WPA2: passphrase 8..63 characters (SETTING_STRING_MAX leaves 63); SSID 1..32 bytes.
  size_t length = strlen(value);
  if (id == SettingId::ApPassword && (length < 8 || length >= SETTING_STRING_MAX)) {
    Log.println("Settings: ApPassword must be 8 to 63 characters");
    return false;
  }
  if (id == SettingId::StaPassword && length != 0 && (length < 8 || length >= SETTING_STRING_MAX)) {
    Log.println("Settings: StaPassword must be empty (open network) or 8 to 63 characters");
    return false;
  }
  if ((id == SettingId::ApSsid && length == 0) ||
      ((id == SettingId::ApSsid || id == SettingId::StaSsid) && length > 32)) {
    Log.println("Settings: a WiFi name is 1 to 32 characters");
    return false;
  }
  if (id == SettingId::Timezone && length == 0) {
    Log.println("Settings: Timezone cannot be empty");
    return false;
  }
  strlcpy(entry->value.valueString, value, SETTING_STRING_MAX);
  Persist(*entry);
  return true;
}

namespace {
bool ParseBool(const char *text) {
  return strcasecmp(text, "true") == 0 || strcasecmp(text, "on") == 0 ||
         strcmp(text, "1") == 0;
}
} // namespace

bool Settings::SetFromString(SettingId id, const char *text) {
  const SettingsEntry *entry = Get(id);
  if (entry == nullptr) {
    return false;
  }
  switch (entry->type) {
  case SettingType::Int:
    return Set(id, (int32_t)strtol(text, nullptr, 10));
  case SettingType::Float:
    return Set(id, (float)strtod(text, nullptr));
  case SettingType::Bool:
    return Set(id, ParseBool(text));
  case SettingType::Str:
    return Set(id, text);
  }
  return false;
}

bool Settings::FindId(const char *key, SettingId &outId) const {
  for (int i = 0; i < SettingId::COUNT; i++) {
    if (strcmp(entries[i].key, key) == 0) {
      outId = (SettingId)i;
      return true;
    }
  }
  Log.print("Settings: unknown key ");
  Log.println(key);
  return false;
}

void Settings::ResetGroup(const char *group) {
  for (int i = 0; i < SettingId::COUNT; i++) {
    if (strcmp(entries[i].group, group) == 0) {
      entries[i].value = entries[i].defaultValue;
      Persist(entries[i]);
    }
  }
}

void Settings::ResetToDefaults() {
  for (int i = 0; i < SettingId::COUNT; i++) {
    entries[i].value = entries[i].defaultValue;
    Persist(entries[i]);
  }
}