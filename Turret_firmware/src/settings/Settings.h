#pragma once

#include <Arduino.h>
#include <Preferences.h>

// Every setting gets an id here. COUNT sizes the table, so adding an id
// without adding its row in Settings.cpp is a compile error.
enum SettingId {
  AngleOffsetX,
  AngleOffsetZ,
  ImuUpAxis,
  AmpGain,
  Volume,
  Voices,
  CooldownMs,
  DetectMaxMm,
  DetectAngle,
  HallOpenL,
  HallOpenR,
  HallCloseL,
  HallCloseR,
  WingTrimL,
  WingTrimR,
  ServoStagger,
  ServoIdleMs,
  LedBright,
  LedMaxmA,
  GreenLedMode,
  GreenLedPeriod,
  GreenLedBright,
  RedLedMode,
  ApSsid,
  ApPassword,
  WebUser,
  WebPassword,
  StaSsid,
  StaPassword,
  Timezone,
  LabMarkers,
  Language,
  OledType,
  OledAnim,
  OledFlip,
  OledContrast,
  OledSleepS,
  OledFace,
  COUNT
};

// Scoped, so the enumerators don't collide with Arduino's String class.
enum class SettingType { Int,
                         Float,
                         Bool,
                         Str };

// 63 characters + terminator: the longest WPA2 passphrase.
constexpr size_t SETTING_STRING_MAX = 64;

// One storage slot shared by every type. Only the member matching the
// entry's SettingType is valid; that's what the type tag is for.
union SettingValue {
  int32_t valueInt;
  float valueFloat;
  bool valueBool;
  char valueString[SETTING_STRING_MAX];

  SettingValue(int32_t v) : valueInt(v) {}
  SettingValue(float v) : valueFloat(v) {}
  SettingValue(bool v) : valueBool(v) {}
  SettingValue(const char *v);
};

class SettingsEntry {
public:
  // Numeric settings: min/max bound the value.
  SettingsEntry(const char *key, const char *label, const char *group, SettingType type, SettingValue defaultValue, SettingValue min, SettingValue max);
  // Int setting that picks one of a list: values 0..n-1 name the options,
  // separated by '|' (min must be 0 and max n-1).
  SettingsEntry(const char *key, const char *label, const char *group, SettingType type, SettingValue defaultValue, SettingValue min, SettingValue max, const char *options);
  // Bool and string settings: no meaningful range.
  SettingsEntry(const char *key, const char *label, const char *group, SettingType type, SettingValue defaultValue);

  const char *key;   // NVS + JSON identity, max 15 chars, never rename
  const char *label; // human readable, for the frontend
  const char *group; // section of the settings page
  SettingType type;
  SettingValue value;
  SettingValue defaultValue;
  SettingValue min;
  SettingValue max;
  const char *options; // "A|B|C" for a choice (web page: a drop-down list), else nullptr
};

// Name of option `index` of an options string "A|B|C" into out; false if there is none.
bool OptionName(const char *options, int32_t index, char *out, size_t size);

class Settings {
public:
  Settings();
  
  SettingsEntry entries[SettingId::COUNT];

  // Opens NVS and overwrites values that have been stored before.
  void Initialize();

  int32_t GetInt(SettingId id) const;
  float GetFloat(SettingId id) const;
  bool GetBool(SettingId id) const;
  const char *GetString(SettingId id) const;
  
  const SettingsEntry *Get(SettingId id) const;
  SettingsEntry *Get(SettingId id);

  // Clamp to range, store, write to NVS. Returns false on a type mismatch.
  bool Set(SettingId id, int32_t value);
  bool Set(SettingId id, float value);
  bool Set(SettingId id, bool value);
  bool Set(SettingId id, const char *value);

  // For the web layer, which only ever has text.
  bool SetFromString(SettingId id, const char *text);
  bool FindId(const char *key, SettingId &outId) const;

  void ResetToDefaults();
  // Only the entries of one group (web page "reset" per group).
  void ResetGroup(const char *group);

private:
  void Persist(const SettingsEntry &entry);
  void Load();

  Preferences prefs;
  bool prefsReady;
};
