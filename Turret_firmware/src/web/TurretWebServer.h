#pragma once

#include "WiFi.h"
#include "settings/Settings.h"
#include <ESPAsyncWebServer.h>

class Actions;
class Station;

// Web page and JSON API (plan §10). Every route, the page included, needs
// HTTP Basic credentials: user "turret", password ApPassword (D7).
// Callbacks run in the async_tcp task: they only read cached values or queue
// command lines for loop() (Actions::Enqueue).
class TurretWebServer
{
public:
    TurretWebServer();
    void Initialize(Settings &settings, Actions &actions, Station &station);
    AsyncWebServer webServer;

    static const char *USERNAME;
    // Password in use (ApPassword read at boot, default if invalid).
    const char *GetPassword() const { return password; }

private:
    Settings *settings;
    Actions *actions;
    Station *station;
    AsyncAuthenticationMiddleware auth;
    // Copy taken at boot: a new ApPassword applies at the next boot.
    char password[SETTING_STRING_MAX];
};
