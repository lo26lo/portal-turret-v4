#pragma once

#include <Arduino.h>
#include <DNSServer.h>

#include "settings/Settings.h"

// WiFi access point (D7): WPA2 with ApPassword, name ApSsid. Read at start;
// a change is applied at the next boot (or the next Start()).
// Captive portal: every DNS name resolves to the turret, so a phone joining
// the access point opens the page by itself (first WiFi setup).
class AccessPoint {
public:
  void Start(Settings &settings);
  void Stop();
  // loop(): answers the DNS requests of the access point clients.
  void Update();
  bool IsOn() const { return on; }
  const char *GetSsid() const { return ssid; }
  uint8_t GetClientCount() const;
  // The password is still the factory one: the web page shows a warning.
  bool HasDefaultPassword() const { return defaultPassword; }

private:
  bool on = false;
  bool defaultPassword = true;
  DNSServer dns;
  char ssid[SETTING_STRING_MAX] = "";
};
