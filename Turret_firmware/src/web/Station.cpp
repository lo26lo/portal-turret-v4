#include "Station.h"

#include "board/Log.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <time.h>

const char *Station::HOSTNAME = "portal-turret";

namespace {
const ulong CHECK_PERIOD_MS = 500;
const time_t VALID_TIME = 1700000000; // 2023: anything earlier is the 1970 default
} // namespace

void Station::Start(Settings &settings) {
  strlcpy(ssid, settings.GetString(SettingId::StaSsid), sizeof(ssid));
  enabled = ssid[0] != '\0';
  if (!enabled) {
    Log.println("WiFi: no home network configured (StaSsid empty)");
    return;
  }
  // The access point keeps running: AP + STA.
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  const char *password = settings.GetString(SettingId::StaPassword);
  WiFi.begin(ssid, password[0] ? password : nullptr);
  // SNTP keeps retrying in the background until the network is up.
  configTzTime(settings.GetString(SettingId::Timezone), "pool.ntp.org", "time.google.com");
  Log.printf("WiFi: joining \"%s\"\n", ssid);
}

void Station::Restart(Settings &settings) {
  Stop();
  enabled = false;
  timeLogged = false;
  Start(settings);
}

void Station::StartScan() {
  if (scanning) {
    return;
  }
  // Scanning needs the station interface; the access point keeps running.
  if (WiFi.getMode() == WIFI_AP) {
    WiFi.mode(WIFI_AP_STA);
  }
  scanning = WiFi.scanNetworks(true /* async */) == WIFI_SCAN_RUNNING;
  Log.println(scanning ? "WiFi: scanning" : "WiFi: scan could not start");
}

String Station::ScanJson() {
  if (scanLock == nullptr) {
    return "[]";
  }
  xSemaphoreTake(scanLock, portMAX_DELAY);
  String copy = scanJson;
  xSemaphoreGive(scanLock);
  return copy;
}

void Station::Stop() {
  if (!enabled) {
    return;
  }
  WiFi.disconnect(true);
  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
  }
  connected = false;
  ip[0] = '\0';
}

void Station::Update() {
  if (scanLock == nullptr) {
    scanLock = xSemaphoreCreateMutex();
  }
  if (scanning) {
    int count = WiFi.scanComplete();
    if (count != WIFI_SCAN_RUNNING) {
      scanning = false;
      String json = "[";
      int listed = 0;
      // One line per name (strongest access point), hidden networks left out.
      for (int i = 0; i < count; i++) {
        String name = WiFi.SSID(i);
        bool duplicate = name.length() == 0;
        for (int j = 0; j < i && !duplicate; j++) {
          duplicate = WiFi.SSID(j) == name && WiFi.RSSI(j) >= WiFi.RSSI(i);
        }
        if (duplicate) {
          continue;
        }
        name.replace("\\", "\\\\");
        name.replace("\"", "\\\"");
        json += listed++ ? ",{\"ssid\":\"" : "{\"ssid\":\"";
        json += name + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
                ",\"secure\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
      }
      json += "]";
      WiFi.scanDelete();
      xSemaphoreTake(scanLock, portMAX_DELAY);
      scanJson = json;
      xSemaphoreGive(scanLock);
      Log.printf("WiFi: %d network(s) found\n", listed);
    }
  }

  if (!enabled) {
    return;
  }
  ulong now = millis();
  if (now - lastCheck < CHECK_PERIOD_MS) {
    return;
  }
  lastCheck = now;

  bool isConnected = WiFi.status() == WL_CONNECTED;
  if (isConnected != connected) {
    connected = isConnected;
    if (connected) {
      strlcpy(ip, WiFi.localIP().toString().c_str(), sizeof(ip));
      Log.printf("WiFi: connected to \"%s\", IP %s, channel %d\n", ssid, ip, WiFi.channel());
      if (!mdnsStarted && MDNS.begin(HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        mdnsStarted = true;
        Log.printf("WiFi: http://%s.local\n", HOSTNAME);
      }
    } else {
      ip[0] = '\0';
      Log.printf("WiFi: lost \"%s\", retrying\n", ssid);
    }
  }
  if (connected) {
    rssi = WiFi.RSSI();
  }
  if (!timeLogged && HasTime()) {
    timeLogged = true;
    Log.printf("Time: %s (NTP)\n", LocalTime().c_str());
  }
}

bool Station::HasTime() { return time(nullptr) > VALID_TIME; }

String Station::LocalTime() {
  if (!HasTime()) {
    return String();
  }
  time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  char text[24];
  strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local);
  return String(text);
}
