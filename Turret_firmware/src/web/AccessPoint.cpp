#include "AccessPoint.h"

#include "board/Log.h"
#include <WiFi.h>

namespace {
const char *DEFAULT_PASSWORD = "stillalive";
} // namespace

void AccessPoint::Start(Settings &settings) {
  strlcpy(ssid, settings.GetString(SettingId::ApSsid), sizeof(ssid));
  const char *password = settings.GetString(SettingId::ApPassword);
  // Settings::Set already refuses a short password; this covers a bad NVS value.
  if (strlen(password) < 8) {
    Log.println("WiFi: stored password too short, using the default one");
    password = DEFAULT_PASSWORD;
  }
  defaultPassword = strcmp(password, DEFAULT_PASSWORD) == 0;
  on = WiFi.softAP(ssid, password);
  Log.printf("WiFi: access point \"%s\" %s (WPA2)%s\n", ssid, on ? "on" : "FAILED",
             defaultPassword ? ", default password" : "");
  if (on) {
    dns.start(53, "*", WiFi.softAPIP());
  }
}

void AccessPoint::Update() {
  if (on) {
    dns.processNextRequest();
  }
}

void AccessPoint::Stop() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  on = false;
  Log.println("WiFi: access point off");
}

uint8_t AccessPoint::GetClientCount() const { return on ? WiFi.softAPgetStationNum() : 0; }
