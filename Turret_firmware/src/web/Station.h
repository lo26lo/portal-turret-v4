#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "settings/Settings.h"

// Home network connection (station mode), next to the access point, which
// stays on so that the turret can always be reached. Once connected:
// time from NTP (Timezone setting) and mDNS name portal-turret.local.
// Note: in AP + STA mode the radio has a single channel, so the access point
// follows the channel of the home network; a phone on the access point may
// drop for a moment when the turret joins the home network.
class Station {
public:
  // Does nothing if StaSsid is empty. Call after the access point is up.
  void Start(Settings &settings);
  void Stop();
  // First WiFi setup from the web page: applies StaSsid / StaPassword now.
  void Restart(Settings &settings);
  // Logs connection changes, starts mDNS on the first connection, collects scan results.
  void Update();

  // Asynchronous scan of the WiFi networks around (loop context). The access
  // point may pause for a moment while the radio hops channels.
  void StartScan();
  // [{"ssid":..,"rssi":..,"secure":..},...], strongest first; any task.
  String ScanJson();
  bool IsScanning() const { return scanning; }

  bool IsEnabled() const { return enabled; }
  bool IsConnected() const { return connected; }
  const char *GetSsid() const { return ssid; }
  // Cached for the web server task.
  const char *GetIp() const { return ip; }
  int8_t GetRssi() const { return rssi; }
  // Time set by NTP at least once.
  static bool HasTime();
  // Local time "YYYY-MM-DD HH:MM:SS", empty if HasTime() is false.
  static String LocalTime();

  static const char *HOSTNAME;

private:
  bool enabled = false;
  bool connected = false;
  bool mdnsStarted = false;
  bool timeLogged = false;
  bool scanning = false;
  String scanJson = "[]";
  SemaphoreHandle_t scanLock = nullptr;
  ulong lastCheck = 0;
  int8_t rssi = 0;
  char ssid[SETTING_STRING_MAX] = "";
  char ip[16] = "";
};
