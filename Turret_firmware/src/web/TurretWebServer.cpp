#include "TurretWebServer.h"

#include "board/Log.h"
#include "control/Actions.h"
#include "settings/Settings.h"
#include "web/Station.h"
#include "web/page_gz.h" // generated from web/page/index.html by scripts/embed_page.py

const char *TurretWebServer::USERNAME = "turret";

namespace {

const char *DEFAULT_PASSWORD = "stillalive";

const char* TypeName(SettingType type) {
  switch (type) {
    case SettingType::Int:
      return "int";
    case SettingType::Float:
      return "float";
    case SettingType::Bool:
      return "bool";
    case SettingType::Str:
      return "string";
  }
  return "unknown";
}

void AppendEscaped(String& out, const char* text) {
  out += '"';
  for (const char* c = text; *c != '\0'; c++) {
    if (*c == '"' || *c == '\\') {
      out += '\\';
      out += *c;
    } else if (*c == '\n') {
      out += "\\n";
    } else {
      out += *c;
    }
  }
  out += '"';
}

void AppendValue(String& out, SettingType type, const SettingValue& value) {
  char buffer[24];
  switch (type) {
    case SettingType::Int:
      out += String(value.valueInt);
      break;
    case SettingType::Float:
      snprintf(buffer, sizeof(buffer), "%.6g", value.valueFloat);
      out += buffer;
      break;
    case SettingType::Bool:
      out += value.valueBool ? "true" : "false";
      break;
    case SettingType::Str:
      AppendEscaped(out, value.valueString);
      break;
  }
}

}  // namespace

// GET /api/settings: everything the page needs to build its forms (plan §10.2).
// "reboot": true = applied at the next boot (WiFi group). The password is never sent.
String ToJson(Settings* settings) {
  String json = "[";
  for (int i = 0; i < SettingId::COUNT; i++) {
    const SettingsEntry& entry = settings->entries[i];
    bool secret = i == SettingId::ApPassword || i == SettingId::StaPassword;
    if (i > 0) {
      json += ',';
    }
    json += "{\"key\":";
    AppendEscaped(json, entry.key);
    json += ",\"label\":";
    AppendEscaped(json, entry.label);
    json += ",\"group\":";
    AppendEscaped(json, entry.group);
    json += ",\"type\":";
    AppendEscaped(json, TypeName(entry.type));
    json += ",\"value\":";
    if (secret) {
      json += "\"\"";
    } else {
      AppendValue(json, entry.type, entry.value);
    }
    json += ",\"default\":";
    if (secret) {
      json += "\"\"";
    } else {
      AppendValue(json, entry.type, entry.defaultValue);
    }
    if (entry.type == SettingType::Int || entry.type == SettingType::Float) {
      json += ",\"min\":";
      AppendValue(json, entry.type, entry.min);
      json += ",\"max\":";
      AppendValue(json, entry.type, entry.max);
    }
    json += ",\"reboot\":";
    json += strcmp(entry.group, "WiFi") == 0 ? "true" : "false";
    if (secret) {
      json += ",\"secret\":true";
    }
    json += '}';
  }
  json += ']';
  return json;
}

TurretWebServer::TurretWebServer() : webServer(80) {}

void TurretWebServer::Initialize(Settings& settingsIn, Actions& actionsIn, Station& stationIn) {
  settings = &settingsIn;
  actions = &actionsIn;
  station = &stationIn;

  strlcpy(password, settings->GetString(SettingId::ApPassword), sizeof(password));
  if (strlen(password) < 8) {
    strlcpy(password, DEFAULT_PASSWORD, sizeof(password));
  }

  // HTTP Basic on every route (D7). Note: the middleware runs after the body
  // has been received; /update checks the credentials itself (web/Ota.cpp).
  auth.setUsername(USERNAME);
  auth.setPassword(password);
  auth.setRealm("Portal Turret");
  auth.setAuthType(AsyncAuthType::AUTH_BASIC);
  auth.generateHash();
  webServer.addMiddleware(&auth);

  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response =
        request->beginResponse(200, "text/html", PAGE_GZ, PAGE_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  webServer.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "application/json", actions->StatusJson());
  });

  webServer.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "application/json", ToJson(settings));
  });

  // Form-encoded key=value pairs; each becomes a "set" command for loop().
  webServer.on("/api/settings", HTTP_POST, [this](AsyncWebServerRequest* request) {
    int queued = 0;
    int refused = 0;
    for (size_t i = 0; i < request->params(); i++) {
      const AsyncWebParameter* param = request->getParam(i);
      if (!param->isPost()) {
        continue;
      }
      if (actions->Enqueue("set " + param->name() + " " + param->value())) {
        queued++;
      } else {
        refused++;
      }
    }
    request->send(refused ? 503 : 202, "application/json",
                  "{\"queued\":" + String(queued) + ",\"refused\":" + String(refused) + "}");
  });

  webServer.on("/api/settings/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
    String group = request->hasParam("group", true) ? request->getParam("group", true)->value() : String("");
    bool ok = actions->Enqueue("reset-settings " + group);
    request->send(ok ? 202 : 503, "application/json", ok ? "{\"queued\":1}" : "{\"queued\":0}");
  });

  // Same commands as the serial console; the answers go to the log.
  webServer.on("/api/action", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!request->hasParam("cmd", true)) {
      request->send(400, "application/json", "{\"error\":\"cmd missing\"}");
      return;
    }
    bool ok = actions->Enqueue(request->getParam("cmd", true)->value());
    request->send(ok ? 202 : 503, "application/json", ok ? "{\"queued\":1}" : "{\"queued\":0}");
  });

  // First WiFi setup: networks found by "wifi scan".
  webServer.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "application/json",
                  String("{\"scanning\":") + (station->IsScanning() ? "true" : "false") +
                      ",\"networks\":" + station->ScanJson() + "}");
  });

  // Captive portal: the connectivity checks of Android, iOS / macOS and Windows
  // are redirected to the page, so the phone opens it after joining the access
  // point. No credentials here (a 401 would not open the page); the page asks.
  const char* probes[] = {"/generate_204", "/gen_204", "/hotspot-detect.html",
                          "/library/test/success.html", "/connecttest.txt", "/ncsi.txt", "/fwlink"};
  for (const char* probe : probes) {
    webServer.on(probe, HTTP_ANY, [](AsyncWebServerRequest* request) {
      request->redirect("http://192.168.4.1/");
    }).skipServerMiddlewares();
  }

  webServer.on("/api/log", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain; charset=utf-8", Log.Tail());
  });

  webServer.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
    bool ok = actions->Enqueue("reboot");
    request->send(ok ? 202 : 503, "application/json", ok ? "{\"queued\":1}" : "{\"queued\":0}");
  });

  webServer.begin();
}
