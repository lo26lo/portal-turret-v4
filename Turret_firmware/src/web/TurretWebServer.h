#pragma once

#include "WiFi.h"
#include "settings/Settings.h"
#include <ESPAsyncWebServer.h>
#include <functional>

class Actions;
class Station;

// Web page and JSON API (plan §10). Every route, the page included, needs
// HTTP Basic credentials: WebUser / WebPassword (factory Admin / 1234), separate
// from the WiFi password ApPassword.
// Callbacks run in the async_tcp task: they only read cached values or queue
// command lines for loop() (Actions::Enqueue).
class TurretWebServer
{
public:
    TurretWebServer();
    void Initialize(Settings &settings, Actions &actions, Station &station);
    // GET /api/screen: the debug screen as JSON (ui/DebugUi). Set before Initialize.
    void SetScreenProvider(std::function<String()> provider) { screenProvider = provider; }
    AsyncWebServer webServer;

    // Login in use (WebUser / WebPassword read at boot, factory values if invalid).
    const char *GetUsername() const { return username; }
    const char *GetPassword() const { return password; }

private:
    Settings *settings;
    Actions *actions;
    Station *station;
    std::function<String()> screenProvider;
    AsyncAuthenticationMiddleware auth;
    // Copies taken at boot: a new login applies at the next boot.
    char username[SETTING_STRING_MAX];
    char password[SETTING_STRING_MAX];
};
